#!/usr/bin/env python3
"""Analizza una sessione di read_link_deformation.py: ADC nel tempo e curva forza -> codice ADC.

Uso: python analyze_session.py sessions/test4.csv [--log sessions/test4.log] [--out PNG] [--no-show]

- Outlier: filtro di Hampel (mediana mobile +- HAMPEL_K * MAD) su adc_raw e adc_filtered;
  i campioni scartati vengono contati e non entrano nei grafici ne' nei calcoli.
- Step di carico: i plateau vengono trovati nel segnale (segmentazione a tratti costanti),
  non dai tempi del log: la forza veniva scritta a carico gia' applicato, a volte uno step
  dopo. Ogni plateau prende la forza dell'ultimo evento "forza X N" del .log scritto al suo
  interno; uno step mai scritto nel log viene dedotto dalla griglia di --step N (default 200)
  se sta tra due plateau distanti due step; i plateau restanti senza forza sono rampe di
  carico o scarico e vengono esclusi. Di ogni plateau si scartano PLATEAU_TRIM_S ai bordi.
- Inizio: la parte prima del primo evento del log viene tolta, il grafico nel tempo parte
  START_MARGIN_S prima del primo cambio di forza. Il plateau a 0 N iniziale resta come
  riferimento a vuoto nella curva forza -> codice.
- Curva: mediana di adc_filtered per step contro la forza (trazione N > 0 o compressione
  N < 0: carico/scarico si decide sul modulo), separando carico e scarico, con
  retta ai minimi quadrati sul carico (codici/N, R^2) e scarto di ogni punto dalla retta.
"""

import argparse
import csv
import re
from datetime import datetime
from pathlib import Path

import numpy as np

HAMPEL_WINDOW_S = 0.5  # finestra della mediana mobile per gli outlier
HAMPEL_K = 5.0  # soglia in MAD (scalata a sigma)
SMOOTH_S = 1.0  # mediana mobile per il livello del segnale
PLATEAU_BIN_S = 0.5  # blocchi su cui si fa la mediana prima della segmentazione
PLATEAU_MIN_S = 4.0  # durata minima di un plateau
PLATEAU_PENALTY = 10.0  # penalita' per plateau, in sigma^2 * log(n): piu' alta = meno plateau
PLATEAU_TRIM_S = 1.0  # secondi scartati a inizio e fine di ogni plateau (bordi del gradino)
DEFAULT_STEP_N = 200.0
START_MARGIN_S = 3.0  # secondi mostrati prima della prima variazione
ADC_VREF_NOMINAL_MV = 3290.0
ADC_FULL_SCALE = 65536


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", type=Path, help="csv della sessione")
    parser.add_argument("--log", type=Path, default=None, help="log della sessione (default: stesso nome .log)")
    parser.add_argument("--out", type=Path, default=None, help="immagine salvata (default: <csv>_analisi.png)")
    parser.add_argument("--no-show", action="store_true", help="salva soltanto, senza aprire la finestra")
    parser.add_argument("--step", type=float, default=DEFAULT_STEP_N, metavar="N",
                        help=f"ampiezza degli step di carico, per dedurre quelli non scritti nel log (default: {DEFAULT_STEP_N:g})")
    return parser.parse_args()


def load_csv(path: Path) -> dict[str, np.ndarray]:
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    return {
        "wall": np.array([datetime.fromisoformat(r["wall_time"]).timestamp() for r in rows]),
        "t": np.array([float(r["t_s"]) for r in rows]),
        "raw": np.array([float(r["adc_raw"]) for r in rows]),
        "filt": np.array([float(r["adc_filtered"]) for r in rows]),
    }


def load_force_events(path: Path) -> list[tuple[float, float]]:
    """(timestamp PC, forza N) dagli eventi del log."""
    events = []
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\S+)\s+forza\s+([-+0-9.eE]+)\s*N", line)
        if m:
            events.append((datetime.fromisoformat(m.group(1)).timestamp(), float(m.group(2))))
    return events


def rolling_median(t: np.ndarray, y: np.ndarray, window_s: float) -> np.ndarray:
    """Mediana mobile centrata su una finestra in secondi (campioni non equispaziati)."""
    half = window_s / 2
    lo = np.searchsorted(t, t - half, side="left")
    hi = np.searchsorted(t, t + half, side="right")
    return np.array([np.median(y[a:b]) for a, b in zip(lo, hi)])


