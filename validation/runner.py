#!/usr/bin/env python3
"""
Run engine for the conflict-density study. Invokes the v2x-conflict-5g
binary once per (scenario, qos, seed, density) point, parses its
stdout/CSV, and accumulates results under results/.

Resumable: every completed run is appended to results/cd-runs.csv (and its
per-intent rows to results/cd-intents.csv); on restart, completed runs are
skipped. A run that crashes (a known RLC UM overload abort, which only
occurs well past the conflict point) is counted as a breach.

This module is imported by run_study.py; it is not run directly.
"""

import csv
import os
import subprocess
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)                       # .../scratch/v2x-conflict-5g
NS3_ROOT = os.path.abspath(os.path.join(PKG, "..", ".."))  # .../ns-3.45
BINARY = os.path.join(NS3_ROOT, "build", "scratch", "v2x-conflict-5g",
                      "ns3.45-v2x-conflict-5g-optimized")
OUT_DIR = os.path.join(PKG, "results")
RAW_DIR = os.path.join(OUT_DIR, "raw")
RUN_TIMEOUT_S = 2 * 3600

RUNS_FIELDS = ["study", "scenario", "qos", "seed", "simTime", "numVehicles",
               "breach", "breachedIntents", "crashed"]

csv_lock = threading.Lock()
print_lock = threading.Lock()


def log(msg):
    with print_lock:
        print(msg, flush=True)


def run_key(row):
    return (row["study"], row["scenario"], str(int(row["qos"])),
            str(row["seed"]), str(float(row["simTime"])), str(row["numVehicles"]))


def load_done(study):
    """Already-completed runs of this study, keyed for resume."""
    done = {}
    path = os.path.join(OUT_DIR, f"{study}-runs.csv")
    if os.path.exists(path):
        with open(path) as f:
            for row in csv.DictReader(f):
                done[run_key(row)] = row
    return done


def append_row(study, suffix, fields, rows):
    path = os.path.join(OUT_DIR, f"{study}-{suffix}.csv")
    with csv_lock:
        new = not os.path.exists(path)
        with open(path, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
            if new:
                w.writeheader()
            for r in rows:
                w.writerow(r)
            f.flush()


def execute(study, scenario, seed, sim_time, n, done, qos=False):
    """Run one simulation (or return its cached result). Returns runs-row."""
    meta = {"study": study, "scenario": scenario, "qos": int(qos), "seed": seed,
            "simTime": float(sim_time), "numVehicles": n}
    cached = done.get(run_key(meta))
    if cached:
        cached["breach"] = int(cached["breach"])
        return cached

    tag = f"{study}_{scenario}_qos{int(qos)}_s{seed}_n{n}_t{sim_time}"
    raw_csv = os.path.join(RAW_DIR, tag + ".csv")
    if os.path.exists(raw_csv):
        os.remove(raw_csv)  # binary appends; stale partial data must go

    cmd = [BINARY, f"--scenario={scenario}", f"--numVehicles={n}",
           f"--simTime={sim_time}", f"--rngRun={seed}", f"--csvPath={raw_csv}"]
    if qos:
        cmd.append("--qosProtected=1")

    log(f"RUN  {tag}")
    crashed = False
    breach = None
    breached_intents = ""
    try:
        res = subprocess.run(cmd, cwd=NS3_ROOT, capture_output=True, text=True,
                             timeout=RUN_TIMEOUT_S)
        for line in res.stdout.splitlines():
            if line.startswith("SLA_BREACH="):
                breach = int(line.split("=")[1])
            if "breachedIntents=" in line:
                breached_intents = line.split("breachedIntents=")[1].strip()
        if res.returncode != 0 or breach is None:
            crashed = True
    except subprocess.TimeoutExpired:
        crashed = True

    if crashed:
        breach = 1
        breached_intents = "crash-overload"

    row = dict(meta, breach=breach, breachedIntents=breached_intents,
               crashed=int(crashed))
    append_row(study, "runs", RUNS_FIELDS, [row])

    if os.path.exists(raw_csv):
        with open(raw_csv) as f:
            intents = list(csv.DictReader(f))
        for r in intents:
            r.update(study=study, qos=int(qos), seed=seed)
        if intents:
            fields = ["study", "qos", "seed"] + \
                [c for c in intents[0] if c not in ("study", "qos", "seed")]
            append_row(study, "intents", fields, intents)
        os.remove(raw_csv)

    log(f"DONE {tag} breach={breach}{' (crashed)' if crashed else ''}")
    return row
