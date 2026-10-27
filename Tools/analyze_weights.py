#!/usr/bin/env python3
"""Analizza una prova a pesi (morsa + pesi appesi a mano) e stima i pesi non scritti nel log.

Uso: python analyze_weights.py sessions/test9-300hz-peso-morsa.csv [--out PNG] [--no-show]

- Tratti fermi: dove la dispersione su 1 s della tensione filtrata e' bassa; i tratti di
  manipolazione (appendere/togliere pesi, oscillazioni) vengono esclusi.
- Forze: ogni tratto fermo prende l'ultima forza del log scritta al suo interno, o al massimo
  LABEL_LOOKAHEAD_S dopo il suo inizio (la forza veniva scritta a peso gia' appeso). Tratti senza
  forza con livello uguale al vicino (entro THRESHOLDS) sono lo stesso peso.
- Legge sia il CSV dell'hydra (read_link_deformation.py) sia quello del lock-in (read_lockin.py).
- Livelli: ogni tratto viene riferito allo zero piu' vicino nel tempo (media dei tratti a 0 N prima
  e dopo, pesata sulla distanza), cosi' la deriva lenta pesa poco.
- Calibrazione: interpolazione lineare a tratti della curva forza -> variazione di codice, separata
  per carico e scarico, piu' una retta su tutti i punti.
- Tratti fermi senza forza e con livello diverso dallo zero sono pesi sconosciuti: per ognuno si
  stima la forza (e la massa, g = 9.81) con le tre curve, che danno anche l'incertezza.
"""

import argparse
import csv
from datetime import datetime
from pathlib import Path

import numpy as np

import analyze_session as an

G = 9.81
# soglie in codici, per scheda: il lock-in ha un segnale ~10 volte piu' piccolo per N (eccitazione 0.3 V)
LOCKIN_SMOOTH_S = 0.5  # media mobile sul segnale del lock-in prima dell'analisi
THRESHOLDS = {  # scheda: (dispersione su 1 s oltre la quale e' manipolazione, livelli "uguali")
    "hydra": (8.0, 6.0),
    "lock-in": (3.5, 1.0),
}
QUIET_MARGIN_S = 2.0  # margine tolto attorno alla manipolazione
QUIET_MIN_S = 5.0  # durata minima di un tratto fermo
LABEL_LOOKAHEAD_S = 15.0
ADC_VREF_MV = 3290.0
ADC_FULL_SCALE = 65536


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", type=Path, help="csv della sessione (log con lo stesso nome .log)")
    parser.add_argument("--out", type=Path, default=None, help="immagine (default: <csv>_pesi.png)")
    parser.add_argument("--no-show", action="store_true", help="salva soltanto, senza aprire la finestra")
    return parser.parse_args()


