#!/bin/bash
# Runs the four megaminx configurations (original, current/no-jit,
# current/--jit, current/--jit --jit-table) on a fixed scramble, twice each,
# strictly sequentially (never concurrently -- timing-sensitive), capturing
# instructions/cycles where the platform can supply them.  Meant to be
# launched once and left alone; writes a summary at the end via
# aggregate_megaminx_ab.py.
#
# Portable across machines/architectures: run from anywhere, on any clone.
# Needs build/bin/twsearch_original (built from main) and
# build/bin/twsearch_current (built from this branch) already present --
# see the README in this directory.
#
# Instructions/cycles come from:
#   macOS:            /usr/bin/time -l          (built in, no setup)
#   Linux, with perf: perf stat -e instructions,cycles -x,
#   Linux, no perf:   not available -- wall time only (still meaningful)
set -u
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1

SEQ="R' E' BL2 I2 BF' L2' R2 E2' A' I2' BL I' BL R"
PUZZLE=samples/symm/megaminx.tws
LOGDIR=experiments/minrot/logs
mkdir -p "$LOGDIR"

OS=$(uname -s)
HAVEPERF=0
if [ "$OS" = "Linux" ] && command -v perf >/dev/null 2>&1; then
  HAVEPERF=1
fi

declare -a LABELS=(original current_nojit current_jit current_jittable)
declare -a BINS=(build/bin/twsearch_original build/bin/twsearch_current build/bin/twsearch_current build/bin/twsearch_current)
declare -a ARGS=("" "" "--jit" "--jit --jit-table")

SUMMARY="$LOGDIR/megaminx_ab_summary.txt"
echo "megaminx A/B run started $(date)" > "$SUMMARY"
echo "host: $(uname -a)" >> "$SUMMARY"
echo "scramble: $SEQ" >> "$SUMMARY"
echo "puzzle: $PUZZLE" >> "$SUMMARY"
echo >> "$SUMMARY"

for rep in 1 2; do
  for i in 0 1 2 3; do
    label="${LABELS[$i]}"
    bin="${BINS[$i]}"
    args="${ARGS[$i]}"
    logfile="$LOGDIR/megaminx_${label}_run${rep}.log"
    echo "=== run $rep: $label ($bin $args) -> $logfile ===" | tee -a "$SUMMARY"
    start=$(date +%s)
    if [ "$OS" = "Darwin" ]; then
      # shellcheck disable=SC2086
      echo "$SEQ" | /usr/bin/time -l "$bin" --nowrite -M 8000 $args -s "$PUZZLE" \
        > "$logfile" 2>&1
      ec=$?
      instr=$(grep 'instructions retired' "$logfile" | awk '{print $1}')
      cyc=$(grep 'cycles elapsed' "$logfile" | awk '{print $1}')
    elif [ "$HAVEPERF" = "1" ]; then
      statfile="$logfile.perfstat"
      # shellcheck disable=SC2086
      echo "$SEQ" | perf stat -e instructions,cycles -x, -o "$statfile" -- \
        "$bin" --nowrite -M 8000 $args -s "$PUZZLE" > "$logfile" 2>&1
      ec=$?
      cat "$statfile" >> "$logfile"
      instr=$(grep ',instructions,' "$statfile" | cut -d, -f1)
      cyc=$(grep ',cycles,' "$statfile" | cut -d, -f1)
    else
      # shellcheck disable=SC2086
      echo "$SEQ" | "$bin" --nowrite -M 8000 $args -s "$PUZZLE" \
        > "$logfile" 2>&1
      ec=$?
      instr=""
      cyc=""
    fi
    end=$(date +%s)
    # Canonical, platform-independent summary lines the aggregator reads;
    # wall time is measured here directly rather than parsed out of
    # whichever tool's own report, so it's consistent across platforms.
    {
      echo "WALL_SECONDS: $(( end - start ))"
      echo "INSTRUCTIONS: ${instr:-NA}"
      echo "CYCLES: ${cyc:-NA}"
      echo "EXIT: $ec"
    } >> "$logfile"
    echo "  wall $(( end - start ))s, exit $ec" | tee -a "$SUMMARY"
  done
done

echo >> "$SUMMARY"
echo "all runs complete $(date)" | tee -a "$SUMMARY"
echo >> "$SUMMARY"

python3 experiments/minrot/aggregate_megaminx_ab.py "$LOGDIR" 2 "${LABELS[@]}" >> "$SUMMARY" 2>&1
echo >> "$SUMMARY"
echo "=== summary ($SUMMARY) ==="
cat "$SUMMARY"
