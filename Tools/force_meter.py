#!/usr/bin/env python3
"""Bilancia live: legge l'hydra e mostra la forza applicata al braccetto, con la calibrazione dell'ultima prova a pesi.

Uso: python force_meter.py [--setup morsa|assiale] [--k COD_PER_N] [--avg S] [--no-plot] [dbc]

Calibrazioni (--setup). Il gauge e' una sola griglia a 45 gradi: vede assiale, flessione e torsione,
quindi la sensibilita' dipende da come e' caricato il braccetto e va scelta per il montaggio:
  - morsa (default): braccetto dritto in morsa con pesi appesi (flessione), prova
    test-corretto-hydra-peso-morsa_6: -0.671 codici/N, lineare da 0 a ~62 N
  - assiale: trazione/compressione lungo l'asse, come in macchina: macchina di trazione, test4-6:
    -0.520 codici/N fino a +-2000 N; + = trazione, - = compressione
--k sovrascrive la sensibilita' (codici ADC per N) per un altro montaggio.

Tara: all'avvio la forza viene azzerata sulla media del primo TARE_S secondi (braccetto scarico!);
il pulsante "Tara" (o Invio nel terminale con --no-plot) la rifa' sulla media dell'ultimo secondo.
La forza mostrata e' la media mobile degli ultimi --avg secondi (default 1 s): il rumore di una
singola misura e' ~7 N, la media su 1 s scende a circa 1 N.
"""

import argparse
import sys
import threading
import time
from collections import deque
from pathlib import Path

import can
import cantools

from gs_usb_bus import open_bus

# montaggio: (codici ADC per N, forza massima calibrata [N], descrizione)
SETUPS = {
    "morsa": (-0.671, 62.0, "morsa + pesi appesi (test-corretto-hydra-peso-morsa_6)"),
    "assiale": (-0.520, 2000.0, "assiale, macchina di trazione (test4-6): + trazione, - compressione"),
}
G = 9.81
TARE_S = 1.0
BITRATE = 1_000_000
MESSAGE_NAME = "ACQUISINATOR_HYDRA_ADC"
ADC_VREF_MV = 3290.0
ADC_FULL_SCALE = 65536
DEFAULT_DBC = Path(__file__).resolve().parent / "acquisinator_hydra_adc.dbc"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dbc", nargs="?", type=Path, default=DEFAULT_DBC, help="dbc dell'hydra")
    parser.add_argument("--setup", choices=SETUPS, default="morsa", help="calibrazione da usare (default: morsa)")
    parser.add_argument("--k", type=float, default=None, metavar="COD_PER_N",
                        help="sensibilita' in codici ADC per N, sovrascrive quella di --setup")
    parser.add_argument("--avg", type=float, default=1.0, metavar="S", help="media mobile mostrata, secondi (default: 1)")
    parser.add_argument("--no-plot", action="store_true", help="solo terminale")
    return parser.parse_args()


class Meter:
    """Ultimi campioni (tempo, codice), tara e conversione in forza."""

    def __init__(self, k: float, avg_s: float) -> None:
        self.k = k
        self.avg_s = avg_s
        self.valid_max = float("inf")  # forza massima calibrata, impostata da main
        self.lock = threading.Lock()
        self.samples: deque = deque()  # (monotonic, codice) dell'ultimo max(avg_s, 1 s)
        self.history: deque = deque(maxlen=20000)  # (monotonic, forza) per il grafico
        self.zero: float | None = None
        self.t_start: float | None = None
        self.tare_buf: list[float] = []

    def add(self, code: float) -> None:
        now = time.monotonic()
        with self.lock:
            if self.zero is None:
                self.t_start = self.t_start or now
                self.tare_buf.append(code)
                if now - self.t_start >= TARE_S:
                    self.zero = sum(self.tare_buf) / len(self.tare_buf)
                    print(f"tara: {self.zero:.1f} codici ({len(self.tare_buf)} misure)")
            self.samples.append((now, code))
            keep = max(self.avg_s, 1.0)
            while self.samples and self.samples[0][0] < now - keep:
                self.samples.popleft()
            if self.zero is not None:
                self.history.append((now, (code - self.zero) / self.k))

    def mean_code(self, seconds: float) -> float | None:
        with self.lock:
            if not self.samples:
                return None
            t_last = self.samples[-1][0]
            recent = [c for t, c in self.samples if t >= t_last - seconds]
        return sum(recent) / len(recent)

    def force(self) -> float | None:
        code = self.mean_code(self.avg_s)
        if code is None or self.zero is None:
            return None
        return (code - self.zero) / self.k

    def tare(self) -> None:
        code = self.mean_code(1.0)
        if code is not None:
            with self.lock:
                self.zero = code
            print(f"tara: {code:.1f} codici")