def quiet_intervals(t: np.ndarray, v: np.ndarray, quiet_std: float) -> tuple[list[tuple[float, float]], np.ndarray]:
    """Tratti fermi (t0, t1) e maschera della manipolazione."""
    fs = 1 / np.median(np.diff(t))
    win = max(3, int(round(fs)))  # 1 s
    step = max(1, win // 20)
    idx = np.arange(0, len(v), step)
    std1 = np.interp(np.arange(len(v)), idx, [v[max(0, i - win // 2): i + win // 2].std() for i in idx])
    margin = int(round(QUIET_MARGIN_S * fs))
    busy = np.convolve((std1 > quiet_std).astype(float), np.ones(2 * margin + 1), "same") > 0
    x = np.r_[0, (~busy).astype(int), 0]
    dx = np.diff(x)
    runs = zip(np.flatnonzero(dx == 1), np.flatnonzero(dx == -1))
    return [(t[a], t[b - 1]) for a, b in runs if t[b - 1] - t[a] >= QUIET_MIN_S], busy


def main() -> None:
    args = parse_args()
    out = args.out or args.csv.with_name(args.csv.stem + "_pesi.png")
    with open(args.csv, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    t = np.array([float(r["t_s"]) for r in rows])
    if "lockin_codes" in rows[0]:
        # lock-in (read_lockin.py): componente in fase in codici ADC; righe senza DIAG scartate
        board = "lock-in"
        keep = [i for i, r in enumerate(rows) if r["lockin_codes"]]
        t = t[keep]
        v = np.array([float(rows[i]["lockin_codes"]) for i in keep])
    else:
        # hydra (read_link_deformation.py): tensione filtrata dal firmware, non arrotondata, in codici
        board = "hydra"
        v = np.array([float(r["voltage_fw_mV"]) for r in rows]) * ADC_FULL_SCALE / ADC_VREF_MV
    quiet_std, same_level = THRESHOLDS[board]
    if board == "lock-in":
        # picchi isolati via (Hampel), poi media mobile centrata di LOCKIN_SMOOTH_S: il rumore del
        # lock-in e' soprattutto un tono a ~13 Hz, che cosi' scende di ~20 volte senza spostare i livelli
        ok = an.hampel(t, v)
        print(f"lock-in: {np.sum(~ok)} picchi isolati tolti, media mobile di {LOCKIN_SMOOTH_S:g} s")
        t, v = t[ok], v[ok]
        keep = np.asarray(keep)[ok]
        n = max(1, int(round(LOCKIN_SMOOTH_S / np.median(np.diff(t)))))
        v = np.convolve(np.pad(v, n // 2, mode="edge"), np.ones(n) / n, mode="same")[n // 2: n // 2 + len(v)]
    print(f"scheda: {board}, soglie: manipolazione > {quiet_std} codici, stesso livello < {same_level} codici")
    wall = np.array([datetime.fromisoformat(r["wall_time"]).timestamp() for r in rows])
    if board == "lock-in":
        wall = wall[keep]
    wall_to_t = np.median(wall - t)  # ora del PC -> t_s, per mettere gli eventi del log sull'asse dei tempi
    events = [(w - wall_to_t, f) for w, f in an.load_force_events(args.csv.with_suffix(".log"))]

    quiet, busy = quiet_intervals(t, v, quiet_std)
    segs = []
    for a, b in quiet:
        m = (t >= a) & (t <= b)
        segs.append({"a": a, "b": b, "lvl": float(np.median(v[m])), "std": float(v[m].std()), "F": None, "why": ""})
    for te, f in events:
        cand = [s for s in segs if s["a"] <= te <= s["b"]] or [s for s in segs if te < s["a"] <= te + LABEL_LOOKAHEAD_S]
        if cand:
            cand[0]["F"], cand[0]["why"] = f, "log"
    for order in (segs, segs[::-1]):
        for s0, s1 in zip(order, order[1:]):
            if s1["F"] is None and s0["F"] is not None and abs(s1["lvl"] - s0["lvl"]) < same_level:
                s1["F"], s1["why"] = s0["F"], "stesso livello"
    first_event = events[0][0] if events else t[0]

    # zero locale: tratti a 0 N prima e dopo, pesati sulla distanza nel tempo
    zeros = [s for s in segs if s["F"] == 0]
    if not zeros:
        raise SystemExit("nessun tratto a 0 N: serve almeno uno zero")

    def local_zero(s) -> float:
        mid = (s["a"] + s["b"]) / 2
        before = [z for z in zeros if z["b"] <= s["a"]]
        after = [z for z in zeros if z["a"] >= s["b"]]
        if before and after:
            zb, za = before[-1], after[0]
            db, da = mid - (zb["a"] + zb["b"]) / 2, (za["a"] + za["b"]) / 2 - mid
            return (zb["lvl"] * da + za["lvl"] * db) / (da + db)
        return (before[-1] if before else after[0])["lvl"]

    peak = 0.0
    for s in segs:
        s["delta"] = s["lvl"] - local_zero(s)
        if s["F"] is not None:
            s["phase"] = "scarico" if abs(s["F"]) < peak else "carico"
            peak = max(peak, abs(s["F"]))
        print(f"{s['a']:6.1f}-{s['b']:6.1f} s  livello {s['lvl']:8.1f}  Δ {s['delta']:+7.1f}  rumore {s['std']:4.1f}"
              f"  forza {'?' if s['F'] is None else format(s['F'], 'g')} {s['why']}")

    # un punto per forza e fase (media dei tratti, pesata sulla durata)
    pts = {}
    for s in segs:
        if s["F"] is None or (s["F"] == 0):
            continue
        pts.setdefault((s["phase"], s["F"]), []).append(s)
    curve = {"carico": [(0.0, 0.0)], "scarico": [(0.0, 0.0)]}
    for (phase, f), ss in sorted(pts.items(), key=lambda kv: kv[0][1]):
        w = np.array([x["b"] - x["a"] for x in ss])
        curve[phase].append((f, float(np.sum(w * [x["delta"] for x in ss]) / w.sum())))
    all_f = np.array([f for c in curve.values() for f, _ in c])
    all_d = np.array([dd for c in curve.values() for _, dd in c])
    k = float(np.sum(all_f * all_d) / np.sum(all_f * all_f))  # retta per lo zero
    print(f"\nretta su tutti i punti: {k:.3f} codici/N ({1 / k:+.3f} N/codice)")
    for phase in ("carico", "scarico"):
        c = sorted(curve[phase])
        print(f"{phase}: " + ", ".join(f"{f:g} N -> {dd:+.1f}" for f, dd in c))

    def from_curve(phase: str, delta: float) -> float:
        c = sorted(curve[phase])
        f, dd = np.array([x[0] for x in c]), np.array([x[1] for x in c])
        order = np.argsort(dd)  # interp vuole le x (variazioni di codice) crescenti
        return float(np.interp(delta, dd[order], f[order], left=np.nan, right=np.nan))

    unknown = [s for s in segs if s["F"] is None and s["a"] > first_event and abs(s["delta"]) > same_level]
    results = []
    for s in unknown:
        est = {"retta": s["delta"] / k, "carico": from_curve("carico", s["delta"]), "scarico": from_curve("scarico", s["delta"])}
        vals = np.array([x for x in est.values() if np.isfinite(x)])
        results.append((s, est, vals))
        print(f"\npeso sconosciuto {s['a']:.0f}-{s['b']:.0f} s: Δ {s['delta']:+.1f} codici")
        for name, f in est.items():
            print(f"  {name:8s}: {f:7.1f} N = {f / G:6.2f} kg" if np.isfinite(f) else f"  {name:8s}: fuori dalla curva")
        print(f"  => {vals.mean():.0f} N, {vals.mean() / G:.1f} kg (intervallo {vals.min() / G:.1f}-{vals.max() / G:.1f} kg)")

    import matplotlib

    if args.no_show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(2, 1, figsize=(13, 9), gridspec_kw={"height_ratios": (1.3, 1)})
    ax[0].plot(t, v, lw=0.5, color="tab:blue", label=f"{board}: segnale [codici ADC]")
    lo, hi = np.percentile([s["lvl"] for s in segs], [0, 100])
    ax[0].fill_between(t, lo - 80, hi + 80, where=busy, color="tab:orange", alpha=0.15, label="manipolazione (esclusa)")
    for s in segs:
        unk = s["F"] is None and s in unknown
        c = "tab:purple" if unk else ("tab:red" if s["F"] is not None else "0.5")
        ax[0].hlines(s["lvl"], s["a"], s["b"], color=c, lw=2.5)
        label = "?" if s["F"] is None else f"{s['F']:g} N"
        ax[0].text((s["a"] + s["b"]) / 2, s["lvl"] + 8, label, ha="center", fontsize=8, color=c)
    for te, _ in events:
        ax[0].axvline(te, color="0.3", ls=":", lw=0.8)
    ax[0].set_ylim(lo - 80, hi + 80)
    ax[0].set_xlim(t[0], t[-1])
    ax[0].set_xlabel("tempo [s]")
    ax[0].set_ylabel("codice ADC")
    ax[0].set_title(f"{args.csv.name} (punteggiate = forza scritta nel log, viola = peso sconosciuto)", fontsize=10)
    ax[0].legend(fontsize=8, loc="lower left")
    ax[0].grid(alpha=0.4)

    for phase, mk, col in (("carico", "o-", "tab:blue"), ("scarico", "s-", "tab:orange")):
        c = sorted(curve[phase])
        ax[1].plot([x[0] for x in c], [x[1] for x in c], mk, color=col, label=phase)
    xs = np.linspace(0, max(all_f.max(), max((r[2].max() for r in results), default=0)) * 1.05, 2)
    ax[1].plot(xs, k * xs, "--", color="tab:red", label=f"retta su tutti: {k:.2f} cod/N")
    for s, est, vals in results:
        ax[1].axhline(s["delta"], color="tab:purple", lw=0.8)
        ax[1].axvspan(vals.min(), vals.max(), color="tab:purple", alpha=0.15)
        ax[1].text(vals.mean(), s["delta"], f"  ? ≈ {vals.mean():.0f} N ≈ {vals.mean() / G:.1f} kg", color="tab:purple",
                   va="bottom", fontsize=9)
    ax[1].set_xlabel("forza [N]")
    ax[1].set_ylabel("codice − zero locale")
    ax[1].grid(alpha=0.4)
    ax[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print(f"\ngrafico salvato in {out}")
    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
