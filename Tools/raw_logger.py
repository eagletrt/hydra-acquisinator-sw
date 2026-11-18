#!/usr/bin/env python3
"""Registra in CSV tutti i campioni grezzi dell'ADC dell'acquisinator hydra, con un'anteprima live.

Uso (dalla cartella Tools, le dipendenze sono in pyproject.toml e le installa uv):
  uv run raw_logger.py --name prova_carico            # registra + anteprima
  uv run raw_logger.py --name rumore --no-plot        # solo registrazione
  uv run raw_logger.py --name statica --duration 30   # si ferma da solo dopo 30 s

La registrazione parte subito e si ferma con Ctrl+C, chiudendo la finestra o allo scadere di --duration.

Il firmware manda il frame ACQUISINATOR_RAW (0x12, vedi Core/Lib/acquisinator-libcan/dbc) con tre
campioni consecutivi e un contatore di frame a 8 bit:
  byte 0-1 campione piu' vecchio, 2-3 centrale, 4-5 piu' recente (uint16 little endian), 6 contatore.
A 8 kHz sono ~2700 frame/s: si decodifica con struct, cantools sarebbe troppo lento.

CSV (<dir>/raw_<nome>_<data>.csv), un campione per riga:
  n        indice del campione dall'inizio della registrazione; i campioni persi lasciano un buco
  t_s      n / fs
  adc_code codice grezzo 0..65535
  adc_v    codice in volt con --vref

L'anteprima mostra gli ultimi secondi decimati (non serve vedere tutti gli 8 kHz) e i numeri dell'ultimo
secondo: campioni/s, frame persi, media e rumore. Nel CSV finisce comunque ogni campione arrivato.

Su macOS serve il transceiver gs_usb (candleLight) e libusb di sistema: brew install libusb
"""

import argparse
import csv
import math
import threading
import time
from collections import deque
from pathlib import Path

import can

from gs_usb_bus import open_bus

BITRATE = 1_000_000
RAW_FRAME_ID = 0x12  # CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW
SAMPLES_PER_FRAME = 3  # RAW_STREAM_SAMPLES_PER_FRAME nel firmware
SEQ_MODULO = 256
ADC_FULL_SCALE = 65536
CSV_COLUMNS = ("n", "t_s", "adc_code", "adc_v")

PREVIEW_S = 2.0  # secondi mostrati nell'anteprima
PREVIEW_POINTS = 2000  # punti disegnati al massimo, il resto e' decimato
STATS_S = 1.0
FPS = 10
FLUSH_EVERY_S = 1.0

TOOLS_DIR = Path(__file__).resolve().parent


class RawStream:
    """Decodifica i frame 0x12, ricostruisce l'indice dei campioni e scrive il CSV."""

    def __init__(self, fs: float, vref: float, writer) -> None:
        self.fs = fs
        self.vref = vref
        self.writer = writer
        self.lock = threading.Lock()
        self.history: deque[tuple[int, int]] = deque(maxlen=int(PREVIEW_S * fs))  # (n, codice)
        # Il contatore a 8 bit gira ogni 256 frame: oltre questo buco non si sa quanti frame mancano
        self.wrap_s = SEQ_MODULO * SAMPLES_PER_FRAME / fs
        self.n = 0  # indice del primo campione del frame corrente
        self.started = False
        self.last_seq = 0
        self.last_time = 0.0
        self.frames = 0
        self.lost_frames = 0
        self.samples = 0

    def add(self, data: bytes | bytearray, timestamp: float) -> None:
        if len(data) < 7:
            return
        codes = (data[0] | data[1] << 8, data[2] | data[3] << 8, data[4] | data[5] << 8)
        seq = data[6]
        with self.lock:
            if self.started:
                step = (seq - self.last_seq) % SEQ_MODULO
                if timestamp - self.last_time > 0.75 * self.wrap_s:
                    # buco lungo: il contatore e' ambiguo, si stima dall'ora di arrivo
                    step = max(1, round((timestamp - self.last_time) * self.fs / SAMPLES_PER_FRAME))
                if step == 0:
                    return  # frame duplicato
                self.lost_frames += step - 1
                self.n += step * SAMPLES_PER_FRAME
            self.started = True
            self.last_seq, self.last_time = seq, timestamp
            self.frames += 1
            for k, code in enumerate(codes):
                n = self.n + k
                self.samples += 1
                self.history.append((n, code))
                self.writer.writerow((n, f"{n / self.fs:.6f}", code, f"{code * self.vref / ADC_FULL_SCALE:.6f}"))

    def counters(self) -> tuple[int, int, int]:
        with self.lock:
            return self.frames, self.lost_frames, self.samples

    def last_seconds(self, seconds: float) -> list[tuple[int, int]]:
        with self.lock:
            if not self.history:
                return []
            first = self.history[-1][0] - int(seconds * self.fs)
            return [h for h in self.history if h[0] > first]


def read_loop(bus: can.BusABC, stream: RawStream, csv_file, stop: threading.Event) -> None:
    last_flush = time.monotonic()
    while not stop.is_set():
        message = bus.recv(timeout=0.05)
        if message is not None and not message.is_error_frame and message.arbitration_id == RAW_FRAME_ID:
            stream.add(message.data, message.timestamp)
        if time.monotonic() - last_flush >= FLUSH_EVERY_S:
            with stream.lock:
                csv_file.flush()
            last_flush = time.monotonic()


