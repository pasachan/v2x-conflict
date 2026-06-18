#!/usr/bin/env python3
"""
Conflict-density study driver (5G mid-band).

Runs a full density grid (no early stopping) across multiple random seeds
for three configurations, writing results under results/:

  single  (all remote driving), proportional-fair scheduler
  multi   (mixed intents),       proportional-fair scheduler
  multi   (mixed intents),       QoS-protected scheduler

The full grids give the conflict-probability curves and first-failure
analysis as well as the conflict-density estimates. Resumable: re-running
skips completed points.

Usage:
  python3 scratch/v2x-conflict-5g/validation/run_study.py --workers 8
"""

import argparse
import os
from concurrent.futures import ThreadPoolExecutor, as_completed

import runner as rv

STUDY = "cd"
SIM_TIME = 2.0
SEEDS = list(range(1, 9))  # 8 seeds

# (scenario, qos, density grid)
JOBS = [
    ("single", False, list(range(9, 21))),       # 9..20 step 1
    ("multi", False, list(range(22, 43, 2))),    # 22..42 step 2
    ("multi", True, list(range(22, 45, 2))),     # 22..44 step 2 (QoS-protected)
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=8)
    args = ap.parse_args()
    if not os.path.exists(rv.BINARY):
        raise SystemExit(f"binary not found: {rv.BINARY}\nBuild it first: ./ns3 build")
    os.makedirs(rv.RAW_DIR, exist_ok=True)

    done = rv.load_done(STUDY)
    tasks = [(scen, qos, seed, n)
             for scen, qos, grid in JOBS
             for seed in SEEDS
             for n in grid]
    tasks.sort(key=lambda t: t[3])  # cheapest (low density) first

    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futs = [ex.submit(rv.execute, STUDY, scen, seed, SIM_TIME, n, done, qos)
                for (scen, qos, seed, n) in tasks]
        for fut in as_completed(futs):
            fut.result()
    rv.log("STUDY-COMPLETE")


if __name__ == "__main__":
    main()