def read_frames(bus: can.BusABC, msg, meter: Meter, stop: threading.Event) -> None:
    while not stop.is_set():
        frame = bus.recv(timeout=0.1)
        if frame is None or frame.is_error_frame or frame.arbitration_id != msg.frame_id:
            continue
        data = msg.decode(frame.data)
        meter.add(data["voltage"] * ADC_FULL_SCALE / ADC_VREF_MV)  # media non arrotondata, in codici


def describe(force: float | None, valid_max: float) -> str:
    if force is None:
        return "tara in corso..."
    warn = "  (fuori calibrazione)" if abs(force) > valid_max else ""
    return f"{force:+7.1f} N   {force / G:+6.2f} kg{warn}"


def run_terminal(meter: Meter, stop: threading.Event) -> None:
    print("Invio = tara, Ctrl+C per uscire")

    def keys():
        while not stop.is_set():
            if sys.stdin.readline() == "":
                return
            meter.tare()

    threading.Thread(target=keys, daemon=True).start()
    while not stop.is_set():
        print(f"\r{describe(meter.force(), meter.valid_max)}      ", end="", flush=True)
        time.sleep(0.2)


def run_plot(meter: Meter) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import Button

    fig = plt.figure(figsize=(8, 5.5))
    big = fig.text(0.5, 0.80, "tara in corso...", ha="center", va="center", fontsize=40, family="monospace")
    sub = fig.text(0.5, 0.67, "", ha="center", va="center", fontsize=11, color="0.4")
    ax = fig.add_axes((0.12, 0.17, 0.83, 0.42))
    ax.set_xlabel("ultimi 30 s")
    ax.set_ylabel("forza [N]")
    ax.grid(alpha=0.4)
    (line,) = ax.plot([], [], lw=0.6, alpha=0.5)
    (avg_line,) = ax.plot([], [], lw=1.8, color=line.get_color())
    button = Button(fig.add_axes((0.42, 0.03, 0.16, 0.07)), "Tara")
    button.on_clicked(lambda _event: meter.tare())

    def update(_):
        force = meter.force()
        if force is None:
            return
        big.set_text(f"{force:+.1f} N")
        warn = f"   FUORI CALIBRAZIONE (±{meter.valid_max:g} N)" if abs(force) > meter.valid_max else ""
        sub.set_text(f"{force / G:+.2f} kg   |   media {meter.avg_s:g} s   |   {meter.k:+.3f} cod/N{warn}")
        with meter.lock:
            hist = list(meter.history)
        if not hist:
            return
        t_last = hist[-1][0]
        xs = [t - t_last for t, _ in hist if t >= t_last - 30]
        ys = [f for t, f in hist if t >= t_last - 30]
        n = max(1, int(len(ys) / 30 * meter.avg_s))  # campioni in avg_s secondi
        avg = [sum(ys[max(0, i - n + 1): i + 1]) / len(ys[max(0, i - n + 1): i + 1]) for i in range(len(ys))]
        line.set_data(xs, ys)
        avg_line.set_data(xs, avg)
        ax.set_xlim(-30, 0)
        lo, hi = min(avg), max(avg)
        pad = max(10.0, (hi - lo) * 0.2)
        ax.set_ylim(lo - pad, hi + pad)
        fig.canvas.draw_idle()

    # tenere il riferimento: senza, l'animazione viene raccolta dal GC e si ferma
    anim = FuncAnimation(fig, update, interval=100, cache_frame_data=False)  # noqa: F841
    plt.show()


def main() -> None:
    args = parse_args()
    msg = cantools.database.load_file(args.dbc).get_message_by_name(MESSAGE_NAME)
    try:
        bus = open_bus(BITRATE)
    except can.CanInitializationError as error:
        sys.exit(str(error))
    k_setup, valid_max, label = SETUPS[args.setup]
    meter = Meter(args.k if args.k is not None else k_setup, args.avg)
    meter.valid_max = valid_max
    stop = threading.Event()
    reader = threading.Thread(target=read_frames, args=(bus, msg, meter, stop), daemon=True)
    reader.start()
    print(f"calibrazione {args.setup}: {meter.k:+.3f} codici/N, valida fino a {valid_max:g} N ({label})")
    print(f"tara sul primo {TARE_S:g} s: braccetto scarico!")
    try:
        if args.no_plot:
            run_terminal(meter, stop)
        else:
            run_plot(meter)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        reader.join(timeout=1.0)
        bus.shutdown()
        print()


if __name__ == "__main__":
    main()
