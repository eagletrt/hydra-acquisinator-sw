#!/usr/bin/env python3
"""Legge ACQUISINATOR_HYDRA_ADC dall'acquisinator hydra (gs_usb), registra la sessione e calibra l'ADC.

Uso: python read_link_deformation.py [--no-plot] [--window S] [--out NOME] [--no-save] [dbc]

Grafico live con due pannelli:
  - sopra: codice ADC grezzo (adc_raw, sottile) e media del firmware su 20 ms (adc_filtered)
  - sotto: tensione all'ingresso dell'ADC, calibrata se c'e' una calibrazione, altrimenti
    quella nominale del firmware (3.29 V * codice / 65536)

Forza: scrivere i N applicati nella casella "forza [N]" e premere Invio. Il valore finisce in
tutte le righe successive del CSV, nel log e come linea verticale nel grafico.

Calibrazione ADC: misurare con il multimetro la tensione sul pin d'ingresso dell'ADC (uscita
dell'amplificatore), scriverla in volt nella casella "V multimetro" e premere Invio: il punto
viene preso con la media di adc_filtered dell'ultimo CAL_SECONDS. Servono almeno due punti a
tensioni diverse (es. puntone scarico e carico, o regolando l'offset): con due o piu' punti si
ricava V = gain * codice + offset ai minimi quadrati. La calibrazione viene salvata in
adc_calibration.json accanto allo script e ricaricata agli avvii successivi; "Reset cal" la
cancella.

Sessione (default, --no-save per non salvare) in Tools/sessions/:
  - sessione_<data>_<ora>.csv: una riga per frame, con ora del PC, forza, codici, tensioni e
    deformazione
  - sessione_<data>_<ora>.log: eventi con timestamp (avvio, forza, punti di calibrazione, stop)
Ogni riga viene scritta subito su disco: se lo script si chiude male i dati restano.

Con --no-plot i comandi si scrivono nel terminale: un numero imposta la forza in N,
"cal 1.234" aggiunge un punto di calibrazione a 1.234 V, "cal reset" la cancella.
"""

import argparse
import csv
import json
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path

import can
import cantools

from gs_usb_bus import open_bus
from read_both_acquisinators import hydra_deformation

MESSAGE_NAME = "ACQUISINATOR_HYDRA_ADC"
BITRATE = 1_000_000
PLOT_FPS = 20
SILENCE_WARNING_S = 2.0  # ogni quanto avvisare se dal bus non arriva niente

ADC_FULL_SCALE = 65536
ADC_VREF_NOMINAL_V = 3.29  # ACQUISINATORE_VREF_INT del firmware
CAL_SECONDS = 1.0  # finestra mediata per un punto di calibrazione
CAL_MIN_CODE_SPAN = 100  # codici: punti piu' vicini di cosi' non danno un guadagno affidabile
CAL_MAX_SPREAD = 50  # codici: oltre questa escursione nell'ultimo CAL_SECONDS il segnale non e' fermo

TOOLS_DIR = Path(__file__).resolve().parent
DEFAULT_DBC = TOOLS_DIR / "acquisinator_hydra_adc.dbc"
CALIBRATION_FILE = TOOLS_DIR / "adc_calibration.json"
SESSIONS_DIR = TOOLS_DIR / "sessions"

CSV_COLUMNS = (
    "wall_time",  # ora del PC, ISO 8601 al millisecondo
    "t_s",  # secondi dal primo frame
    "force_N",
    "adc_raw",  # ultimo campione dell'LTC1865
    "adc_filtered",  # media del firmware sugli ultimi 20 ms (un periodo di rete)
    "voltage_fw_mV",  # tensione mandata dal firmware (nominale, dalla media non arrotondata)
    "voltage_cal_mV",  # tensione calibrata da adc_filtered (vuota senza calibrazione)
    "deformation_ustrain",  # dalla tensione calibrata se c'e', altrimenti da quella del firmware
)


