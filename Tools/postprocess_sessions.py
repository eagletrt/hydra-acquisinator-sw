#!/usr/bin/env python3
"""Post-processing di piu' sessioni: forza stimata dall'ADC, filtrata senza ritardo, contro la forza applicata.

Uso: python postprocess_sessions.py sessions/test4.csv sessions/test5.csv ... [--smooth S] [--out PNG] [--no-show]

Per ogni sessione (log con lo stesso nome .log):
  1. outlier tolti con il filtro di Hampel (come analyze_session.py)
  2. adc_filtered ricampionato su una griglia uniforme di GRID_S e filtrato con un nucleo
     gaussiano centrato (--smooth = sigma in secondi): filtro a fase zero, i gradini non
     vengono spostati nel tempo, solo arrotondati
  3. plateau e forze applicate come in analyze_session.py (forze dal log, step mancanti
     dedotti dalla griglia di --step N)
Calibrazione unica: retta ai minimi quadrati sui plateau di tutte le sessioni, con la pendenza
in comune e lo zero di ciascuna sessione (tara sul suo primo plateau a 0 N), come si userebbe
in macchina. La forza stimata e' (codice - zero della sessione) / pendenza.
Errore: dentro i plateau (senza i bordi) forza stimata filtrata meno forza applicata, rms e max.
"""

import argparse
from pathlib import Path

import numpy as np

import analyze_session as an

GRID_S = 0.01  # griglia uniforme del ricampionamento
DEFAULT_SMOOTH_S = 1.0  # sigma del nucleo gaussiano


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", type=Path, nargs="+", help="csv delle sessioni")
    parser.add_argument("--smooth", type=float, default=DEFAULT_SMOOTH_S, metavar="S",
                        help=f"sigma del filtro gaussiano in secondi (default: {DEFAULT_SMOOTH_S:g})")
    parser.add_argument("--step", type=float, default=an.DEFAULT_STEP_N, metavar="N", help="ampiezza degli step di carico")
    parser.add_argument("--out", type=Path, default=None, help="immagine (default: sessions/postprocess.png)")
    parser.add_argument("--no-show", action="store_true", help="salva soltanto, senza aprire la finestra")
    return parser.parse_args()


def gaussian_smooth(y: np.ndarray, sigma_samples: float) -> np.ndarray:
    """Filtro gaussiano centrato (fase zero), bordi riflessi per non piegare la curva agli estremi."""
    half = int(np.ceil(4 * sigma_samples))
    k = np.exp(-0.5 * (np.arange(-half, half + 1) / sigma_samples) ** 2)
    k /= k.sum()
    padded = np.pad(y, half, mode="reflect")
    return np.convolve(padded, k, mode="valid")


def load_session(path: Path, step_n: float, smooth_s: float) -> dict:
    d = an.load_csv(path)
    wall_to_t = np.median(d["wall"] - d["t"])
    events = [(w - wall_to_t, force) for w, force in an.load_force_events(path.with_suffix(".log"))]
    ok = an.hampel(d["t"], d["filt"])
    t, y = d["t"][ok], d["filt"][ok]
    steps, transitions = an.label_plateaus(an.find_plateaus(t, y, events[0][0]), events, step_n)
    for s in steps:
        m = (t >= s["t0"] + an.PLATEAU_TRIM_S) & (t < s["t1"] - an.PLATEAU_TRIM_S)
        s["median"] = float(np.median(y[m]))
    # da qui in poi solo dal primo plateau: la parte morta iniziale non serve
    keep = t >= steps[0]["t0"]
    t, y = t[keep], y[keep]
    grid = np.arange(t[0], t[-1], GRID_S)
    y_grid = np.interp(grid, t, y)
    return {
        "name": path.stem, "t": t, "y": y, "grid": grid,
        "y_smooth": gaussian_smooth(y_grid, smooth_s / GRID_S),
        "steps": steps, "transitions": transitions,
        "zero": next(s["median"] for s in steps if s["force"] == 0),
    }


def fit_common_slope(sessions: list[dict]) -> float:
    """Pendenza comune [codici/N] con uno zero per sessione: minimi quadrati su (codice - zero)."""
    forces, codes = [], []
    for s in sessions:
        for st in s["steps"]:
            forces.append(st["force"])
            codes.append(st["median"] - s["zero"])
    forces, codes = np.array(forces), np.array(codes)
    return float(np.sum(forces * codes) / np.sum(forces * forces))


def applied_force(s: dict, t: np.ndarray) -> np.ndarray:
    """Forza applicata nel tempo (NaN nelle transizioni e fuori dai plateau)."""
    f = np.full(len(t), np.nan)
    for st in s["steps"]:
        f[(t >= st["t0"]) & (t < st["t1"])] = st["force"]
    return f


