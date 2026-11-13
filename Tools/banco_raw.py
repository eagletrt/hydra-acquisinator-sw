#!/usr/bin/env python3
"""Banco dell'acquisinator hydra: tutti i campioni grezzi dell'ADC a 8 kHz, rumore, spettro e registrazione.

Uso:
  python banco_raw.py [--dir sessions/raw] [--scope-ms 200] [--fs 8000] [dbc]

Serve il firmware dell'env "raw" (pio run -e raw -t upload): ADC (SE_CH2) a 8 kHz e ogni campione su
CAN nel frame ACQUISINATOR_HYDRA_ADC_STREAM 0x7B1, tre per frame (~2700 frame/s, ~35% di 1 Mbit/s:
solo al banco, non in macchina). Il frame filtrato 0x7B0 continua ad arrivare, a 250 Hz.

Finestra:
  oscilloscopio  ultimi N ms a scorrimento (slider), senza trigger
  spettro        degli ultimi 2 s (Welch, segmenti da 0.5 s con finestra di Hann), in code di picco;
                 tratteggiate le armoniche dei 50 Hz
  istogramma     dei codici dell'ultimo secondo: forma del rumore, codici mancanti, saturazione
  andamento      media e rumore rms ogni 0.25 s, ultimi 5 minuti: deriva e rumore che cambia
  comandi        nota, "Registra" / "Stop"; "Pausa" congela i grafici (la lettura continua)

Numeri (ultimo secondo, solo campioni arrivati davvero, senza interpolare): media, rumore rms,
picco-picco, bit senza rumore = log2(65536 / p-p), bit efficaci = log2(65536 / (rms * sqrt(12))),
ampiezza a 50 Hz e picco piu' alto dello spettro, campioni/s, frame persi (salti dell'indice), errori
SPI (campioni non validi), e l'ultimo valore filtrato dalla scheda (0x7B0).

Registrazione in --dir: <nome>.csv con un campione per riga (n = indice del tick, i persi contano;
t_s = n / fs) e <nome>_stat.csv con i numeri dell'andamento ogni 0.25 s.
"""

import argparse
import csv
import math
import os
import struct
import threading
import time
from collections import deque
from pathlib import Path

import can
import cantools
import numpy as np

from gs_usb_bus import open_bus

BITRATE = 1_000_000
ADC_VREF_V = 3.29  # ACQUISINATORE_VREF_INT
ADC_FULL_SCALE = 65536
STREAM_LAYOUT = struct.Struct("<HHHBB")  # come il DBC; piu' leggero di cantools a migliaia di frame/s
PER_FRAME = 3
HISTORY_S = 2.0
RESYNC_S = 0.2  # buco oltre il quale l'indice a 8 bit e' ambiguo
STATS_S = 1.0
SPECTRUM_S = 2.0
SEGMENT_S = 0.5
TREND_S = 300.0
TREND_EVERY_S = 0.25
FPS = 10
TEXT_EVERY_S = 0.5
MAINS_HZ = 50.0
MAINS_MARKS = 6
CSV_COLUMNS = ("n", "t_s", "adc_code", "adc_v")
STAT_COLUMNS = ("t_s", "mean_code", "mean_v", "rms_code", "rms_uv", "pp_code", "rate_hz", "lost_frames_last_s",
                "spi_errors_last_s", "board_filtered_code")

TOOLS_DIR = Path(__file__).resolve().parent
DEFAULT_DBC = TOOLS_DIR / "acquisinator_hydra_adc.dbc"


def volts(code: float) -> float:
    return code * ADC_VREF_V / ADC_FULL_SCALE