def now_iso() -> str:
    return datetime.now().isoformat(timespec="milliseconds")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dbc", nargs="?", type=Path, default=DEFAULT_DBC, help="path al dbc dell'acquisinator")
    parser.add_argument("--no-plot", action="store_true", help="stampa soltanto, comandi da terminale")
    parser.add_argument(
        "--window",
        type=float,
        default=60.0,
        metavar="S",
        help="secondi mostrati nel grafico, 0 = tutta la storia (default: 60)",
    )
    parser.add_argument(
        "--out",
        type=str,
        default=None,
        metavar="NOME",
        help="nome della sessione, senza estensione (default: sessione_<data>_<ora>)",
    )
    parser.add_argument("--no-save", action="store_true", help="non salvare la sessione")
    return parser.parse_args()


class Calibration:
    """V = gain * codice + offset, dai punti (codice, V misurata) ai minimi quadrati."""

    def __init__(self, path: Path) -> None:
        self.path = path
        self.points: list[tuple[float, float]] = []
        self.gain: float | None = None
        self.offset: float | None = None
        if path.is_file():
            self.points = [tuple(p) for p in json.loads(path.read_text())["points"]]
            self._fit()

    def _fit(self) -> None:
        self.gain = self.offset = None
        if len(self.points) < 2:
            return
        codes = [c for c, _ in self.points]
        if max(codes) - min(codes) < CAL_MIN_CODE_SPAN:
            return
        n = len(self.points)
        mean_c = sum(codes) / n
        mean_v = sum(v for _, v in self.points) / n
        cov = sum((c - mean_c) * (v - mean_v) for c, v in self.points)
        var = sum((c - mean_c) ** 2 for c in codes)
        self.gain = cov / var
        self.offset = mean_v - self.gain * mean_c

    def add(self, code: float, volts: float) -> None:
        self.points.append((code, volts))
        self._fit()
        self._save()

    def reset(self) -> None:
        self.points = []
        self._fit()
        if self.path.is_file():
            self.path.unlink()

    def _save(self) -> None:
        self.path.write_text(
            json.dumps(
                {"updated": now_iso(), "gain_V_per_code": self.gain, "offset_V": self.offset, "points": self.points},
                indent=2,
            )
        )

    def volts(self, code: float) -> float | None:
        if self.gain is None:
            return None
        return self.gain * code + self.offset

    def describe(self) -> str:
        if self.gain is not None:
            vref = self.gain * ADC_FULL_SCALE
            return f"ADC calibrato: {self.gain * 1e6:.3f} µV/codice (Vref eq. {vref:.4f} V), offset {self.offset * 1000:+.2f} mV, {len(self.points)} punti"
        if self.points:
            return f"ADC non calibrato: {len(self.points)} punto/i, serve un altro punto ad almeno {CAL_MIN_CODE_SPAN} codici di distanza"
        return "ADC non calibrato (tensione nominale del firmware)"


