#!/usr/bin/env python3
"""
Analysis for the 5G mid-band conflict-density study. Reads
results/cd-runs.csv and results/cd-intents.csv (from run_study.py) and
writes, to results/analysis/:

  cd-summary.csv / table_cd.tex          conflict density per configuration
  fig_probability.png                    P(conflict) vs density, single + multi
  fig_qos_probability.png                multi: baseline vs QoS-protected
  fig_first_failure.png                  first-failing intent per configuration
  first-failure.csv                      intent + SLA term percentages
  criteria.csv / fig_criteria.png        CD vs conflict criterion (multi baseline)
  table_qos.tex                          headline baseline-vs-QoS comparison

Usage: python3 scratch/v2x-conflict-5g/validation/analyze.py
"""

import os
import re

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy import stats

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)
IN = os.path.join(PKG, "results")
OUT = os.path.join(IN, "analysis")

# (scenario, qos, label, colour)
CONFIGS = [
    ("single", 0, "all remote driving", "#d62728"),
    ("multi", 0, "mixed intents (baseline)", "#1f77b4"),
    ("multi", 1, "mixed intents (QoS-protected)", "#2ca02c"),
]
CRITERIA = {"A: 1 vehicle": None, "B: 5%": 0.05, "C: 10%": 0.10, "D: 25%": 0.25}


def ci95_half(x):
    x = np.asarray([v for v in x if not np.isnan(v)], float)
    if len(x) < 2:
        return np.nan
    return stats.t.ppf(0.975, len(x) - 1) * x.std(ddof=1) / np.sqrt(len(x))


def style(ax, xl, yl):
    ax.set_xlabel(xl); ax.set_ylabel(yl); ax.grid(True, alpha=0.3)
    ax.spines["top"].set_visible(False); ax.spines["right"].set_visible(False)


def tex_table(path, header, rows, caption, label):
    esc = lambda c: re.sub(r"(?<!\\)%", r"\\%", str(c))
    lines = [r"\begin{table}[t]", r"\centering\small", rf"\caption{{{caption}}}",
             rf"\label{{{label}}}", rf"\begin{{tabular}}{{{'l' * len(header)}}}",
             r"\toprule", " & ".join(header) + r" \\", r"\midrule"]
    lines += [" & ".join(esc(c) for c in r) + r" \\" for r in rows]
    lines += [r"\bottomrule", r"\end{tabular}", r"\end{table}"]
    open(path, "w").write("\n".join(lines) + "\n")


def load():
    runs = pd.read_csv(os.path.join(IN, "cd-runs.csv")).drop_duplicates(
        ["scenario", "qos", "seed", "numVehicles"])
    intents = pd.read_csv(os.path.join(IN, "cd-intents.csv")).drop_duplicates(
        ["scenario", "qos", "seed", "numVehicles", "intent"])
    frac = intents.groupby(["scenario", "qos", "seed", "numVehicles"]).apply(
        lambda d: d["violatingFlows"].sum() / d["nFlows"].sum(),
        include_groups=False).rename("violFraction").reset_index()
    runs = runs.merge(frac, on=["scenario", "qos", "seed", "numVehicles"], how="left")
    runs.loc[runs["crashed"] == 1, "violFraction"] = 1.0
    runs["violFraction"] = runs["violFraction"].fillna(0.0)
    return runs, intents


def per_seed_cd(runs, scen, qos):
    d0 = runs[(runs.scenario == scen) & (runs.qos == qos)]
    cds = []
    for seed, d in d0.groupby("seed"):
        br = d[d["breach"] == 1]
        if not br.empty:
            cds.append(int(br["numVehicles"].min()))
    return np.array(cds)


def prob_curve(runs, scen, qos):
    d = runs[(runs.scenario == scen) & (runs.qos == qos)]
    return d.groupby("numVehicles")["breach"].agg(["mean", "count"]).reset_index()


def wilson(ax, d, col):
    n, p, z = d["count"], d["mean"], 1.96
    den = 1 + z**2 / n
    c = (p + z**2 / (2 * n)) / den
    hw = z * np.sqrt(p * (1 - p) / n + z**2 / (4 * n**2)) / den
    ax.fill_between(d["numVehicles"], c - hw, c + hw, color=col, alpha=0.15)


