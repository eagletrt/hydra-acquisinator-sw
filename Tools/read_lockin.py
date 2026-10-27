#!/usr/bin/env python3
"""Legge l'acquisinator lock-in (acquisinator-sw) tramite gs_usb e registra la sessione, come read_link_deformation.py.

Uso: python read_lockin.py [--no-plot] [--window S] [--out NOME] [--no-save] [dbc]

Il lock-in non manda i singoli campioni dell'ADC: ogni misura (125 al secondo con il firmware
attuale, una per periodo di eccitazione) e' il risultato del demodulatore sull'ultima finestra,
in due frame consecutivi:
  - 0x7A0 ACQUISINATOR_DIAG:   lockin_out = componente in fase all'uscita dell'amplificatore [V],
                               adc_raw = media dell'uscita sulla finestra [codici], adc_poll
  - 0x7A1 ACQUISINATOR_BRIDGE: resistenza del gauge [ohm], fase [deg], flag, seq
Il dato piu' grezzo e' lockin_out: qui viene riportato anche in codici ADC (lockin_codes), la
stessa unita' di adc_raw / adc_filtered dell'hydra, per confrontare rumore e sensibilita'.

Grafico live:
  - sopra: componente in fase [codici ADC] (sottile) e mediana mobile di 1 s
  - sotto: resistenza del gauge [ohm]
Forza: scrivere i N applicati nella casella "forza [N]" e premere Invio (con --no-plot nel
terminale): finisce in tutte le righe successive del CSV, nel log e come linea nel grafico.

Sessione (default, --no-save per non salvare) in Tools/sessions/, mai sovrascritta:
  - <nome>.csv: una riga per misura, con ora del PC, forza e tutti i campi dei due frame
  - <nome>.log: eventi con timestamp (avvio, forza, misure mancanti, stop)
"""

import argparse
import csv
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path

import can
import cantools

from gs_usb_bus import open_bus

DIAG_MESSAGE_NAME = "ACQUISINATOR_DIAG"
BRIDGE_MESSAGE_NAME = "ACQUISINATOR_BRIDGE"
BITRATE = 1_000_000
PLOT_FPS = 20
SILENCE_WARNING_S = 2.0
SMOOTH_S = 1.0  # mediana mobile nel grafico
NOISE_SECONDS = 5.0

ADC_VREF_V = 3.29  # LTC1865L_VREF del firmware lock-in
ADC_FULL_SCALE = 65536
STRAIN_GAUGE_RNOM = 350.0  # ohm
STRAIN_GAUGE_K = 2.01

FLAG_NAMES = ((0, "SATURA BASSO"), (1, "SATURA ALTO"), (2, "ERRORE SPI"), (3, "non calibrato"), (4, "NESSUNA MISURA"))

TOOLS_DIR = Path(__file__).resolve().parent
DEFAULT_DBC = TOOLS_DIR / "acquisinator_lockin.dbc"  # copia di acquisinator-sw/acquisinator.dbc
SESSIONS_DIR = TOOLS_DIR / "sessions"

CSV_COLUMNS = (
    "wall_time",  # ora del PC, ISO 8601 al millisecondo
    "t_s",  # secondi dalla prima misura
    "force_N",
    "seq",  # contatore del firmware (0..255): un salto = misura persa
    "lockin_out_mV",  # componente in fase all'uscita dell'amplificatore, con segno
    "lockin_codes",  # la stessa in codici ADC
    "adc_mean",  # media dell'uscita dell'amplificatore sulla finestra [codici]
    "adc_poll",  # 0 = SPI ok
    "resistance_ohm",
    "phase_deg",
    "flags",
    "deformation_ustrain",  # (350 - R) / (350 * K), stesso segno dell'hydra
)


def now_iso() -> str:
    return datetime.now().isoformat(timespec="milliseconds")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dbc", nargs="?", type=Path, default=DEFAULT_DBC, help="dbc del lock-in")
    parser.add_argument("--no-plot", action="store_true", help="stampa soltanto, forza da terminale")
    parser.add_argument("--window", type=float, default=60.0, metavar="S",
                        help="secondi mostrati nel grafico, 0 = tutta la storia (default: 60)")
    parser.add_argument("--out", type=str, default=None, metavar="NOME",
                        help="nome della sessione, senza estensione (default: lockin_<data>_<ora>)")
    parser.add_argument("--no-save", action="store_true", help="non salvare la sessione")
    parser.add_argument("--quiet", action="store_true", help="non stampare ogni misura (125 righe/s)")
    return parser.parse_args()