class Session:
    """Stato condiviso tra lettura CAN e interfaccia: forza, calibrazione, file CSV e log."""

    def __init__(self, name: str | None, save: bool) -> None:
        self.lock = threading.Lock()
        self.force_n: float | None = None
        self.calibration = Calibration(CALIBRATION_FILE)
        self.recent: deque = deque()  # (monotonic, adc_filtered) dell'ultimo CAL_SECONDS
        self.t0: float | None = None
        self.csv_path = self.log_path = None
        self._csv_file = self._writer = self._log = None
        if save:
            SESSIONS_DIR.mkdir(parents=True, exist_ok=True)
            stem = name or f"sessione_{datetime.now():%Y%m%d_%H%M%S}"
            # mai sovrascrivere una sessione esistente: stesso nome -> nome_2, nome_3, ...
            base, n = stem, 1
            while (SESSIONS_DIR / f"{stem}.csv").exists() or (SESSIONS_DIR / f"{stem}.log").exists():
                n += 1
                stem = f"{base}_{n}"
            self.csv_path = SESSIONS_DIR / f"{stem}.csv"
            self.log_path = SESSIONS_DIR / f"{stem}.log"
            self._csv_file = open(self.csv_path, "w", newline="", encoding="utf-8")
            self._writer = csv.DictWriter(self._csv_file, fieldnames=CSV_COLUMNS)
            self._writer.writeheader()
            self._log = open(self.log_path, "a", encoding="utf-8")
            print(f"Sessione: {self.csv_path}\n          {self.log_path}")
        self.event(f"avvio; {self.calibration.describe()}")

    def event(self, text: str) -> None:
        line = f"{now_iso()}  {text}"
        print(line)
        if self._log is not None:
            self._log.write(line + "\n")
            self._log.flush()

    def set_force(self, force_n: float) -> bool:
        with self.lock:
            if force_n == self.force_n:
                return False
            self.force_n = force_n
        self.event(f"forza {force_n:g} N")
        return True

    def add_calibration_point(self, volts: float) -> bool:
        with self.lock:
            codes = [c for _, c in self.recent]
        if not codes:
            self.event(f"punto di calibrazione a {volts:g} V scartato: nessun campione nell'ultimo {CAL_SECONDS:g} s")
            return False
        if max(codes) - min(codes) > CAL_MAX_SPREAD:
            # segnale ancora in movimento: la media mescolerebbe due livelli diversi
            self.event(
                f"punto di calibrazione a {volts:g} V scartato: adc_filtered non stabile"
                f" ({min(codes)}..{max(codes)} nell'ultimo {CAL_SECONDS:g} s), riprova a segnale fermo"
            )
            return False
        code = sum(codes) / len(codes)
        self.calibration.add(code, volts)
        self.event(f"punto di calibrazione: codice {code:.1f} (media di {len(codes)}) = {volts:g} V; {self.calibration.describe()}")
        return True

    def reset_calibration(self) -> None:
        self.calibration.reset()
        self.event("calibrazione cancellata")

    def record(self, timestamp: float, data: dict) -> dict:
        """Aggiorna la media per la calibrazione, scrive la riga e la ritorna (valori numerici)."""
        now = time.monotonic()
        adc_filtered = int(data["adc_filtered"])
        with self.lock:
            self.recent.append((now, adc_filtered))
            while self.recent and self.recent[0][0] < now - CAL_SECONDS:
                self.recent.popleft()
            force_n = self.force_n
        if self.t0 is None:
            self.t0 = timestamp
        voltage_cal = self.calibration.volts(adc_filtered)
        volts_for_strain = voltage_cal if voltage_cal is not None else data["voltage"] / 1000.0
        row = {
            "t_s": timestamp - self.t0,
            "force_N": force_n,
            "adc_raw": int(data["adc_raw"]),
            "adc_filtered": adc_filtered,
            "voltage_fw_mV": data["voltage"],
            "voltage_cal_mV": None if voltage_cal is None else voltage_cal * 1000.0,
            "deformation_ustrain": hydra_deformation(volts_for_strain) * 1e6,
        }
        if self._writer is not None:
            line = {key: "" if value is None else value for key, value in row.items()}
            line["wall_time"] = now_iso()
            line["t_s"] = f"{row['t_s']:.4f}"
            self._writer.writerow(line)
            self._csv_file.flush()
        return row

    def close(self) -> None:
        self.event("stop")
        for f in (self._csv_file, self._log):
            if f is not None:
                f.close()
        if self.csv_path is not None:
            print(f"Sessione salvata in {self.csv_path}")


def handle_command(session: Session, text: str) -> None:
    """Comandi da terminale (--no-plot): numero = forza in N, 'cal <V>' o 'cal reset'."""
    text = text.strip().replace(",", ".")
    if not text:
        return
    words = text.split()
    try:
        if words[0].lower() == "cal":
            if len(words) == 2 and words[1].lower() == "reset":
                session.reset_calibration()
            else:
                session.add_calibration_point(float(words[1]))
        else:
            session.set_force(float(text))
    except (ValueError, IndexError):
        print(f"comando non valido: '{text}'. Esempi: '1500' (forza in N), 'cal 1.234' (V multimetro), 'cal reset'")


