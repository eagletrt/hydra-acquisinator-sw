#!/usr/bin/env python3
"""Registra in CSV i risultati del lock-in dell'acquisinator hydra, con un'anteprima live.

Uso (dalla cartella Tools, le dipendenze sono in pyproject.toml e le installa uv):
  uv run lockin_logger.py                                # grafico live, si registra col pulsante
  uv run lockin_logger.py --name statica --duration 30   # ogni registrazione si ferma dopo 30 s
  uv run lockin_logger.py --name statica --no-plot       # senza finestra: registra subito

Nella finestra il grafico e' sempre live; il pulsante Registra apre un nuovo CSV con il nome scritto nel
campo accanto, Stop lo chiude. Si possono fare quante registrazioni si vuole senza chiudere la finestra.

Il firmware manda ogni risultato come due frame (vedi Core/Lib/acquisinator-libcan/dbc):
  ACQUISINATOR_LOCKIN        0x13  byte 0-3 I, 4-7 Q (int32 little endian, x 0.0001 = conteggi ADC di picco)
  ACQUISINATOR_LOCKIN_STATUS 0x14  byte 0-1 dc, 2-3 min, 4-5 max (uint16, conteggi ADC),
                                   bit 48 errore SPI, bit 49-56 contatore dei risultati
0x13 arriva sempre prima del suo 0x14: la riga del CSV si scrive all'arrivo di 0x14.

CSV (<dir>/lockin_<nome>_<data>.csv), un risultato per riga:
  x_mv       valore filtrato del lock-in: componente in fase dell'uscita dell'amplificatore,
             con segno, in mV di picco (e' quello del grafico)
  seq        contatore del firmware (0..255)
  t_s        tempo di arrivo dall'inizio della registrazione
  i, q       componenti in fase e in quadratura [conteggi ADC, picco]
  amp        sqrt(i^2 + q^2) [conteggi]           amp_v   amp in volt con --vref
  phase_deg  atan2(q, i) [gradi]
  dc, min, max  media, minimo e massimo della finestra [conteggi]
  spi_error  1 se una lettura SPI e' fallita (la finestra e' ripartita)
  clip       1 se min o max toccano i bordi dell'ADC (I e Q non affidabili)

Su macOS serve il transceiver gs_usb (candleLight) e libusb di sistema: brew install libusb
"""

import argparse
import csv
import math
import struct
import threading
import time
from collections import deque
from pathlib import Path

import can

from gs_usb_bus import open_bus

BITRATE = 1_000_000
LOCKIN_ID = 0x13  # CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN
STATUS_ID = 0x14  # CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS
IQ_SCALE = 0.0001
SEQ_MODULO = 256
ADC_FULL_SCALE = 65536
CLIP_MARGIN = 64  # conteggi dai bordi dell'ADC oltre i quali si considera saturo
CSV_COLUMNS = ("x_mv", "seq", "t_s", "i", "q", "amp", "amp_v", "phase_deg", "dc", "min", "max", "spi_error", "clip")

PREVIEW_S = 10.0
STATS_S = 1.0
FPS = 10
FLUSH_EVERY_S = 1.0

TOOLS_DIR = Path(__file__).resolve().parent


def decode_lockin(data: bytes | bytearray) -> tuple[float, float]:
    i_raw, q_raw = struct.unpack_from("<ii", data)
    return i_raw * IQ_SCALE, q_raw * IQ_SCALE


def decode_status(data: bytes | bytearray) -> tuple[int, int, int, bool, int]:
    dc, low, high = struct.unpack_from("<HHH", data)
    spi_error = bool(data[6] & 0x01)
    seq = ((data[6] >> 1) | (data[7] << 7)) & 0xFF
    return dc, low, high, spi_error, seq