def summary(stream: RawStream) -> str:
    recent = [code for _, code in stream.last_seconds(STATS_S)]
    if not recent:
        return "nessun campione: scheda accesa? transceiver a 1 Mbit/s? un altro nodo da' l'ACK?"
    mean = sum(recent) / len(recent)
    rms = math.sqrt(sum((c - mean) ** 2 for c in recent) / len(recent))
    return f"media {mean:9.1f} code   rumore {rms:6.2f} code rms   p-p {max(recent) - min(recent):5d} code"


def preview(stream: RawStream, path: Path, start: float, duration: float | None, stop: threading.Event) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    fig, ax = plt.subplots(figsize=(11, 5))
    if fig.canvas.manager is not None:
        fig.canvas.manager.set_window_title(f"raw_logger - {path.name}")
    (line,) = ax.plot([], [], lw=0.8)
    ax.set_xlabel("tempo [s]")
    ax.set_ylabel("codice ADC")
    ax.grid(alpha=0.3)
    info = fig.text(0.01, 0.01, "", family="monospace", fontsize=9)
    fig.subplots_adjust(bottom=0.2)
    rate = {"t": time.monotonic(), "samples": 0, "value": 0.0}

    def update(_):
        if stop.is_set() or (duration is not None and time.monotonic() - start >= duration):
            plt.close(fig)
            return (line,)
        points = stream.last_seconds(PREVIEW_S)
        if points:
            step = max(1, len(points) // PREVIEW_POINTS)
            points = points[::step]
            t = [n / stream.fs for n, _ in points]
            y = [c for _, c in points]
            line.set_data(t, y)
            ax.set_xlim(t[0], max(t[-1], t[0] + 1e-3))
            low, high = min(y), max(y)
            pad = max((high - low) * 0.2, 5)
            ax.set_ylim(low - pad, high + pad)

        now = time.monotonic()
        _, lost, samples = stream.counters()
        if now - rate["t"] >= 1.0:
            rate["value"] = (samples - rate["samples"]) / (now - rate["t"])
            rate["t"], rate["samples"] = now, samples
        info.set_text(
            f"REC {now - start:6.1f} s -> {path}\n"
            f"{rate['value']:6.0f} campioni/s (attesi {stream.fs:.0f})   frame persi {lost}   "
            f"campioni salvati {samples}\n{summary(stream)}"
        )
        return (line,)

    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=1000 // FPS, cache_frame_data=False)
    plt.show()
    del anim


def main() -> None:
    parser = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    parser.add_argument("--name", default="", help="nome della registrazione, finisce nel nome del file")
    parser.add_argument("--dir", type=Path, default=TOOLS_DIR / "sessions" / "raw", help="cartella dei CSV")
    parser.add_argument("--fs", type=float, default=8000.0,
                        help="campioni/s nello stream: RAW_STREAM_SAMPLE_RATE_HZ / RAW_STREAM_CAN_DECIMATION")
    parser.add_argument("--vref", type=float, default=3.0, help="riferimento dell'ADC (AD1583, 3.000 V) per la colonna adc_v [V]")
    parser.add_argument("--duration", type=float, default=None, help="ferma la registrazione dopo questi secondi")
    parser.add_argument("--no-plot", action="store_true", help="solo registrazione, senza finestra")
    args = parser.parse_args()

    safe_name = "".join(c if c.isalnum() or c in "-." else "_" for c in args.name)
    parts = ["raw"] + ([safe_name] if safe_name else []) + [time.strftime("%Y%m%d_%H%M%S")]
    args.dir.mkdir(parents=True, exist_ok=True)
    path = args.dir / ("_".join(parts) + ".csv")

    bus = open_bus(BITRATE)
    bus.set_filters([{"can_id": RAW_FRAME_ID, "can_mask": 0x7FF, "extended": False}])
    stop = threading.Event()
    start = time.monotonic()

    with open(path, "w", newline="", encoding="utf-8") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(CSV_COLUMNS)
        stream = RawStream(args.fs, args.vref, writer)
        reader = threading.Thread(target=read_loop, args=(bus, stream, csv_file, stop), daemon=True)
        reader.start()
        print(f"registrazione in {path} (Ctrl+C per fermare)")
        try:
            if args.no_plot:
                while args.duration is None or time.monotonic() - start < args.duration:
                    time.sleep(1.0)
                    frames, lost, samples = stream.counters()
                    print(f"\r{time.monotonic() - start:6.0f} s   campioni {samples}   frame persi {lost}   "
                          f"{summary(stream)}", end="", flush=True)
            else:
                preview(stream, path, start, args.duration, stop)
        except KeyboardInterrupt:
            pass
        finally:
            stop.set()
            reader.join(timeout=1.0)
            bus.shutdown()

    frames, lost, samples = stream.counters()
    print(f"\nsalvati {samples} campioni ({frames} frame, {lost} frame persi) in {path}")


if __name__ == "__main__":
    main()