def read_commands(session: Session, stop: threading.Event) -> None:
    print("Comandi: '1500' = forza in N, 'cal 1.234' = punto di calibrazione in V, 'cal reset'")
    while not stop.is_set():
        line = sys.stdin.readline()
        if not line:
            return  # stdin chiuso
        handle_command(session, line)


def read_frames(bus: can.BusABC, msg, session: Session, stop: threading.Event, samples: deque | None) -> None:
    last_frame = last_warning = time.monotonic()
    while not stop.is_set():
        frame = bus.recv(timeout=0.1)
        now = time.monotonic()
        if frame is None:
            if now - last_warning >= SILENCE_WARNING_S:
                last_warning = now
                print(f"[nessun frame da {now - last_frame:.0f} s]")
            continue
        last_frame = last_warning = now
        if frame.is_error_frame or frame.arbitration_id != msg.frame_id:
            continue
        row = session.record(frame.timestamp, msg.decode(frame.data))
        cal = "" if row["voltage_cal_mV"] is None else f"  cal={row['voltage_cal_mV']:8.2f} mV"
        print(
            f"{row['t_s']:9.3f}  adc={row['adc_raw']:5d}  filt={row['adc_filtered']:5d}"
            f"  V={row['voltage_fw_mV']:8.2f} mV{cal}  def={row['deformation_ustrain']:+9.1f} µε"
        )
        if samples is not None:
            samples.append(row)