class Session:
    """Forza corrente, CSV e log della sessione; condiviso fra lettura CAN e interfaccia."""

    def __init__(self, name: str | None, save: bool) -> None:
        self.lock = threading.Lock()
        self.force_n: float | None = None
        self.t0: float | None = None
        self.csv_path = self.log_path = None
        self._csv_file = self._writer = self._log = None
        if save:
            SESSIONS_DIR.mkdir(parents=True, exist_ok=True)
            stem = name or f"lockin_{datetime.now():%Y%m%d_%H%M%S}"
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
        self.event("avvio (lock-in)")

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

    def record(self, timestamp: float, row: dict) -> dict:
        if self.t0 is None:
            self.t0 = timestamp
        row["t_s"] = timestamp - self.t0
        with self.lock:
            row["force_N"] = self.force_n
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


def read_commands(session: Session, stop: threading.Event) -> None:
    """Con --no-plot: ogni riga scritta nel terminale (un numero, in N) imposta la forza."""
    print("Scrivi la forza applicata in N e premi Invio per registrarla")
    while not stop.is_set():
        line = sys.stdin.readline()
        if not line:
            return
        try:
            session.set_force(float(line.strip().replace(",", ".")))
        except ValueError:
            print(f"'{line.strip()}' non e' un numero: scrivi la forza in N, es. 150")


def read_frames(bus: can.BusABC, diag, bridge, session: Session, quiet: bool, stop: threading.Event,
                samples: deque | None) -> None:
    last_frame = last_warning = time.monotonic()
    last_diag = None  # DIAG arriva subito prima del BRIDGE della stessa misura
    last_seq = None
    while not stop.is_set():
        frame = bus.recv(timeout=0.1)
        now = time.monotonic()
        if frame is None:
            if now - last_warning >= SILENCE_WARNING_S:
                last_warning = now
                print(f"[nessun frame da {now - last_frame:.0f} s]")
            continue
        last_frame = last_warning = now
        if frame.is_error_frame:
            continue
        if frame.arbitration_id == diag.frame_id:
            last_diag = diag.decode(frame.data)
            continue
        if frame.arbitration_id != bridge.frame_id:
            continue

        data = bridge.decode(frame.data)
        flags = int(data["flags"])
        notes = ", ".join(name for bit, name in FLAG_NAMES if flags & (1 << bit) and bit != 3)
        resistance = data["resistance"]
        if resistance != resistance:  # NaN: frame di stato senza misura
            session.event(f"nessuna misura: {notes}")
            continue
        seq = int(data["seq"])
        if last_seq is not None and seq != (last_seq + 1) % 256:
            session.event(f"misure perse: seq {last_seq} -> {seq}")
        last_seq = seq

        row = {key: None for key in CSV_COLUMNS}
        row.update(seq=seq, resistance_ohm=resistance, phase_deg=data["phase"], flags=flags,
                   deformation_ustrain=(STRAIN_GAUGE_RNOM - resistance) / (STRAIN_GAUGE_RNOM * STRAIN_GAUGE_K) * 1e6)
        if last_diag is not None:
            row.update(lockin_out_mV=last_diag["lockin_out"] * 1000.0,
                       lockin_codes=last_diag["lockin_out"] / ADC_VREF_V * ADC_FULL_SCALE,
                       adc_mean=int(last_diag["adc_raw"]), adc_poll=int(last_diag["adc_poll"]))
            last_diag = None
        row = session.record(frame.timestamp, row)
        if not quiet:
            codes = "      -" if row["lockin_codes"] is None else f"{row['lockin_codes']:+9.1f}"
            print(f"{row['t_s']:9.3f}  in fase={codes} cod  media={row['adc_mean'] or 0:5d}  R={resistance:8.3f} ohm"
                  f"  fase={data['phase']:+7.2f}°  {notes}")
        if samples is not None and row["lockin_codes"] is not None:
            samples.append(row)


