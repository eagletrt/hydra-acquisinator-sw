#!/usr/bin/env python3
"""Legge insieme l'acquisinator hydra e l'acquisinator lock-in dallo stesso CAN (gs_usb).

Uso: python read_both_acquisinators.py [--no-zero] [--no-plot] [--window S] [--cutoff HZ] [--taps N]
                                      [--ylim U] [--out CSV | --no-save] [--raw]
                                      [--hydra-dbc PATH] [--lockin-dbc PATH]

Le due schede stanno sullo stesso bus con ID diversi:
  - hydra   (questo repo):   ACQUISINATOR_HYDRA_ADC 0x7B0, tensione e codici ADC; la deformazione si ricava
                              qui dalla tensione con la formula del firmware (main.c)
  - lock-in (acquisinator-sw): ACQUISINATOR_BRIDGE 0x7A1, resistenza del gauge, la deformazione si ricava qui
                              con la stessa formula e la stessa convenzione di segno dell'hydra

Il grafico ha due pannelli, uno per scheda, entrambi in microstrain e con lo stesso asse dei tempi:
in ognuno il dato grezzo (sottile) e lo stesso dato dopo un FIR passa basso (--cutoff, --taps).
I due pannelli hanno sempre la stessa ampiezza di scala (stessi µε per divisione), ognuno
centrato sui propri dati: il rumore delle due schede si confronta a occhio. La scala si adatta
alla scheda con l'escursione maggiore; con lo slider in basso (o --ylim U all'avvio) si fissa
a mano a ±U µε, e ogni pannello viene centrato sulla mediana dei suoi dati, cosi' un picco
non schiaccia il resto della traccia. "Auto" torna alla scala automatica.
All'avvio ogni scheda viene azzerata sulla media del suo primo secondo di campioni (puntoni
scarichi!), cosi' le due curve partono entrambe da 0 e sono confrontabili; con --no-zero si
vedono i valori assoluti (hydra: zero a STRAIN_GAUGE_VREF, senza l'offset della flash; lock-in:
riferito a 350 ohm).
Sessione: ogni frame delle schede viene salvato su CSV (default Tools/sessions/, --out per
sceglierlo, --no-save per non salvare) con tensione, codici ADC, resistenza, deformazione e la
forza applicata in N. La forza si scrive nella casella "forza [N]" del grafico e si preme Invio
(con --no-plot si scrive nel terminale): vale per tutte le righe successive, e nel grafico il
cambio viene segnato con una linea verticale.
Il CAN viene letto in un thread separato e il grafico viene ridisegnato solo quando arrivano
campioni nuovi, al massimo PLOT_FPS volte al secondo: cosi' non accumula ritardo.
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

BITRATE = 1_000_000
PLOT_FPS = 20
ZERO_SECONDS = 1.0  # finestra mediata per lo zero all'avvio
NOISE_SECONDS = 5.0  # finestra su cui si calcolano media e rumore mostrati nel grafico
Y_MARGIN = 0.1  # margine sopra e sotto i dati con la scala automatica, in frazione dell'escursione
MIN_Y_HALF_SPAN = 1.0  # µε, evita una scala nulla quando il segnale e' piatto
SLIDER_MIN_HALF_SPAN = 0.3  # µε, estremi dello slider della scala verticale
SLIDER_MAX_HALF_SPAN = 10000.0
SILENCE_WARNING_S = 2.0  # ogni quanto avvisare se dal bus non arriva niente

HYDRA = "hydra"
LOCKIN = "lock-in"
BOARDS = (HYDRA, LOCKIN)

HYDRA_MESSAGE_NAME = "ACQUISINATOR_HYDRA_ADC"
LOCKIN_MESSAGE_NAME = "ACQUISINATOR_BRIDGE"
LOCKIN_DIAG_MESSAGE_NAME = "ACQUISINATOR_DIAG"

# campioni al secondo di ciascuna scheda, servono per progettare il FIR
SAMPLE_RATE_HZ = {
    HYDRA: 300.0,  # 1500 Hz / ACQUISINATORE_ADC_SEND_EVERY (5)
    LOCKIN: 125.0,  # 125 Hz / OUTPUT_EVERY_PERIODS del firmware lock-in (main.c di acquisinator-sw)
}

# Come in acquisinator_config.h
STRAIN_GAUGE_R1 = 352.0  # ohm
STRAIN_GAUGE_R2 = 350.0
STRAIN_GAUGE_R3 = 352.0
STRAIN_GAUGE_RNOM = 350.0
STRAIN_GAUGE_G = 100.0  # guadagno dell'amplificatore
STRAIN_GAUGE_VREF = 1.501  # V, uscita dell'amplificatore a ponte bilanciato
STRAIN_GAUGE_VS = 3.294  # V, alimentazione del ponte
STRAIN_GAUGE_K = 2.01  # gauge factor

LOCKIN_FLAG_NAMES = ((0, "SATURA BASSO"), (1, "SATURA ALTO"), (2, "ERRORE SPI"), (3, "non calibrato"), (4, "NESSUNA MISURA"))

TOOLS_DIR = Path(__file__).resolve().parent
DEFAULT_HYDRA_DBC = TOOLS_DIR / "acquisinator_hydra_adc.dbc"
DEFAULT_LOCKIN_DBC = TOOLS_DIR / "acquisinator_lockin.dbc"  # copia di acquisinator-sw/acquisinator.dbc
SESSIONS_DIR = TOOLS_DIR / "sessions"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--hydra-dbc", type=Path, default=DEFAULT_HYDRA_DBC, help="dbc dell'acquisinator hydra")
    parser.add_argument("--lockin-dbc", type=Path, default=DEFAULT_LOCKIN_DBC, help="dbc dell'acquisinator lock-in")
    parser.add_argument("--no-zero", action="store_true", help="non azzerare all'avvio: valori assoluti")
    parser.add_argument("--no-plot", action="store_true", help="stampa soltanto, senza grafico")
    parser.add_argument("--raw", action="store_true", help="stampa anche ogni frame ricevuto, qualunque ID (debug)")
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        metavar="CSV",
        help="file della sessione (default: Tools/sessions/sessione_<data>_<ora>.csv)",
    )
    parser.add_argument("--no-save", action="store_true", help="non salvare la sessione su file")
    parser.add_argument(
        "--window",
        type=float,
        default=30.0,
        metavar="S",
        help="secondi mostrati nel grafico, 0 = tutta la storia (default: 30)",
    )
    parser.add_argument("--cutoff", type=float, default=0.3, metavar="HZ", help="taglio del FIR passa basso (default: 0.3)")
    parser.add_argument(
        "--taps",
        type=int,
        default=21,
        metavar="N",
        help="numero di coefficienti del FIR: piu' sono, piu' taglia netto ma piu' ritarda (default: 21)",
    )
    parser.add_argument(
        "--ylim",
        type=float,
        default=None,
        metavar="U",
        help="scala fissa di ±U µε su entrambi i pannelli (default: automatica, comune ai due)",
    )
    args = parser.parse_args()
    if args.ylim is not None and args.ylim <= 0:
        parser.error("--ylim deve essere positivo")
    nyquist = min(SAMPLE_RATE_HZ.values()) / 2
    if not 0 < args.cutoff < nyquist:
        parser.error(f"--cutoff deve stare tra 0 e {nyquist:.1f} Hz (Nyquist della scheda piu' lenta)")
    if args.taps < 1:
        parser.error("--taps deve essere almeno 1")
    for path in (args.hydra_dbc, args.lockin_dbc):
        if not path.is_file():
            parser.error(f"{path} non trovato")
    return args


def hydra_deformation(voltage_v: float) -> float:
    """Strain adimensionale dalla tensione, come acquisinatore_link_deformation_from_V_to_elongation()."""
    x = (voltage_v - STRAIN_GAUGE_VREF) / STRAIN_GAUGE_G * (STRAIN_GAUGE_R1 + STRAIN_GAUGE_R2) / STRAIN_GAUGE_VS
    resistance = STRAIN_GAUGE_R2 * (STRAIN_GAUGE_R3 - x) / (STRAIN_GAUGE_R1 + x)
    return (STRAIN_GAUGE_RNOM - resistance) / (STRAIN_GAUGE_RNOM * STRAIN_GAUGE_K)


def lockin_deformation(resistance: float) -> float:
    """Strain adimensionale dalla resistenza, stessa formula e segno del firmware hydra."""
    return (STRAIN_GAUGE_RNOM - resistance) / (STRAIN_GAUGE_RNOM * STRAIN_GAUGE_K)


class Zero:
    """Media del primo ZERO_SECONDS di campioni di una scheda, poi sottratta a tutti i successivi."""

    def __init__(self, board: str, enabled: bool) -> None:
        self.board = board
        self.value = None if enabled else 0.0
        self.t_start = None
        self.buffer: list[float] = []

    def apply(self, t: float, ustrain: float) -> float | None:
        """Ritorna il valore azzerato, oppure None finche' lo zero non e' pronto."""
        if self.value is not None:
            return ustrain - self.value
        if self.t_start is None:
            self.t_start = t
        self.buffer.append(ustrain)
        if t - self.t_start < ZERO_SECONDS:
            return None
        self.value = sum(self.buffer) / len(self.buffer)
        print(f"Zero {self.board}: {self.value:+.1f} µε (media di {len(self.buffer)} campioni)")
        return ustrain - self.value


class Session:
    """Registra ogni frame delle schede su CSV, insieme alla forza applicata in quel momento.

    Ogni riga viene scritta e flushata subito: se lo script si chiude male i dati restano.
    La forza [N] la imposta l'utente (casella nel grafico, o da terminale con --no-plot) e
    finisce in tutte le righe successive, finche' non viene cambiata.
    """

    COLUMNS = (
        "wall_time",  # ora del PC, ISO 8601
        "t_s",  # secondi dal primo frame
        "board",
        "message",
        "force_N",
        "deformation_ustrain",  # azzerata come nel grafico (vuota finche' lo zero non e' pronto)
        "deformation_abs_ustrain",  # senza zero
        "voltage_mV",  # hydra
        "adc_raw",  # hydra: ultimo campione; lock-in: media della finestra (DIAG)
        "adc_filtered",  # hydra: media mobile
        "lockin_out_mV",  # lock-in (DIAG)
        "resistance_ohm",  # lock-in (BRIDGE)
        "phase_deg",
        "flags",
    )

    def __init__(self, path: Path | None) -> None:
        self.force_n: float | None = None
        self.t0: float | None = None
        self.path = path
        self._file = None
        self._writer = None
        if path is not None:
            path.parent.mkdir(parents=True, exist_ok=True)
            self._file = open(path, "w", newline="", encoding="utf-8")
            self._writer = csv.DictWriter(self._file, fieldnames=self.COLUMNS)
            self._writer.writeheader()
            print(f"Sessione salvata in {path}")

    def set_force(self, force_n: float) -> None:
        self.force_n = force_n
        print(f"Forza applicata: {force_n:g} N")

    def write(self, timestamp: float, board: str, message: str, **values) -> None:
        if self._writer is None:
            return
        if self.t0 is None:
            self.t0 = timestamp
        row = {
            "wall_time": datetime.now().isoformat(timespec="milliseconds"),
            "t_s": f"{timestamp - self.t0:.4f}",
            "board": board,
            "message": message,
            "force_N": "" if self.force_n is None else f"{self.force_n:g}",
        }
        row.update({key: "" if value is None else value for key, value in values.items()})
        self._writer.writerow(row)
        self._file.flush()

    def close(self) -> None:
        if self._file is not None:
            self._file.close()
            print(f"Sessione salvata in {self.path}")


def read_force_from_stdin(session: Session, stop: threading.Event) -> None:
    """Con --no-plot: ogni riga scritta nel terminale (un numero, in N) imposta la forza."""
    print("Scrivi la forza applicata in N e premi Invio per registrarla")
    while not stop.is_set():
        line = sys.stdin.readline()
        if not line:
            return  # stdin chiuso
        try:
            session.set_force(float(line.strip().replace(",", ".")))
        except ValueError:
            print(f"'{line.strip()}' non e' un numero: scrivi la forza in N, es. 1500")


def read_frames(
    bus: can.BusABC,
    hydra,
    lockin,
    diag,
    zero: bool,
    raw: bool,
    session: Session,
    stop: threading.Event,
    samples: deque | None,
) -> None:
    zeros = {board: Zero(board, zero) for board in BOARDS}
    last_frame = time.monotonic()
    last_warning = last_frame
    while not stop.is_set():
        frame = bus.recv(timeout=0.1)
        now = time.monotonic()
        if frame is None:
            # bus muto: lo si dice, altrimenti non si distingue da "frame con ID sconosciuti"
            if now - last_warning >= SILENCE_WARNING_S:
                last_warning = now
                print(f"[nessun frame da {now - last_frame:.0f} s]")
            continue
        last_frame = last_warning = now
        if raw:
            print(f"RAW id=0x{frame.arbitration_id:X} ext={frame.is_extended_id} err={frame.is_error_frame} dlc={frame.dlc} data={frame.data.hex()}")
        if frame.is_error_frame:
            continue

        if frame.arbitration_id == hydra.frame_id:
            data = hydra.decode(frame.data)
            board, ustrain = HYDRA, hydra_deformation(data["voltage"] / 1000.0) * 1e6
            info = f"V={data['voltage']:8.2f} mV  adc={int(data['adc_raw']):5d}  filt={int(data['adc_filtered']):5d}"
            columns = {
                "voltage_mV": data["voltage"],
                "adc_raw": int(data["adc_raw"]),
                "adc_filtered": int(data["adc_filtered"]),
            }
        elif frame.arbitration_id == lockin.frame_id:
            data = lockin.decode(frame.data)
            flags = int(data["flags"])
            notes = ", ".join(name for bit, name in LOCKIN_FLAG_NAMES if flags & (1 << bit))
            columns = {"resistance_ohm": data["resistance"], "phase_deg": data["phase"], "flags": flags}
            if data["resistance"] != data["resistance"]:  # NaN: frame di stato senza misura
                print(f"{frame.timestamp:.3f}  {LOCKIN:7s}  {notes}")
                session.write(frame.timestamp, LOCKIN, lockin.name, **columns)
                continue
            board, ustrain = LOCKIN, lockin_deformation(data["resistance"]) * 1e6
            info = f"R={data['resistance']:8.3f} ohm  {notes}"
        elif frame.arbitration_id == diag.frame_id:
            # solo su file: tensione e codice ADC del lock-in, non c'e' una deformazione da plottare
            data = diag.decode(frame.data)
            session.write(
                frame.timestamp,
                LOCKIN,
                diag.name,
                lockin_out_mV=data["lockin_out"] * 1000.0,
                adc_raw=int(data["adc_raw"]),
            )
            continue
        else:
            continue

        value = zeros[board].apply(frame.timestamp, ustrain)
        session.write(
            frame.timestamp,
            board,
            hydra.name if board == HYDRA else lockin.name,
            deformation_ustrain=value,
            deformation_abs_ustrain=ustrain,
            **columns,
        )
        if value is None:
            continue
        print(f"{frame.timestamp:.3f}  {board:7s}  def={value:+9.1f} µε  {info}")
        if samples is not None:
            samples.append((frame.timestamp, board, value))


def fir_lowpass(cutoff_hz: float, n_taps: int, sample_rate_hz: float):
    """Coefficienti di un FIR passa basso (sinc finestrata Hamming), guadagno unitario in continua."""
    import numpy as np

    fc = cutoff_hz / sample_rate_hz  # cicli/campione
    n = np.arange(n_taps) - (n_taps - 1) / 2
    taps = 2 * fc * np.sinc(2 * fc * n) * np.hamming(n_taps)
    return taps / taps.sum()


def plot_live(
    samples: deque, session: Session, window: float, cutoff_hz: float, n_taps: int, y_half_span: float | None
) -> None:
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import Button, Slider, TextBox

    fig, axes = plt.subplots(len(BOARDS), 1, sharex=True, figsize=(9.0, 7.0))
    axes = dict(zip(BOARDS, axes))
    axes[BOARDS[-1]].set_xlabel("tempo [s]")

    taps = {board: fir_lowpass(cutoff_hz, n_taps, SAMPLE_RATE_HZ[board]) for board in BOARDS}
    lines = {}
    stats = {}
    for board, ax in axes.items():
        delay_s = (n_taps - 1) / 2 / SAMPLE_RATE_HZ[board]
        ax.set_title(f"{board}  (FIR {cutoff_hz:g} Hz, {n_taps} tap, ritardo {delay_s:.1f} s)", loc="left", fontsize=10)
        ax.set_ylabel("deformazione [µε]")
        ax.ticklabel_format(useOffset=False)
        ax.grid(True)
        (raw_line,) = ax.plot([], [], linewidth=0.8, alpha=0.4, label="grezzo")
        (fir_line,) = ax.plot([], [], linewidth=1.8, color=raw_line.get_color(), label="FIR")
        ax.legend(loc="upper left", fontsize=8)
        lines[board] = (raw_line, fir_line)
        stats[board] = ax.text(
            0.99, 0.97, "in attesa di campioni...", transform=ax.transAxes, ha="right", va="top", family="monospace", fontsize=8
        )
    # spazio in alto per il titolo con la scala, in basso per forza, slider e pulsante
    fig.tight_layout(rect=(0.0, 0.14, 1.0, 0.96))

    t0 = None
    series: dict[str, tuple[list[float], list[float], list[float]]] = {board: ([], [], []) for board in BOARDS}
    history: dict[str, deque | None] = {board: None for board in BOARDS}  # ultimi n_taps campioni grezzi
    manual_half = y_half_span  # None = scala automatica

    def update(_):
        nonlocal t0
        if not samples:
            return  # niente di nuovo: nessun ridisegno
        while samples:
            t, board, ustrain = samples.popleft()
            if t0 is None:
                t0 = t
            if history[board] is None:
                # storia inizializzata al primo valore: il filtro parte gia' a regime
                history[board] = deque([ustrain] * n_taps, maxlen=n_taps)
            history[board].append(ustrain)
            xs, ys, yf = series[board]
            xs.append(t - t0)
            ys.append(ustrain)
            yf.append(float(np.dot(taps[board], history[board])))

        t_last = max(xs[-1] for xs, _, _ in series.values() if xs)
        for board, (xs, ys, yf) in series.items():
            if window > 0:
                while xs and xs[0] < t_last - window:
                    del xs[0], ys[0], yf[0]
            lines[board][0].set_data(xs, ys)
            lines[board][1].set_data(xs, yf)
            if xs:
                recent = [y for x, y in zip(xs, ys) if x >= xs[-1] - NOISE_SECONDS]
                mean = sum(recent) / len(recent)
                std = (sum((y - mean) ** 2 for y in recent) / len(recent)) ** 0.5
                stats[board].set_text(
                    f"ultimi {NOISE_SECONDS:.0f} s: media {mean:+.1f} µε\n"
                    f"rumore {std:.2f} µε rms, {max(recent) - min(recent):.2f} µε p-p"
                )
        if window > 0:
            axes[BOARDS[0]].set_xlim(max(0.0, t_last - window), max(window, t_last))
        else:
            axes[BOARDS[0]].set_xlim(0.0, max(t_last, 1.0))
        rescale()

    def rescale() -> None:
        """Stessa ampiezza di scala (µε per divisione) su tutti i pannelli.

        Automatica: si adatta alla scheda con l'escursione maggiore, ogni pannello centrato sul
        punto medio dei suoi dati. Manuale (slider): ogni pannello centrato sulla mediana dei
        suoi dati visibili, cosi' un picco non sposta la vista e il resto della traccia si legge.
        """
        centers = {}
        half = manual_half if manual_half is not None else 0.0
        for board, (_, ys, _) in series.items():
            if not ys:
                continue
            if manual_half is None:
                centers[board] = (max(ys) + min(ys)) / 2.0
                half = max(half, (max(ys) - min(ys)) / 2.0 * (1.0 + Y_MARGIN))
            else:
                centers[board] = float(np.median(ys))
        half = max(half, MIN_Y_HALF_SPAN) if manual_half is None else half
        for board, center in centers.items():
            axes[board].set_ylim(center - half, center + half)
        mode = "auto" if manual_half is None else "manuale"
        force = "non impostata" if session.force_n is None else f"{session.force_n:g} N"
        fig.suptitle(f"scala comune ({mode}): ±{half:.1f} µε su ogni pannello   |   forza: {force}", fontsize=10)
        fig.canvas.draw_idle()

    def on_force(text: str) -> None:
        text = text.strip().replace(",", ".")
        if not text:
            return
        try:
            force_n = float(text)
        except ValueError:
            print(f"'{text}' non e' un numero: scrivi la forza in N, es. 1500")
            return
        if force_n == session.force_n:
            return  # la casella invia di nuovo lo stesso valore anche quando si clicca fuori
        session.set_force(force_n)
        # segna il cambio di forza sui due pannelli, nell'istante dell'ultimo campione
        t_mark = max((xs[-1] for xs, _, _ in series.values() if xs), default=0.0)
        for ax in axes.values():
            ax.axvline(t_mark, color="tab:red", linestyle=":", linewidth=1.0)
            ax.text(
                t_mark, 1.0, f" {force_n:g} N", transform=ax.get_xaxis_transform(),
                color="tab:red", fontsize=8, va="top", clip_on=True,
            )
        rescale()

    def on_slider(log_half: float) -> None:
        nonlocal manual_half
        manual_half = 10.0**log_half
        slider.valtext.set_text(f"±{manual_half:.3g} µε")
        rescale()

    def on_auto(_event) -> None:
        nonlocal manual_half
        manual_half = None
        slider.valtext.set_text("auto")
        rescale()

    # Slider logaritmico: copre sia il rumore (frazioni di µε) sia i picchi (migliaia di µε)
    slider = Slider(
        fig.add_axes((0.18, 0.03, 0.55, 0.03)),
        "scala ±µε",
        np.log10(SLIDER_MIN_HALF_SPAN),
        np.log10(SLIDER_MAX_HALF_SPAN),
        valinit=np.log10(manual_half if manual_half is not None else 50.0),
    )
    slider.valtext.set_text("auto" if manual_half is None else f"±{manual_half:.3g} µε")
    slider.on_changed(on_slider)
    auto_button = Button(fig.add_axes((0.86, 0.02, 0.10, 0.05)), "Auto")
    auto_button.on_clicked(on_auto)
    # forza in N: si scrive il valore e si preme Invio, vale per tutti i campioni successivi
    force_box = TextBox(fig.add_axes((0.18, 0.08, 0.15, 0.04)), "forza [N] ", initial="")
    force_box.on_submit(on_force)
    fig.text(0.35, 0.10, "scrivi e premi Invio" + (f"  -  salvataggio: {session.path.name}" if session.path else ""),
             fontsize=8, va="center")

    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=1000 // PLOT_FPS, cache_frame_data=False)  # noqa: F841
    plt.show()


def main() -> None:
    args = parse_args()
    hydra = cantools.database.load_file(args.hydra_dbc).get_message_by_name(HYDRA_MESSAGE_NAME)
    lockin_db = cantools.database.load_file(args.lockin_dbc)
    lockin = lockin_db.get_message_by_name(LOCKIN_MESSAGE_NAME)
    diag = lockin_db.get_message_by_name(LOCKIN_DIAG_MESSAGE_NAME)
    print(
        f"In ascolto di {HYDRA}: {hydra.name} (0x{hydra.frame_id:X}) e {LOCKIN}: {lockin.name} (0x{lockin.frame_id:X}),"
        f" {diag.name} (0x{diag.frame_id:X})..."
    )

    try:
        bus = open_bus(BITRATE)
    except can.CanInitializationError as error:
        sys.exit(str(error))

    if args.no_save:
        out = None
    else:
        out = args.out or SESSIONS_DIR / f"sessione_{datetime.now():%Y%m%d_%H%M%S}.csv"
    session = Session(out)
    stop = threading.Event()
    reader = None
    try:
        if args.no_plot:
            threading.Thread(target=read_force_from_stdin, args=(session, stop), daemon=True).start()
            read_frames(bus, hydra, lockin, diag, not args.no_zero, args.raw, session, stop, None)
        else:
            samples: deque = deque()
            reader = threading.Thread(
                target=read_frames,
                args=(bus, hydra, lockin, diag, not args.no_zero, args.raw, session, stop, samples),
                daemon=True,
            )
            reader.start()
            # ritorna alla chiusura della finestra
            plot_live(samples, session, args.window, args.cutoff, args.taps, args.ylim)
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