class Stream:
    """Campioni dei frame 0x7B1 con il loro indice di tick; conteggi tra due letture."""

    def __init__(self, fs: float) -> None:
        self.fs = fs
        self.lock = threading.Lock()
        self.history: deque[tuple[int, int]] = deque(maxlen=int(HISTORY_S * fs))  # (indice, codice)
        self.writer = None
        self.csv_base = 0
        self.n = 0  # indice del primo campione dell'ultimo frame
        self.started = False
        self.last_index = 0
        self.last_time = 0.0
        self.clear_counts()

    def clear_counts(self) -> None:
        self.samples = 0
        self.lost_frames = 0
        self.spi_errors = 0

    def set_writer(self, writer) -> None:
        with self.lock:
            self.writer = writer
            self.csv_base = self.n + PER_FRAME

    def add(self, data: bytes, timestamp: float) -> None:
        *codes, index, valid = STREAM_LAYOUT.unpack(data)
        with self.lock:
            if self.started:
                step = (index - self.last_index) & 0xFF
                # L'indice gira ogni 256 tick (32 ms a 8 kHz). L'ora di arrivo non serve a sciogliere
                # l'ambiguita' (i frame arrivano a raffiche dal transceiver): si usa solo per i buchi
                # lunghi, dove l'indice non dice piu' niente e i frame persi sono una stima.
                if timestamp - self.last_time > RESYNC_S:
                    step = max(PER_FRAME, round((timestamp - self.last_time) * self.fs / PER_FRAME) * PER_FRAME)
                if step != PER_FRAME:
                    self.lost_frames += max(step // PER_FRAME - 1, 1)
                self.n += max(step, 1)
            self.started = True
            self.last_index, self.last_time = index, timestamp
            for k, code in enumerate(codes):
                if not valid & (1 << k):
                    self.spi_errors += 1
                    continue
                self.samples += 1
                self.history.append((self.n + k, code))
                if self.writer is not None:
                    n = self.n + k - self.csv_base
                    self.writer.writerow((n, f"{n / self.fs:.6f}", code, f"{volts(code):.6f}"))


class RawBench:
    """Stato condiviso tra il thread del CAN e la finestra."""

    def __init__(self, bus: can.BusABC, out_dir: str, fs: float, dbc: Path) -> None:
        db = cantools.database.load_file(dbc)
        self.adc_msg = db.get_message_by_name("ACQUISINATOR_HYDRA_ADC")
        self.stream_id = db.get_message_by_name("ACQUISINATOR_HYDRA_ADC_STREAM").frame_id
        self.bus = bus
        self.out_dir = out_dir
        self.stream = Stream(fs)
        self.stop = threading.Event()
        self.lock = threading.Lock()  # tutto quello che segue, lo stream ha il suo
        self.board: dict | None = None
        self.errors = 0
        self.last_frame = 0.0
        self.message = "pronto"
        self.rec_name: str | None = None
        self.rec_start = 0.0
        self.rec_files: list = []
        self.stat_writer = None

    def run(self) -> None:
        while not self.stop.is_set():
            message = self.bus.recv(timeout=0.05)
            if message is None:
                continue
            if message.is_error_frame:
                with self.lock:
                    self.errors += 1
                continue
            self.last_frame = time.monotonic()
            if message.arbitration_id == self.stream_id:
                self.stream.add(message.data, message.timestamp)
            elif message.arbitration_id == self.adc_msg.frame_id:
                with self.lock:
                    self.board = self.adc_msg.decode(message.data)

    def start_recording(self, note: str) -> str:
        parts = ["raw"]
        if note:
            parts.append("".join(c if c.isalnum() or c in "-." else "_" for c in note))
        parts.append(time.strftime("%Y%m%d_%H%M%S"))
        name = os.path.join(self.out_dir, "_".join(parts))
        os.makedirs(self.out_dir, exist_ok=True)
        samples = open(name + ".csv", "w", newline="")
        stats = open(name + "_stat.csv", "w", newline="")
        writer = csv.writer(samples)
        writer.writerow(CSV_COLUMNS)
        stat_writer = csv.writer(stats)
        stat_writer.writerow(STAT_COLUMNS)
        self.stream.set_writer(writer)
        with self.lock:
            self.rec_name, self.rec_start = name, time.monotonic()
            self.rec_files = [samples, stats]
            self.stat_writer = stat_writer
        return name

    def stop_recording(self) -> str | None:
        self.stream.set_writer(None)
        with self.lock:
            name, files = self.rec_name, self.rec_files
            self.rec_name, self.rec_files, self.stat_writer = None, [], None
        for f in files:
            f.close()
        return name

    def flush(self) -> None:
        with self.stream.lock, self.lock:
            for f in self.rec_files:
                f.flush()


def snapshot(stream: Stream, seconds: float) -> tuple[np.ndarray, np.ndarray] | None:
    """Ultimi `seconds`: indici dei tick e codici (solo i campioni arrivati)."""
    count = int(seconds * stream.fs)
    with stream.lock:
        if not stream.history:
            return None
        history = list(stream.history)[-count:]
    n = np.fromiter((h[0] for h in history), dtype=np.int64, count=len(history))
    x = np.fromiter((h[1] for h in history), dtype=float, count=len(history))
    keep = n > n[-1] - count
    return n[keep], x[keep]


def stats(x: np.ndarray) -> dict:
    mean, rms = float(x.mean()), float(x.std())
    pp = float(x.max() - x.min())
    return {
        "mean": mean, "rms": rms, "pp": pp, "low": float(x.min()), "high": float(x.max()),
        "nfb": math.log2(ADC_FULL_SCALE / max(pp, 1.0)),
        "enob": math.log2(ADC_FULL_SCALE / max(rms * math.sqrt(12.0), 1e-9)),
    }


def spectrum(n: np.ndarray, x: np.ndarray, fs: float) -> tuple[np.ndarray, np.ndarray] | None:
    """Welch: segmenti da SEGMENT_S con Hann e meta' sovrapposti, ampiezza di picco in code."""
    if len(n) < 2:
        return None
    full = np.interp(np.arange(n[0], n[-1] + 1), n, x)  # campioni persi ricostruiti
    seg = int(SEGMENT_S * fs)
    if len(full) < seg:
        return None
    window = np.hanning(seg)
    starts = range(0, len(full) - seg + 1, seg // 2)
    power = np.zeros(seg // 2 + 1)
    for s in starts:
        chunk = full[s:s + seg]
        power += np.abs(np.fft.rfft((chunk - chunk.mean()) * window)) ** 2
    amps = 2.0 * np.sqrt(power / len(starts)) / window.sum()
    return np.fft.rfftfreq(seg, 1.0 / fs), amps


def fit_limits(ax, low: float, high: float, min_span: float, log: bool = False) -> bool:
    """Limiti y con isteresi: cambiano solo se i dati escono o occupano meno di un terzo dell'asse."""
    if log:
        low, high = math.log10(low), math.log10(high)
    lo, hi = ax.get_ylim()
    if log:
        lo, hi = math.log10(lo), math.log10(hi)
    span = max(high - low, min_span)
    if lo <= low and high <= hi and span >= (hi - lo) / 3.0:
        return False
    pad = span * 0.25
    lo, hi = low - pad, low + span + pad
    ax.set_ylim(*((10.0 ** lo, 10.0 ** hi) if log else (lo, hi)))
    return True


def window(bench: RawBench, scope_ms: float) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import Button, Slider, TextBox

    fs = bench.stream.fs
    fig = plt.figure(figsize=(14.0, 8.5))
    fig.canvas.manager.set_window_title("Banco acquisinator hydra - ADC grezzo")
    grid = fig.add_gridspec(2, 3, left=0.06, right=0.94, top=0.80, bottom=0.20, hspace=0.38, wspace=0.28,
                            width_ratios=(1.4, 1.4, 1.0))
    ax_info = fig.add_axes((0.0, 0.83, 1.0, 0.17))
    ax_info.set_axis_off()
    ax_scope = fig.add_subplot(grid[0, 0:2])
    ax_hist = fig.add_subplot(grid[0, 2])
    ax_spec = fig.add_subplot(grid[1, 0:2])
    ax_trend = fig.add_subplot(grid[1, 2])

    (scope_line,) = ax_scope.plot([], [], linewidth=0.8)
    ax_scope.set_xlabel("tempo [ms] (0 = campione piu' recente)")
    ax_scope.set_ylabel("ADC [code]")
    ax_scope.grid(True)
    ax_scope.set_ylim(0.0, ADC_FULL_SCALE)

    (spec_line,) = ax_spec.semilogy([], [], linewidth=0.8)
    ax_spec.set_xlabel("frequenza [Hz]")
    ax_spec.set_ylabel("ampiezza di picco [code]")
    ax_spec.grid(True, which="both", alpha=0.3)
    ax_spec.set_ylim(1e-2, 1e3)
    ax_spec.set_xlim(0.0, fs / 2.0)
    for k in range(1, MAINS_MARKS + 1):
        ax_spec.axvline(k * MAINS_HZ, color="tab:red", linestyle=":", linewidth=0.7)

    (hist_line,) = ax_hist.plot([], [], drawstyle="steps-mid", linewidth=0.9)
    ax_hist.set_xlabel("codice - media")
    ax_hist.set_ylabel("campioni (ultimo s)")
    ax_hist.grid(True, alpha=0.4)
    ax_hist.set_ylim(0.0, 10.0)

    (trend_mean,) = ax_trend.plot([], [], color="tab:blue", linewidth=0.9, label="media")
    ax_rms = ax_trend.twinx()
    (trend_rms,) = ax_rms.plot([], [], color="tab:gray", linewidth=0.8, label="rms (destra)")
    ax_trend.set_xlabel("tempo [s]")
    ax_trend.set_ylabel("media [code]")
    ax_trend.ticklabel_format(axis="y", useOffset=False)
    ax_rms.set_ylabel("rms [code]")
    ax_trend.grid(True, alpha=0.4)
    ax_trend.legend(handles=[trend_mean, trend_rms], loc="lower left", bbox_to_anchor=(0.0, 1.0), ncol=2,
                    fontsize=8, frameon=False)
    ax_trend.set_ylim(0.0, 1.0)
    ax_rms.set_ylim(0.0, 1.0)

    title = ax_info.text(0.06, 0.78, "", fontsize=12, weight="bold", transform=ax_info.transAxes)
    numbers = ax_info.text(0.06, 0.62, "", fontsize=10, family="monospace", va="top", transform=ax_info.transAxes)
    status = ax_info.text(0.06, 0.02, "", fontsize=10, family="monospace", transform=ax_info.transAxes)

    lines = [scope_line, spec_line, hist_line, trend_mean, trend_rms]
    texts = [title, numbers, status]
    for artist in lines + texts:
        artist.set_animated(True)

    t0 = time.monotonic()
    trend: deque[tuple[float, float, float]] = deque(maxlen=int(TREND_S / TREND_EVERY_S))
    view = {"ms": scope_ms, "paused": False, "last_trend": 0.0, "last_text": 0.0, "last_count": time.monotonic(),
            "last_flush": 0.0, "rate": 0.0, "lost": 0, "spi": 0}

    note_box = TextBox(fig.add_axes((0.10, 0.06, 0.20, 0.04)), "nota ", initial="")
    rec_btn = Button(fig.add_axes((0.32, 0.06, 0.08, 0.04)), "Registra")
    pause_btn = Button(fig.add_axes((0.41, 0.06, 0.07, 0.04)), "Pausa")
    slider = Slider(fig.add_axes((0.60, 0.07, 0.30, 0.025)), "finestra [ms]", 2.0, HISTORY_S * 1000.0,
                    valinit=scope_ms)

    def on_rec(_) -> None:
        if bench.rec_name is None:
            name = bench.start_recording(note_box.text.strip())
            rec_btn.label.set_text("Stop")
            with bench.lock:
                bench.message = f"registro in {name}.csv"
        else:
            name = bench.stop_recording()
            rec_btn.label.set_text("Registra")
            with bench.lock:
                bench.message = f"salvato {name}.csv e {name}_stat.csv"
        fig.canvas.draw_idle()

    def on_pause(_) -> None:
        view["paused"] = not view["paused"]
        pause_btn.label.set_text("Riprendi" if view["paused"] else "Pausa")
        fig.canvas.draw_idle()

    rec_btn.on_clicked(on_rec)
    pause_btn.on_clicked(on_pause)
    slider.on_changed(lambda value: view.update(ms=value))

    def counters(now: float) -> None:
        """Campioni al secondo, frame persi ed errori SPI dall'ultima volta."""
        elapsed = now - view["last_count"]
        if elapsed < STATS_S:
            return
        with bench.stream.lock:
            view["rate"] = bench.stream.samples / elapsed
            view["lost"] = bench.stream.lost_frames
            view["spi"] = bench.stream.spi_errors
            bench.stream.clear_counts()
        view["last_count"] = now

    anim = None  # FuncAnimation, assegnata sotto: update gira gia' durante la sua creazione

    def update(_):
        now = time.monotonic()
        counters(now)
        if now - view["last_flush"] >= 1.0:
            bench.flush()
            view["last_flush"] = now
        redraw = False

        snap = snapshot(bench.stream, max(SPECTRUM_S, view["ms"] / 1000.0))
        last = None
        if snap is not None:
            n, x = snap
            recent = n > n[-1] - int(STATS_S * fs)
            last = stats(x[recent])

            if now - view["last_trend"] >= TREND_EVERY_S:
                view["last_trend"] = now
                part = x[n > n[-1] - int(TREND_EVERY_S * fs)]
                if len(part):
                    mean, rms = float(part.mean()), float(part.std())
                    trend.append((now - t0, mean, rms))
                    with bench.lock:
                        writer, start, board = bench.stat_writer, bench.rec_start, bench.board
                    if writer is not None:
                        writer.writerow((
                            f"{now - start:.3f}", f"{mean:.2f}", f"{volts(mean):.6f}", f"{rms:.3f}",
                            f"{volts(rms) * 1e6:.2f}", int(part.max() - part.min()), f"{view['rate']:.0f}",
                            view["lost"], view["spi"], "" if board is None else int(board["adc_filtered"]),
                        ))

            if not view["paused"]:
                shown = n > n[-1] - int(view["ms"] / 1000.0 * fs)
                scope_line.set_data((n[shown] - n[-1]) * 1000.0 / fs, x[shown])
                ax_scope.set_xlim(-view["ms"], 0.0)
                redraw |= fit_limits(ax_scope, float(x[shown].min()), float(x[shown].max()), 8.0)

                spec = spectrum(n, x, fs)
                if spec is not None:
                    freqs, amps = spec
                    spec_line.set_data(freqs[1:], amps[1:])
                    positive = amps[1:][amps[1:] > 0]
                    if len(positive):
                        redraw |= fit_limits(ax_spec, float(np.percentile(positive, 1)), float(positive.max()), 1.0,
                                             log=True)
                    k50 = int(round(MAINS_HZ / (freqs[1] - freqs[0])))
                    last["a50"] = float(amps[max(k50 - 1, 1):k50 + 2].max()) if k50 + 2 < len(amps) else math.nan
                    peak = int(np.argmax(amps[2:])) + 2
                    last["peak_f"], last["peak_a"] = float(freqs[peak]), float(amps[peak])

                codes = x[recent].astype(np.int64)
                lo, hi = int(codes.min()), int(codes.max())
                if hi - lo <= 400:
                    counts = np.bincount(codes - lo, minlength=hi - lo + 1)
                    centers = np.arange(lo, hi + 1)
                else:
                    counts, edges = np.histogram(codes, bins=200)
                    centers = (edges[:-1] + edges[1:]) / 2.0
                xs = centers - last["mean"]
                hist_line.set_data(xs, counts)
                half = max(abs(float(xs.min())), abs(float(xs.max())), 2.0)
                shown_half = ax_hist.get_xlim()[1]
                if half > shown_half or half < shown_half / 3.0:
                    ax_hist.set_xlim(-half * 1.3, half * 1.3)
                    redraw = True
                redraw |= fit_limits(ax_hist, 0.0, float(counts.max()), 10.0)

                if trend:
                    tt = np.array([p[0] for p in trend])
                    mm = np.array([p[1] for p in trend])
                    rr = np.array([p[2] for p in trend])
                    trend_mean.set_data(tt, mm)
                    trend_rms.set_data(tt, rr)
                    if tt[-1] > ax_trend.get_xlim()[1]:
                        ax_trend.set_xlim(max(0.0, float(tt[-1]) - TREND_S), float(tt[-1]) + 30.0)
                        redraw = True
                    redraw |= fit_limits(ax_trend, float(mm.min()), float(mm.max()), 4.0)
                    redraw |= fit_limits(ax_rms, 0.0, float(rr.max()), 1.0)

        if now - view["last_text"] >= TEXT_EVERY_S or redraw:
            view["last_text"] = now
            with bench.lock:
                board, errors, message, rec = bench.board, bench.errors, bench.message, bench.rec_name
                bench.errors = 0
            title.set_text(f"Acquisinator hydra - ADC grezzo a {fs:.0f} Hz" + ("   [PAUSA]" if view["paused"] else ""))
            if last is None:
                numbers.set_text("nessun campione 0x7B1: scheda accesa? firmware dell'env raw? transceiver a 1 Mbit/s?")
            else:
                filtered = "" if board is None else f"   filtrato scheda {board['adc_filtered']:.0f}"
                spec_txt = ""
                if "a50" in last:
                    spec_txt = (f"   50 Hz {last['a50']:.2f} code   picco {last['peak_f']:.1f} Hz "
                                f"{last['peak_a']:.2f} code")
                numbers.set_text(
                    f"media {last['mean']:9.1f} code = {volts(last['mean']):.5f} V{filtered}\n"
                    f"rumore {last['rms']:6.2f} code rms = {volts(last['rms']) * 1e6:7.1f} uV   p-p "
                    f"{last['pp']:5.0f} code = {volts(last['pp']) * 1e3:.3f} mV   min {last['low']:.0f} "
                    f"max {last['high']:.0f}\n"
                    f"bit senza rumore {last['nfb']:5.2f}   bit efficaci {last['enob']:5.2f}{spec_txt}\n"
                    f"{view['rate']:6.0f} campioni/s (attesi {fs:.0f})   frame persi {view['lost']}   "
                    f"errori SPI {view['spi']}"
                )
            silent = time.monotonic() - bench.last_frame if bench.last_frame else math.inf
            parts = [message]
            if rec:
                parts.append(f"REC {time.monotonic() - bench.rec_start:5.0f} s")
            if errors:
                parts.append(f"{errors} error frame (ACK, cablaggio, bitrate)")
            if silent > 2.0:
                parts.append("nessun frame dal bus" if math.isinf(silent) else f"nessun frame da {silent:.0f} s")
            status.set_text("   |   ".join(parts))

        if redraw:
            # assi e tick nuovi; le curve (animate) si ridisegnano col blitting. Gli sfondi salvati
            # vanno buttati tutti: con gli assi gemelli uno vecchio coprirebbe quello nuovo
            fig.canvas.draw()
            if anim is not None:
                getattr(anim, "_blit_cache", {}).clear()
        return lines + texts

    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=1000 // FPS, blit=True, cache_frame_data=False)
    plt.show()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dbc", nargs="?", type=Path, default=DEFAULT_DBC, help="dbc dell'acquisinator hydra")
    parser.add_argument("--dir", default=str(TOOLS_DIR / "sessions" / "raw"), help="cartella delle registrazioni")
    parser.add_argument("--scope-ms", type=float, default=200.0, help="finestra iniziale dell'oscilloscopio")
    parser.add_argument("--fs", type=float, default=8000.0, help="frequenza di campionamento del firmware [Hz]")
    args = parser.parse_args()

    bus = open_bus(BITRATE)
    bench = RawBench(bus, args.dir, args.fs, args.dbc)
    reader = threading.Thread(target=bench.run, daemon=True)
    reader.start()
    try:
        window(bench, args.scope_ms)
    except KeyboardInterrupt:
        pass
    finally:
        bench.stop.set()
        reader.join(timeout=1.0)
        bench.stop_recording()
        bus.shutdown()


if __name__ == "__main__":
    main()