def main() -> None:
    args = parse_args()
    sessions = [load_session(p, args.step, args.smooth) for p in args.csv]
    slope = fit_common_slope(sessions)
    print(f"pendenza comune: {slope:.4f} codici/N  ->  {1 / slope:+.3f} N/codice")

    import matplotlib

    if args.no_show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    n = len(sessions)
    fig = plt.figure(figsize=(15, 3.2 * n + 1))
    gs = fig.add_gridspec(n, 2, width_ratios=(2.2, 1.0))
    ax_cal = fig.add_subplot(gs[: max(1, n - 1), 1])
    ax_res = fig.add_subplot(gs[n - 1, 1]) if n > 1 else None
    fig.suptitle(f"Forza stimata dall'ADC: outlier tolti, filtro gaussiano a fase zero σ = {args.smooth:g} s, "
                 f"calibrazione comune {1 / slope:+.3f} N/codice", fontsize=11)

    all_f, all_res = [], []
    for row, s in enumerate(sessions):
        ax = fig.add_subplot(gs[row, 0])
        force_raw = (s["y"] - s["zero"]) / slope
        force_smooth = (s["y_smooth"] - s["zero"]) / slope
        ref = applied_force(s, s["grid"])

        # errore dentro i plateau, senza i bordi
        inside = np.zeros(len(s["grid"]), dtype=bool)
        for st in s["steps"]:
            inside |= (s["grid"] >= st["t0"] + an.PLATEAU_TRIM_S + args.smooth) & (s["grid"] < st["t1"] - an.PLATEAU_TRIM_S - args.smooth)
        err = (force_smooth - ref)[inside]
        rms, worst = np.sqrt(np.mean(err**2)), np.max(np.abs(err))
        print(f"{s['name']:>20}: zero {s['zero']:.0f} codici, errore filtrato nei plateau {rms:.1f} N rms, {worst:.1f} N max")

        t0 = s["grid"][0]
        ax.plot(s["t"] - t0, force_raw, ",", color="0.75", label="da adc_filtered (outlier tolti)")
        ax.plot(s["grid"] - t0, ref, color="black", linewidth=1.2, label="forza applicata (log / griglia)")
        ax.plot(s["grid"] - t0, force_smooth, color="tab:blue", linewidth=1.8, label="stimata, filtrata")
        for a, b in s["transitions"]:
            ax.axvspan(a - t0, b - t0, color="tab:orange", alpha=0.2, linewidth=0)
        lo, hi = np.nanmin(ref), np.nanmax(ref)
        pad = 0.15 * max(hi - lo, args.step)
        ax.set_ylim(lo - pad, hi + pad)
        ax.set_xlim(0, s["grid"][-1] - t0)
        ax.set_ylabel("forza [N]")
        ax.set_title(f"{s['name']}: errore nei plateau {rms:.0f} N rms, {worst:.0f} N max", loc="left", fontsize=10)
        ax.grid(True, alpha=0.4)
        if row == 0:
            ax.legend(loc="upper left", fontsize=8, markerscale=20)
        if row == n - 1:
            ax.set_xlabel("tempo [s]")

        f = np.array([st["force"] for st in s["steps"]])
        c = np.array([st["median"] - s["zero"] for st in s["steps"]])
        (pts,) = ax_cal.plot(f, c, "o", label=s["name"])
        if ax_res is not None:
            ax_res.plot(f, c / slope - f, "o", color=pts.get_color())
        all_f.append(f)
        all_res.append(c / slope - f)

    all_f = np.concatenate(all_f)
    xs = np.array([all_f.min(), all_f.max()])
    ax_cal.plot(xs, slope * xs, "--", color="tab:red", label=f"comune: {slope:.4f} cod/N")
    ax_cal.set_xlabel("forza applicata [N]")
    ax_cal.set_ylabel("codice - zero della sessione")
    ax_cal.set_title("calibrazione (mediane dei plateau)", fontsize=10)
    ax_cal.grid(True, alpha=0.4)
    ax_cal.legend(fontsize=8)
    if ax_res is not None:
        res = np.concatenate(all_res)
        ax_res.axhline(0, color="tab:red", linestyle="--")
        ax_res.set_xlabel("forza applicata [N]")
        ax_res.set_ylabel("scarto [N]")
        ax_res.set_title(f"scarti dei plateau: {np.sqrt(np.mean(res**2)):.0f} N rms, {np.max(np.abs(res)):.0f} N max",
                         fontsize=10)
        ax_res.grid(True, alpha=0.4)

    fig.tight_layout()
    out = args.out or args.csv[0].parent / "postprocess.png"
    fig.savefig(out, dpi=130)
    print(f"grafico salvato in {out}")
    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