class LockinStream:
    """Accoppia 0x13 e 0x14, conta i risultati persi e scrive il CSV."""

    def __init__(self, vref: float, phase_deg: float, start: float) -> None:
        self.vref = vref
        self.cal_cos = math.cos(math.radians(phase_deg))
        self.cal_sin = math.sin(math.radians(phase_deg))
        self.start = start
        self.csv_file = None
        self.writer = None
        self.path: Path | None = None
        self.rec_start = 0.0
        self.rec_rows = 0
        self.lock = threading.Lock()
        self.history: deque[tuple[float, float]] = deque()  # (t, x_mv)
        self.pending_iq: tuple[float, float] | None = None
        self.last_seq: int | None = None
        self.results = 0
        self.lost = 0
        self.unpaired = 0
        self.spi_errors = 0
        self.clips = 0
        self.last_row: dict | None = None

    def add(self, arbitration_id: int, data: bytes | bytearray) -> None:
        if len(data) < 8:
            return
        with self.lock:
            if arbitration_id == LOCKIN_ID:
                if self.pending_iq is not None:
                    self.unpaired += 1  # 0x14 del risultato precedente perso
                self.pending_iq = decode_lockin(data)
                return
            if self.pending_iq is None:
                self.unpaired += 1  # 0x13 di questo risultato perso
                return
            i, q = self.pending_iq
            self.pending_iq = None
            dc, low, high, spi_error, seq = decode_status(data)

            if self.last_seq is not None:
                self.lost += (seq - self.last_seq - 1) % SEQ_MODULO
            self.last_seq = seq

            t = time.monotonic() - self.start
            amp = math.hypot(i, q)
            # componente in fase con segno (ruotata di --phase-deg), in mV
            x_mv = (i * self.cal_cos + q * self.cal_sin) * self.vref / ADC_FULL_SCALE * 1000.0
            phase = math.degrees(math.atan2(q, i))
            clip = low <= CLIP_MARGIN or high >= ADC_FULL_SCALE - 1 - CLIP_MARGIN
            self.results += 1
            self.spi_errors += spi_error
            self.clips += clip
            self.history.append((t, x_mv))
            while self.history and self.history[0][0] < t - PREVIEW_S:
                self.history.popleft()
            self.last_row = {"x_mv": x_mv, "phase": phase, "dc": dc, "min": low, "max": high}
            if self.writer is None:
                return
            self.rec_rows += 1
            self.writer.writerow((f"{x_mv:.5f}", seq, f"{t - self.rec_start:.4f}", f"{i:.4f}", f"{q:.4f}", f"{amp:.4f}",
                                  f"{amp * self.vref / ADC_FULL_SCALE:.7f}", f"{phase:.3f}",
                                  dc, low, high, int(spi_error), int(clip)))

    def start_recording(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        csv_file = open(path, "w", newline="", encoding="utf-8")
        writer = csv.writer(csv_file)
        writer.writerow(CSV_COLUMNS)
        with self.lock:
            self.csv_file, self.writer, self.path = csv_file, writer, path
            self.rec_start = time.monotonic() - self.start
            self.rec_rows = 0

    def stop_recording(self) -> tuple[Path, int] | None:
        with self.lock:
            if self.csv_file is None or self.path is None:
                return None
            csv_file, path, rows = self.csv_file, self.path, self.rec_rows
            self.csv_file = self.writer = None
        csv_file.close()
        return path, rows

    def recording(self) -> tuple[Path, float, int] | None:
        """(file, secondi registrati, righe) se si sta registrando."""
        with self.lock:
            if self.csv_file is None or self.path is None:
                return None
            return self.path, time.monotonic() - self.start - self.rec_start, self.rec_rows

    def flush(self) -> None:
        with self.lock:
            if self.csv_file is not None:
                self.csv_file.flush()

    def counters(self) -> tuple[int, int, int, int, int]:
        with self.lock:
            return self.results, self.lost, self.unpaired, self.spi_errors, self.clips

    def last_seconds(self, seconds: float) -> list[tuple[float, float]]:
        with self.lock:
            if not self.history:
                return []
            first = self.history[-1][0] - seconds
            return [h for h in self.history if h[0] > first]


def read_loop(bus: can.BusABC, stream: LockinStream, stop: threading.Event) -> None:
    last_flush = time.monotonic()
    while not stop.is_set():
        message = bus.recv(timeout=0.05)
        if message is not None and not message.is_error_frame and message.arbitration_id in (LOCKIN_ID, STATUS_ID):
            stream.add(message.arbitration_id, message.data)
        if time.monotonic() - last_flush >= FLUSH_EVERY_S:
            stream.flush()
            last_flush = time.monotonic()


def summary(stream: LockinStream) -> str:
    recent = stream.last_seconds(STATS_S)
    if not recent:
        return "nessun risultato: scheda accesa? transceiver a 1 Mbit/s? un altro nodo da' l'ACK?"
    values = [x for _, x in recent]
    mean = sum(values) / len(values)
    rms = math.sqrt(sum((x - mean) ** 2 for x in values) / len(values))
    row = stream.last_row or {}
    return (f"lock-in {mean:9.3f} mV   rumore {rms * 1000:7.1f} uV rms   fase {row.get('phase', 0):7.2f} deg   "
            f"ADC dc {row.get('dc', 0):5d}  min {row.get('min', 0):5d}  max {row.get('max', 0):5d}")


def recording_path(directory: Path, name: str) -> Path:
    safe_name = "".join(c if c.isalnum() or c in "-." else "_" for c in name.strip())
    parts = ["lockin"] + ([safe_name] if safe_name else []) + [time.strftime("%Y%m%d_%H%M%S")]
    path = directory / ("_".join(parts) + ".csv")
    suffix = 2
    while path.exists():  # due registrazioni nello stesso secondo
        path = directory / ("_".join(parts) + f"_{suffix}.csv")
        suffix += 1
    return path


def preview(stream: LockinStream, directory: Path, name: str, duration: float | None, stop: threading.Event) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import Button, TextBox

    fig, ax = plt.subplots(figsize=(11, 6))
    if fig.canvas.manager is not None:
        fig.canvas.manager.set_window_title("lockin_logger")
    (line,) = ax.plot([], [], lw=1.0)
    ax.set_title("uscita filtrata del lock-in")
    ax.set_ylabel("lock-in [mV]")
    ax.set_xlabel("tempo [s]")
    ax.grid(alpha=0.3)
    fig.subplots_adjust(bottom=0.27, top=0.95)
    info = fig.text(0.01, 0.01, "", family="monospace", fontsize=9)

    name_box = TextBox(fig.add_axes((0.12, 0.165, 0.45, 0.04)), "nome ", initial=name)
    rec_button = Button(fig.add_axes((0.60, 0.165, 0.14, 0.04)), "Registra", color="#d8f0d8", hovercolor="#b8e0b8")
    last_saved = {"text": ""}

    def stop_recording() -> None:
        done = stream.stop_recording()
        if done is not None:
            path, rows = done
            last_saved["text"] = f"salvato {path.name} ({rows} risultati)"
            print(last_saved["text"])
        rec_button.label.set_text("Registra")
        rec_button.color, rec_button.hovercolor = "#d8f0d8", "#b8e0b8"

    def toggle(_event=None) -> None:
        if stream.recording() is not None:
            stop_recording()
        else:
            path = recording_path(directory, name_box.text)
            stream.start_recording(path)
            print(f"registrazione in {path}")
            rec_button.label.set_text("Stop")
            rec_button.color, rec_button.hovercolor = "#f4c7c7", "#eaa5a5"
        fig.canvas.draw_idle()

    rec_button.on_clicked(toggle)
    rate = {"t": time.monotonic(), "results": 0, "value": 0.0}


    def update(_):
        if stop.is_set():
            plt.close(fig)
            return (line,)
        rec = stream.recording()
        if rec is not None and duration is not None and rec[1] >= duration:
            stop_recording()
            rec = None

        points = stream.last_seconds(PREVIEW_S)
        if points:
            t = [p[0] for p in points]
            values = [p[1] for p in points]
            line.set_data(t, values)
            ax.set_xlim(t[0], max(t[-1], t[0] + 1e-3))
            low, high = min(values), max(values)
            pad = max((high - low) * 0.2, 0.01)  # almeno +-10 uV di scala
            ax.set_ylim(low - pad, high + pad)

        now = time.monotonic()
        results, lost, unpaired, spi_errors, clips = stream.counters()
        if now - rate["t"] >= 1.0:
            rate["value"] = (results - rate["results"]) / (now - rate["t"])
            rate["t"], rate["results"] = now, results
        if rec is not None:
            status = f"● REC {rec[1]:6.1f} s  {rec[2]} risultati -> {rec[0].name}"
        else:
            status = f"live (non registra)   {last_saved['text']}"
        info.set_text(
            f"{status}\n"
            f"{rate['value']:5.0f} risultati/s   persi {lost}   frame spaiati {unpaired}   "
            f"errori SPI {spi_errors}   saturazioni {clips}\n{summary(stream)}"
        )
        return (line,)

    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=1000 // FPS, cache_frame_data=False)
    try:
        plt.show()
    finally:
        stop_recording()
    del anim


def main() -> None:
    parser = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    parser.add_argument("--name", default="", help="nome iniziale delle registrazioni, finisce nel nome del file")
    parser.add_argument("--dir", type=Path, default=TOOLS_DIR / "sessions" / "lockin", help="cartella dei CSV")
    parser.add_argument("--vref", type=float, default=3.0, help="riferimento dell'ADC (AD1583, 3.000 V) per la colonna amp_v [V]")
    parser.add_argument("--phase-deg", type=float, default=0.0,
                        help="sfasamento della catena analogica: ruota I/Q prima di prendere la componente in fase")
    parser.add_argument("--duration", type=float, default=None, help="ferma ogni registrazione dopo questi secondi")
    parser.add_argument("--no-plot", action="store_true", help="senza finestra: registra subito fino a Ctrl+C")
    args = parser.parse_args()

    bus = open_bus(BITRATE)
    # solo 0x13 e 0x14: lo stream grezzo 0x12 (~2700 frame/s) non serve qui
    bus.set_filters([{"can_id": frame_id, "can_mask": 0x7FF, "extended": False} for frame_id in (LOCKIN_ID, STATUS_ID)])
    stop = threading.Event()
    start = time.monotonic()

    stream = LockinStream(args.vref, args.phase_deg, start)
    reader = threading.Thread(target=read_loop, args=(bus, stream, stop), daemon=True)
    reader.start()
    try:
        if args.no_plot:
            path = recording_path(args.dir, args.name)
            stream.start_recording(path)
            print(f"registrazione in {path} (Ctrl+C per fermare)")
            while args.duration is None or time.monotonic() - start < args.duration:
                time.sleep(1.0)
                results, lost, _, spi_errors, clips = stream.counters()
                print(f"\r{time.monotonic() - start:6.0f} s   risultati {results}   persi {lost}   "
                      f"SPI {spi_errors}   sat {clips}   {summary(stream)}", end="", flush=True)
        else:
            print("finestra aperta: Registra / Stop per salvare, chiudi la finestra per uscire")
            preview(stream, args.dir, args.name, args.duration, stop)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        reader.join(timeout=1.0)
        bus.shutdown()
        done = stream.stop_recording()
        if done is not None:
            print(f"\nsalvato {done[0]} ({done[1]} risultati)")


if __name__ == "__main__":
    main()