def first_fail(intents, scen, qos, cds_by_seed):
    intent_share, sla_share = [], []
    for seed, cd in cds_by_seed.items():
        sel = intents[(intents.scenario == scen) & (intents.qos == qos) &
                      (intents.seed == seed) & (intents.numVehicles == cd)]
        viol = sel[sel["violatingFlows"] > 0]
        if viol.empty:
            intent_share.append("crash-overload"); sla_share.append("crash-overload")
            continue
        intent_share.append(viol.groupby("intent")["violatingFlows"].sum().idxmax())
        t = {"latency": viol["violDelay"].sum(), "throughput": viol["violThr"].sum(),
             "loss": viol["violLoss"].sum()}
        sla_share.append(max(t, key=t.get))
    return (pd.Series(intent_share).value_counts(normalize=True) * 100,
            pd.Series(sla_share).value_counts(normalize=True) * 100)


def main():
    os.makedirs(OUT, exist_ok=True)
    runs, intents = load()

    # ---- CD summary ----
    rows, summ = [], []
    for scen, qos, label, _ in CONFIGS:
        v = per_seed_cd(runs, scen, qos)
        if len(v) == 0:
            continue
        h = ci95_half(v)
        summ.append({"config": label, "scenario": scen, "qos": qos, "nSeeds": len(v),
                     "meanCD": v.mean(), "stdCD": v.std(ddof=1),
                     "minCD": int(v.min()), "maxCD": int(v.max()), "confidence95": h})
        rows.append([label, len(v), f"{v.mean():.1f} $\\pm$ {h:.1f}",
                     f"{v.std(ddof=1):.2f}", int(v.min()), int(v.max())])
    pd.DataFrame(summ).to_csv(os.path.join(OUT, "cd-summary.csv"), index=False)
    tex_table(os.path.join(OUT, "table_cd.tex"),
              ["Configuration", "Seeds", "CD (mean $\\pm$ 95\\% CI)", "SD", "Min", "Max"],
              rows, "Conflict density per configuration (5G mid-band, "
              "vehicles per 500\\,m).", "tab:cd")
    print(pd.DataFrame(summ).to_string(index=False))

    # ---- baseline probability curve (single + multi) ----
    fig, ax = plt.subplots(figsize=(7, 4.2))
    for scen, qos, label, col in CONFIGS:
        if qos:
            continue
        d = prob_curve(runs, scen, qos).sort_values("numVehicles")
        ax.plot(d["numVehicles"], d["mean"], "o-", color=col, lw=2, label=label)
        wilson(ax, d, col)
    ax.axhline(0.5, color="gray", ls=":", lw=1)
    style(ax, "vehicle density [vehicles / 500 m]", "P(conflict)")
    ax.set_ylim(-0.04, 1.04)
    ax.set_title("Conflict probability vs vehicle density (5G mid-band)", fontsize=10)
    ax.legend(fontsize=9)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "fig_probability.png"), dpi=200)
    plt.close(fig)

    # ---- QoS comparison probability (multi baseline vs QoS) ----
    fig, ax = plt.subplots(figsize=(7, 4.2))
    for qos, label, col in [(0, "baseline (proportional fair)", "#1f77b4"),
                            (1, "QoS-protected", "#2ca02c")]:
        d = prob_curve(runs, "multi", qos).sort_values("numVehicles")
        if d.empty:
            continue
        ax.plot(d["numVehicles"], d["mean"], "o-", color=col, lw=2, label=label)
        wilson(ax, d, col)
    style(ax, "vehicle density [vehicles / 500 m]", "P(conflict)")
    ax.set_ylim(-0.04, 1.04)
    ax.set_title("Effect of QoS-protected scheduling (mixed intents)", fontsize=10)
    ax.legend(fontsize=9)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "fig_qos_probability.png"), dpi=200)
    plt.close(fig)

    # ---- first-failure analysis ----
    ff_rows = []
    intent_panels = {}
    for scen, qos, label, _ in CONFIGS:
        v = runs[(runs.scenario == scen) & (runs.qos == qos)]
        cds = {s: int(per_seed_cd(v[v.seed == s], scen, qos)[0])
               for s in v["seed"].unique()
               if len(per_seed_cd(v[v.seed == s], scen, qos))}
        if not cds:
            continue
        intent_pct, sla_pct = first_fail(intents, scen, qos, cds)
        intent_panels[label] = intent_pct
        for name, pct in intent_pct.items():
            ff_rows.append({"config": label, "kind": "intent", "name": name, "percent": pct})
        for name, pct in sla_pct.items():
            ff_rows.append({"config": label, "kind": "sla", "name": name, "percent": pct})
    pd.DataFrame(ff_rows).to_csv(os.path.join(OUT, "first-failure.csv"), index=False)

    if intent_panels:
        ff = pd.DataFrame(intent_panels).fillna(0)
        fig, ax = plt.subplots(figsize=(8, 4.2))
        ff.plot.bar(ax=ax, rot=15)
        style(ax, "", "share of conflicts whose first failure is this intent [%]")
        ax.set_title("First-failing intent per configuration (5G mid-band)", fontsize=10)
        ax.legend(fontsize=8)
        fig.tight_layout(); fig.savefig(os.path.join(OUT, "fig_first_failure.png"), dpi=200)
        plt.close(fig)
    print("\nfirst-failure:")
    print(pd.DataFrame(ff_rows).to_string(index=False))

    # ---- criterion sensitivity (multi baseline) ----
    crit_rows = []
    base_multi = runs[(runs.scenario == "multi") & (runs.qos == 0)]
    for crit, frac in CRITERIA.items():
        for seed, d in base_multi.groupby("seed"):
            hit = d[d["breach"] == 1] if frac is None else d[d["violFraction"] >= frac]
            crit_rows.append({"criterion": crit, "seed": seed,
                              "conflictDensity": hit["numVehicles"].min()
                              if not hit.empty else np.nan})
    crit = pd.DataFrame(crit_rows)
    if not crit.empty:
        crit.to_csv(os.path.join(OUT, "criteria.csv"), index=False)
        csumm = crit.groupby("criterion").agg(meanCD=("conflictDensity", "mean")).reset_index()
        csumm["confidence95"] = crit.groupby("criterion")["conflictDensity"].apply(
            ci95_half).values
        fig, ax = plt.subplots(figsize=(6.5, 4))
        cl = list(CRITERIA)
        cs = csumm.set_index("criterion").reindex(cl)
        ax.bar(cl, cs["meanCD"], yerr=cs["confidence95"], capsize=4, color="#1f77b4", alpha=0.85)
        style(ax, "conflict criterion (fraction of vehicles violating)",
              "conflict density [vehicles / 500 m]")
        ax.set_title("Sensitivity of conflict density to the conflict criterion\n"
                     "(mixed intents, baseline)", fontsize=10)
        fig.tight_layout(); fig.savefig(os.path.join(OUT, "fig_criteria.png"), dpi=200)
        plt.close(fig)

    # ---- headline QoS comparison table ----
    def cd_of(scen, qos):
        v = per_seed_cd(runs, scen, qos)
        return f"{v.mean():.1f} $\\pm$ {ci95_half(v):.1f}" if len(v) else "-"
    bm = intent_panels.get("mixed intents (baseline)")
    qm = intent_panels.get("mixed intents (QoS-protected)")
    tex_table(os.path.join(OUT, "table_qos.tex"),
              ["Scheduling", "Conflict density", "Dominant first-failure intent"],
              [["Baseline (proportional fair)", cd_of("multi", 0),
                bm.idxmax() if bm is not None and not bm.empty else "-"],
               ["QoS-protected (RD+sensor GBR)", cd_of("multi", 1),
                qm.idxmax() if qm is not None and not qm.empty else "-"]],
              "Effect of QoS-protected scheduling on the mixed-intent conflict "
              "density and the first intent to fail (5G mid-band).", "tab:qos")
    print(f"\nOutputs in {OUT}/")


if __name__ == "__main__":
    main()