def hampel(t: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Maschera dei campioni buoni: |y - mediana mobile| <= HAMPEL_K * sigma robusta locale."""
    med = rolling_median(t, y, HAMPEL_WINDOW_S)
    dev = np.abs(y - med)
    # sigma robusta globale dal MAD: quella locale su 0.5 s e' troppo instabile con pochi campioni
    sigma = 1.4826 * np.median(dev)
    return dev <= HAMPEL_K * max(sigma, 1.0)


def find_plateaus(t: np.ndarray, y: np.ndarray, t_begin: float) -> list[tuple[float, float, float]]:
    """Tratti a livello costante (t0, t1, livello) da t_begin in poi.

    Segmentazione ottima ai minimi quadrati (programmazione dinamica con penalita' per tratto)
    sulle mediane a blocchi di PLATEAU_BIN_S, tratti lunghi almeno PLATEAU_MIN_S.
    """
    edges = np.arange(t_begin, t[-1] + PLATEAU_BIN_S, PLATEAU_BIN_S)
    which = np.digitize(t, edges) - 1
    y_bin = np.array([np.median(y[which == k]) if np.any(which == k) else np.nan for k in range(len(edges) - 1)])
    good = np.isfinite(y_bin)
    y_bin = np.interp(np.arange(len(y_bin)), np.nonzero(good)[0], y_bin[good])
    n = len(y_bin)
    sigma = 1.4826 * np.median(np.abs(np.diff(y_bin))) / np.sqrt(2)  # rumore tra blocchi vicini
    beta = PLATEAU_PENALTY * sigma**2 * np.log(n)
    min_len = int(round(PLATEAU_MIN_S / PLATEAU_BIN_S))
    c1 = np.concatenate([[0.0], np.cumsum(y_bin)])
    c2 = np.concatenate([[0.0], np.cumsum(y_bin * y_bin)])
    best = np.full(n + 1, np.inf)
    best[0] = -beta
    prev = np.zeros(n + 1, dtype=int)
    for j in range(min_len, n + 1):
        i = np.arange(0, j - min_len + 1)
        i = i[np.isfinite(best[i])]
        s = c1[j] - c1[i]
        cost = best[i] + (c2[j] - c2[i]) - s * s / (j - i) + beta
        k = np.argmin(cost)
        best[j], prev[j] = cost[k], i[k]
    cuts = [n]
    while cuts[-1] > 0:
        cuts.append(prev[cuts[-1]])
    cuts = cuts[::-1]
    return [(edges[a], edges[b], float(np.median(y_bin[a:b]))) for a, b in zip(cuts[:-1], cuts[1:])]


def label_plateaus(plateaus, events, step_n: float):
    """Assegna una forza a ogni plateau; ritorna (step, transizioni escluse).

    - La forza viene scritta nel log mentre il carico e' gia' applicato, spesso con qualche
      secondo di ritardo: un plateau prende la forza dell'ultimo evento scritto al suo interno.
    - Plateau vicini con forze entro meta' step sono lo stesso carico (es. 2000 N che si
      rilassa a 1967 N): vengono uniti, con la forza del tratto piu' lungo.
    - Un plateau senza evento compreso tra due plateau distanti esattamente due step (es. 0 e
      -400 N) e' uno step non scritto nel log: prende la forza di mezzo. Altrimenti e' una
      transizione (rampa di carico o scarico) ed e' escluso.
    """
    items = []
    for t0, t1, lvl in plateaus:
        inside = [force for t_ev, force in events if t0 <= t_ev < t1]
        items.append({"t0": t0, "t1": t1, "level": lvl, "force": inside[-1] if inside else None, "origin": "log"})

    merged = []
    for it in items:
        last = merged[-1] if merged else None
        if last and it["force"] is not None and last["force"] is not None and abs(it["force"] - last["force"]) < step_n / 2:
            if it["t1"] - it["t0"] > last["t1"] - last["t0"]:
                last["force"] = it["force"]
            last["level"] = (last["level"] + it["level"]) / 2
            last["t1"] = it["t1"]
        else:
            merged.append(dict(it))

    steps, transitions = [], []
    for i, it in enumerate(merged):
        if it["force"] is not None:
            steps.append(it)
            continue
        before = merged[i - 1] if i > 0 else None
        after = merged[i + 1] if i + 1 < len(merged) else None
        if (before and after and before["force"] is not None and after["force"] is not None
                and abs(abs(after["force"] - before["force"]) - 2 * step_n) < step_n / 2
                and min(before["level"], after["level"]) < it["level"] < max(before["level"], after["level"])):
            it["force"] = (before["force"] + after["force"]) / 2
            it["origin"] = f"dedotto (griglia {step_n:g} N)"
            steps.append(it)
        else:
            transitions.append((it["t0"], it["t1"]))
    return steps, transitions


def main() -> None:
    args = parse_args()
    log_path = args.log or args.csv.with_suffix(".log")
    out_path = args.out or args.csv.with_name(args.csv.stem + "_analisi.png")
    d = load_csv(args.csv)
    t, raw, filt = d["t"], d["raw"], d["filt"]
    # wall_time del csv -> t_s: serve per mettere gli eventi del log sull'asse dei tempi
    wall_to_t = np.median(d["wall"] - t)
    events = [(w - wall_to_t, force) for w, force in load_force_events(log_path)]
    if not events:
        raise SystemExit(f"nessun evento 'forza' in {log_path}")

    ok_raw, ok_filt = hampel(t, raw), hampel(t, filt)
    print(f"outlier scartati: adc_raw {np.sum(~ok_raw)} / {len(t)}, adc_filtered {np.sum(~ok_filt)} / {len(t)}")
    tf, ff = t[ok_filt], filt[ok_filt]
    level = rolling_median(tf, ff, SMOOTH_S)

    plateaus = find_plateaus(tf, ff, events[0][0])
    steps, transitions = label_plateaus(plateaus, events, args.step)
    for s in steps:
        m = (tf >= s["t0"] + PLATEAU_TRIM_S) & (tf < s["t1"] - PLATEAU_TRIM_S)
        values = ff[m] if np.sum(m) >= 10 else ff[(tf >= s["t0"]) & (tf < s["t1"])]
        s.update(median=np.median(values), std=np.std(values), n=len(values))
    peak_force = 0.0  # in modulo: vale sia per trazione (N > 0) sia per compressione (N < 0)
    for s in steps:
        s["unloading"] = abs(s["force"]) < peak_force
        peak_force = max(peak_force, abs(s["force"]))

    # prima variazione: il primo plateau con una forza diversa da quella iniziale
    changes = [s["t0"] for s in steps if s["force"] != steps[0]["force"]]
    t_first_change = changes[0] if changes else steps[0]["t0"]
    t_start = max(steps[0]["t0"], t_first_change - START_MARGIN_S)
    print(f"prima variazione a t={t_first_change:.1f} s: il grafico parte da t={t_start:.1f} s")
    print(f"{'plateau [s]':>15} {'forza [N]':>10} {'origine':>22}")
    for s in steps:
        print(f"{s['t0']:7.1f}-{s['t1']:6.1f} {s['force']:10g} {s['origin']:>22}")
    for t0, t1 in transitions:
        print(f"{t0:7.1f}-{t1:6.1f} {'-':>10} {'transizione, esclusa':>22}")

    load = [s for s in steps if not s["unloading"]]
    fl = np.array([s["force"] for s in load])
    cl = np.array([s["median"] for s in load])
    slope, intercept = np.polyfit(fl, cl, 1)
    fit = slope * fl + intercept
    r2 = 1 - np.sum((cl - fit) ** 2) / np.sum((cl - cl.mean()) ** 2)
    mv_per_code = ADC_VREF_NOMINAL_MV / ADC_FULL_SCALE
    print(f"\ncarico: {slope:.4f} codici/N ({slope * mv_per_code * 1000:.2f} µV/N nominali), "
          f"{1 / slope:+.2f} N/codice, a vuoto {intercept:.1f} codici, R^2 {r2:.5f}")
    print(f"{'forza [N]':>10} {'fase':>8} {'mediana':>9} {'std':>7} {'n':>6} {'scarto [N]':>11}")
    for s in steps:
        resid_n = (s["median"] - (slope * s["force"] + intercept)) / slope
        mark = "*" if s["origin"] != "log" else " "
        print(f"{s['force']:9g}{mark} {'scarico' if s['unloading'] else 'carico':>8} {s['median']:9.1f}"
              f" {s['std']:7.1f} {s['n']:6d} {resid_n:+11.1f}")

    import matplotlib

    if args.no_show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig = plt.figure(figsize=(13, 9))
    gs = fig.add_gridspec(2, 2, height_ratios=(1.15, 1.0))
    ax_t = fig.add_subplot(gs[0, :])
    ax_c = fig.add_subplot(gs[1, 0])
    ax_r = fig.add_subplot(gs[1, 1])
    fig.suptitle(f"{args.csv.name}: outlier tolti (Hampel), plateau dal segnale, forze dal log", fontsize=11)

    # --- nel tempo
    view_raw = ok_raw & (t >= t_start)
    view = tf >= t_start
    ax_t.plot(t[view_raw], raw[view_raw], ",", color="0.7", label="adc_raw")
    ax_t.plot(tf[view], ff[view], linewidth=0.6, alpha=0.6, color="tab:blue", label="adc_filtered")
    ax_t.plot(tf[view], level[view], linewidth=1.8, color="tab:blue", label=f"mediana mobile {SMOOTH_S:g} s")
    cmap = plt.get_cmap("viridis")
    max_force = max(abs(s["force"]) for s in steps) or 1.0
    force_lo = min(s["force"] for s in steps)
    force_hi = max(s["force"] for s in steps)
    xaxis = ax_t.get_xaxis_transform()
    for s in steps:
        if s["t1"] <= t_start:
            continue  # plateau prima dell'inizio del grafico (resta nella curva forza -> codice)
        a, b = max(s["t0"], t_start), s["t1"]
        inferred = s["origin"] != "log"
        ax_t.axvspan(a, b, color=cmap(abs(s["force"]) / max_force), alpha=0.12, linewidth=0,
                     hatch="//" if inferred else None)
        ax_t.hlines(s["median"], max(s["t0"] + PLATEAU_TRIM_S, t_start), b - PLATEAU_TRIM_S, color="tab:red", linewidth=2)
        ax_t.text((a + b) / 2, 1.0, f"{s['force']:g}" + ("*" if inferred else ""), transform=xaxis,
                  ha="center", va="bottom", fontsize=8, color="tab:purple" if inferred else "black")
    for t0, t1 in transitions:
        if t1 > t_start:
            ax_t.axvspan(max(t0, t_start), t1, color="tab:orange", alpha=0.25, linewidth=0)
            ax_t.text((max(t0, t_start) + t1) / 2, 0.02, "transiz.", transform=xaxis, ha="center", fontsize=7,
                      color="tab:orange")
    # quando la forza e' stata scritta nel log: si vede il ritardo rispetto al gradino
    for i, (t_ev, force) in enumerate(events):
        if t_ev >= t_start:
            ax_t.axvline(t_ev, color="0.3", linestyle=":", linewidth=0.8,
                         label="forza scritta nel log" if i == len(events) - 1 else None)
    ax_t.set_xlim(t_start, tf[-1])
    lo, hi = np.percentile(ff[view], [0.5, 99.5])
    pad = (hi - lo) * 0.25
    ax_t.set_ylim(lo - pad, hi + pad)  # adc_raw e' molto piu' rumoroso: non deve dettare la scala
    ax_t.set_xlabel("tempo [s]")
    ax_t.set_ylabel("ADC [codice]")
    ax_t.text(0.0, 1.06, "forza [N] (* = step non scritto nel log, dedotto dalla griglia di "
              f"{args.step:g} N):", transform=ax_t.transAxes, fontsize=8)
    ax_t.grid(True, alpha=0.4)
    ax_t.legend(loc="lower left", fontsize=8, markerscale=20)
    sec = ax_t.secondary_yaxis("right", functions=(lambda c: c * mv_per_code, lambda v: v / mv_per_code))
    sec.set_ylabel("tensione nominale [mV]")

    # --- forza -> codice
    for phase, marker, color in (("carico", "o", "tab:blue"), ("scarico", "s", "tab:orange")):
        sel = [s for s in steps if s["unloading"] == (phase == "scarico")]
        if sel:
            ax_c.errorbar([s["force"] for s in sel], [s["median"] for s in sel], yerr=[s["std"] for s in sel],
                          fmt=marker, color=color, capsize=3, label=phase)
    xs = np.linspace(force_lo, force_hi, 2)
    ax_c.plot(xs, slope * xs + intercept, "--", color="tab:red",
              label=f"retta carico: {slope:.4f} cod/N, R² {r2:.4f}")
    ax_c.set_xlabel("forza [N]")
    ax_c.set_ylabel("adc_filtered, mediana dello step [codice]")
    ax_c.grid(True, alpha=0.4)
    ax_c.legend(fontsize=8)

    # --- scarti dalla retta, in N
    for phase, marker, color in (("carico", "o", "tab:blue"), ("scarico", "s", "tab:orange")):
        sel = [s for s in steps if s["unloading"] == (phase == "scarico")]
        if sel:
            ax_r.errorbar([s["force"] for s in sel],
                          [(s["median"] - (slope * s["force"] + intercept)) / slope for s in sel],
                          yerr=[s["std"] / abs(slope) for s in sel], fmt=marker, color=color, capsize=3, label=phase)
    ax_r.axhline(0, color="tab:red", linestyle="--")
    ax_r.set_xlabel("forza [N]")
    ax_r.set_ylabel("scarto dalla retta [N]")
    ax_r.grid(True, alpha=0.4)
    ax_r.legend(fontsize=8)

    fig.tight_layout()
    fig.savefig(out_path, dpi=130)
    print(f"\ngrafico salvato in {out_path}")
    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