def plot_live(samples: deque, session: Session, window: float) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import Button, TextBox

    fig, (ax_adc, ax_v) = plt.subplots(2, 1, sharex=True, figsize=(9.5, 7.0))
    ax_adc.set_ylabel("ADC [codice]")
    ax_v.set_xlabel("tempo [s]")
    for ax in (ax_adc, ax_v):
        ax.ticklabel_format(useOffset=False)
        ax.grid(True)
    (raw_line,) = ax_adc.plot([], [], linewidth=0.8, alpha=0.5, label="adc_raw")
    (filt_line,) = ax_adc.plot([], [], linewidth=1.8, color=raw_line.get_color(), label="adc_filtered")
    ax_adc.legend(loc="upper left", fontsize=8)
    (volt_line,) = ax_v.plot([], [], linewidth=1.5, color="tab:green")
    stats = ax_adc.text(0.99, 0.97, "in attesa di campioni...", transform=ax_adc.transAxes, ha="right", va="top",
                        family="monospace", fontsize=8)
    fig.tight_layout(rect=(0.0, 0.13, 1.0, 0.95))

    xs: list[float] = []
    raw: list[float] = []
    filt: list[float] = []
    volts: list[float] = []

    def voltage_label() -> str:
        return "tensione calibrata [mV]" if session.calibration.gain is not None else "tensione nominale [mV]"

    def refresh_title() -> None:
        force = "non impostata" if session.force_n is None else f"{session.force_n:g} N"
        fig.suptitle(f"forza: {force}   |   {session.calibration.describe()}", fontsize=9)
        ax_v.set_ylabel(voltage_label())
        fig.canvas.draw_idle()

    def to_mv(code: float, fw_mv: float) -> float:
        v = session.calibration.volts(code)
        return fw_mv if v is None else v * 1000.0

    fw_mv: list[float] = []  # per ricalcolare la curva quando cambia la calibrazione

    def recompute_voltage() -> None:
        volts[:] = [to_mv(c, f) for c, f in zip(filt, fw_mv)]

    def update(_):
        if not samples:
            return  # niente di nuovo: nessun ridisegno
        while samples:
            row = samples.popleft()
            xs.append(row["t_s"])
            raw.append(row["adc_raw"])
            filt.append(row["adc_filtered"])
            fw_mv.append(row["voltage_fw_mV"])
            volts.append(to_mv(row["adc_filtered"], row["voltage_fw_mV"]))
        if window > 0:
            while xs and xs[0] < xs[-1] - window:
                del xs[0], raw[0], filt[0], fw_mv[0], volts[0]
        raw_line.set_data(xs, raw)
        filt_line.set_data(xs, filt)
        volt_line.set_data(xs, volts)
        recent = [r for x, r in zip(xs, raw) if x >= xs[-1] - 5.0]
        mean = sum(recent) / len(recent)
        std = (sum((r - mean) ** 2 for r in recent) / len(recent)) ** 0.5
        stats.set_text(f"ultimi 5 s: adc_raw medio {mean:.1f}\nrumore {std:.2f} codici rms, {max(recent) - min(recent):.0f} p-p")
        if window > 0:
            ax_adc.set_xlim(max(0.0, xs[-1] - window), max(window, xs[-1]))
        for ax in (ax_adc, ax_v):
            ax.relim()
            ax.autoscale_view(scalex=window <= 0)
        fig.canvas.draw_idle()

    def mark(text: str, color: str) -> None:
        t_mark = xs[-1] if xs else 0.0
        for ax in (ax_adc, ax_v):
            ax.axvline(t_mark, color=color, linestyle=":", linewidth=1.0)
            ax.text(t_mark, 1.0, f" {text}", transform=ax.get_xaxis_transform(), color=color, fontsize=8, va="top")

    def parse_number(text: str) -> float | None:
        text = text.strip().replace(",", ".")
        if not text:
            return None
        try:
            return float(text)
        except ValueError:
            print(f"'{text}' non e' un numero")
            return None

    def on_force(text: str) -> None:
        force_n = parse_number(text)
        if force_n is not None and session.set_force(force_n):
            mark(f"{force_n:g} N", "tab:red")
            refresh_title()

    def on_cal(text: str) -> None:
        v = parse_number(text)
        if v is None:
            return
        if session.add_calibration_point(v):
            mark(f"cal {v:g} V", "tab:purple")
            recompute_voltage()
            refresh_title()
        cal_box.set_val("")  # pronto per il punto successivo (non riscatta: testo vuoto)

    def on_reset(_event) -> None:
        session.reset_calibration()
        recompute_voltage()
        refresh_title()

    # tenere i riferimenti ai widget: senza, smettono di rispondere ai click
    force_box = TextBox(fig.add_axes((0.14, 0.06, 0.14, 0.045)), "forza [N] ")
    force_box.on_submit(on_force)
    cal_box = TextBox(fig.add_axes((0.47, 0.06, 0.14, 0.045)), "V multimetro ")
    cal_box.on_submit(on_cal)
    reset_button = Button(fig.add_axes((0.64, 0.06, 0.12, 0.045)), "Reset cal")
    reset_button.on_clicked(on_reset)
    saving = f"salvataggio: {session.csv_path.name} + .log" if session.csv_path else "sessione NON salvata (--no-save)"
    fig.text(0.14, 0.02, f"scrivi il valore e premi Invio   |   {saving}", fontsize=8)

    refresh_title()
    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=1000 // PLOT_FPS, cache_frame_data=False)  # noqa: F841
    plt.show()


def main() -> None:
    args = parse_args()
    if not args.dbc.is_file():
        sys.exit(f"{args.dbc} non trovato: passalo come argomento")
    msg = cantools.database.load_file(args.dbc).get_message_by_name(MESSAGE_NAME)
    print(f"In ascolto di {msg.name} (0x{msg.frame_id:X})...")

    try:
        bus = open_bus(BITRATE)
    except can.CanInitializationError as error:
        sys.exit(str(error))

    session = Session(args.out, not args.no_save)
    stop = threading.Event()
    reader = None
    try:
        if args.no_plot:
            threading.Thread(target=read_commands, args=(session, stop), daemon=True).start()
            read_frames(bus, msg, session, stop, None)
        else:
            samples: deque = deque()
            reader = threading.Thread(target=read_frames, args=(bus, msg, session, stop, samples), daemon=True)
            reader.start()
            plot_live(samples, session, args.window)  # ritorna alla chiusura della finestra
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        if reader is not None:
            reader.join(timeout=1.0)
        bus.shutdown()
        session.close()


if __name__ == "__main__":
    main()