def plot_live(samples: deque, session: Session, window: float) -> None:
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import TextBox

    fig, (ax_c, ax_r) = plt.subplots(2, 1, sharex=True, figsize=(9.5, 7.0))
    ax_c.set_ylabel("in fase [codici ADC]")
    ax_r.set_ylabel("resistenza [ohm]")
    ax_r.set_xlabel("tempo [s]")
    for ax in (ax_c, ax_r):
        ax.ticklabel_format(useOffset=False)
        ax.grid(True)
    (raw_line,) = ax_c.plot([], [], linewidth=0.7, alpha=0.5, label="misura")
    (med_line,) = ax_c.plot([], [], linewidth=1.8, color=raw_line.get_color(), label=f"mediana {SMOOTH_S:g} s")
    ax_c.legend(loc="upper left", fontsize=8)
    (r_line,) = ax_r.plot([], [], linewidth=1.0, color="tab:green")
    stats = ax_c.text(0.99, 0.97, "in attesa di misure...", transform=ax_c.transAxes, ha="right", va="top",
                      family="monospace", fontsize=8)
    fig.tight_layout(rect=(0.0, 0.10, 1.0, 0.95))

    xs: list[float] = []
    codes: list[float] = []
    res: list[float] = []

    def refresh_title() -> None:
        force = "non impostata" if session.force_n is None else f"{session.force_n:g} N"
        fig.suptitle(f"lock-in   |   forza: {force}", fontsize=10)
        fig.canvas.draw_idle()

    def update(_):
        if not samples:
            return
        while samples:
            row = samples.popleft()
            xs.append(row["t_s"])
            codes.append(row["lockin_codes"])
            res.append(row["resistance_ohm"])
        if window > 0:
            while xs and xs[0] < xs[-1] - window:
                del xs[0], codes[0], res[0]
        t = np.asarray(xs)
        c = np.asarray(codes)
        lo = np.searchsorted(t, t - SMOOTH_S / 2)
        hi = np.searchsorted(t, t + SMOOTH_S / 2, side="right")
        med = [np.median(c[a:b]) for a, b in zip(lo, hi)] if len(t) < 20000 else c
        raw_line.set_data(xs, codes)
        med_line.set_data(xs, med)
        r_line.set_data(xs, res)
        in_noise = t >= t[-1] - NOISE_SECONDS
        recent = c[in_noise]
        span = t[-1] - t[in_noise][0]
        rate = f", {(len(recent) - 1) / span:.0f} misure/s" if span > 0 else ""
        stats.set_text(f"ultimi {NOISE_SECONDS:.0f} s: in fase medio {recent.mean():+.1f}\n"
                       f"rumore {recent.std():.2f} codici rms, {np.ptp(recent):.1f} p-p{rate}")
        if window > 0:
            ax_c.set_xlim(max(0.0, xs[-1] - window), max(window, xs[-1]))
        for ax in (ax_c, ax_r):
            ax.relim()
            ax.autoscale_view(scalex=window <= 0)
        fig.canvas.draw_idle()

    def on_force(text: str) -> None:
        text = text.strip().replace(",", ".")
        if not text:
            return
        try:
            force_n = float(text)
        except ValueError:
            print(f"'{text}' non e' un numero")
            return
        if session.set_force(force_n):
            t_mark = xs[-1] if xs else 0.0
            for ax in (ax_c, ax_r):
                ax.axvline(t_mark, color="tab:red", linestyle=":", linewidth=1.0)
                ax.text(t_mark, 1.0, f" {force_n:g} N", transform=ax.get_xaxis_transform(), color="tab:red",
                        fontsize=8, va="top")
            refresh_title()

    # tenere i riferimenti ai widget: senza, smettono di rispondere
    force_box = TextBox(fig.add_axes((0.14, 0.03, 0.14, 0.045)), "forza [N] ")
    force_box.on_submit(on_force)
    saving = f"salvataggio: {session.csv_path.name} + .log" if session.csv_path else "sessione NON salvata (--no-save)"
    fig.text(0.31, 0.05, f"scrivi e premi Invio   |   {saving}", fontsize=8, va="center")

    refresh_title()
    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=1000 // PLOT_FPS, cache_frame_data=False)  # noqa: F841
    plt.show()


def main() -> None:
    args = parse_args()
    if not args.dbc.is_file():
        sys.exit(f"{args.dbc} non trovato: passalo come argomento")
    db = cantools.database.load_file(args.dbc)
    diag = db.get_message_by_name(DIAG_MESSAGE_NAME)
    bridge = db.get_message_by_name(BRIDGE_MESSAGE_NAME)
    print(f"In ascolto di {diag.name} (0x{diag.frame_id:X}) e {bridge.name} (0x{bridge.frame_id:X})...")

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
            read_frames(bus, diag, bridge, session, args.quiet, stop, None)
        else:
            samples: deque = deque()
            reader = threading.Thread(target=read_frames, args=(bus, diag, bridge, session, args.quiet, stop, samples),
                                      daemon=True)
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
