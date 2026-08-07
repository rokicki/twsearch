#!/usr/bin/env python3
"""Parse per-run A/B logs and print a summary table with per-config
averages over the repeats.  Usage:
  aggregate_megaminx_ab.py <logdir> <nreps> <label1> <label2> ... [--prefix name]

Log files are expected at <logdir>/<prefix>_<label>_run<rep>.log; --prefix
defaults to "megaminx" for backward compatibility with run_megaminx_ab.sh.

Reads the canonical WALL_SECONDS/INSTRUCTIONS/CYCLES/EXIT lines that
run_megaminx_ab.sh appends to each log (platform-independent -- how those
numbers were obtained, e.g. macOS's /usr/bin/time -l vs Linux's perf stat,
is run_megaminx_ab.sh's problem, not this script's).  INSTRUCTIONS/CYCLES
may be "NA" (e.g. Linux without perf available); handled as missing.
"""
import re
import sys


def parse_log(path):
    real = instr = cycles = None
    solution = None
    exitcode = None
    try:
        with open(path) as f:
            lines = f.readlines()
    except FileNotFoundError:
        return None
    for i, line in enumerate(lines):
        s = line.strip()
        m = re.match(r"^WALL_SECONDS:\s*(\d+)", s)
        if m:
            real = float(m.group(1))
        m = re.match(r"^INSTRUCTIONS:\s*(\d+)", s)
        if m:
            instr = int(m.group(1))
        m = re.match(r"^CYCLES:\s*(\d+)", s)
        if m:
            cycles = int(m.group(1))
        m = re.match(r"^EXIT:\s*(-?\d+)", s)
        if m:
            exitcode = int(m.group(1))
        if s.startswith("Found ") and "solution" in s and i > 0:
            solution = lines[i - 1].strip()
    return {
        "real": real,
        "instr": instr,
        "cycles": cycles,
        "solution": solution,
        "exit": exitcode,
    }


def fmt(x, digits=2):
    return "?" if x is None else f"{x:,.{digits}f}"


def main():
    logdir = sys.argv[1]
    nreps = int(sys.argv[2])
    rest = sys.argv[3:]
    prefix = "megaminx"
    if "--prefix" in rest:
        i = rest.index("--prefix")
        prefix = rest[i + 1]
        rest = rest[:i] + rest[i + 2:]
    labels = rest

    print(f"{'config':<18} {'run':>3} {'real(s)':>10} {'instructions':>16} "
          f"{'cycles':>16} {'IPC':>6}  solution")
    allresults = {}
    for label in labels:
        results = []
        for rep in range(1, nreps + 1):
            path = f"{logdir}/{prefix}_{label}_run{rep}.log"
            r = parse_log(path)
            results.append(r)
            if r is None:
                print(f"{label:<18} {rep:>3}   (missing: {path})")
                continue
            ipc = (r["instr"] / r["cycles"]) if r["instr"] and r["cycles"] else None
            print(f"{label:<18} {rep:>3} {fmt(r['real'])} {fmt(r['instr'],0):>16} "
                  f"{fmt(r['cycles'],0):>16} {fmt(ipc):>6}  "
                  f"{r['solution']!r} exit={r['exit']}")
        allresults[label] = results

    print()
    print(f"{'config':<18} {'avg real(s)':>12} {'avg instr':>18} "
          f"{'avg cycles':>18} {'avg IPC':>8}")
    for label in labels:
        results = [r for r in allresults[label] if r]
        reals = [r["real"] for r in results if r["real"] is not None]
        instrs = [r["instr"] for r in results if r["instr"] is not None]
        cycs = [r["cycles"] for r in results if r["cycles"] is not None]
        avgreal = sum(reals) / len(reals) if reals else None
        avginstr = sum(instrs) / len(instrs) if instrs else None
        avgcyc = sum(cycs) / len(cycs) if cycs else None
        avgipc = (avginstr / avgcyc) if avginstr and avgcyc else None
        print(f"{label:<18} {fmt(avgreal):>12} {fmt(avginstr,0):>18} "
              f"{fmt(avgcyc,0):>18} {fmt(avgipc):>8}")


if __name__ == "__main__":
    main()
