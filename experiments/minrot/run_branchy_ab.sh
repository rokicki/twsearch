#!/bin/bash
# Focused A/B: --symmguess-branchy (explicit if/else) vs the default
# (ternary) shape of lowsymmbits' mandatory round, no JIT involved either
# way, on both 3x3x3 and megaminx, two reps each, strictly sequential.
# Settles whether the branch-free rewrite (a clean win on arm64, confirmed
# via disassembly there) is actually a net loss on this machine, since
# lowsymmbits got fully inlined here and isn't independently disassemblable
# the way it was on arm64.  Not meant to be permanent -- see
# --symmguess-branchy in src/cpp/twsearch.cpp.
set -u
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1

LOGDIR=experiments/minrot/logs
mkdir -p "$LOGDIR"

OS=$(uname -s)
HAVEPERF=0
if [ "$OS" = "Linux" ] && command -v perf >/dev/null 2>&1; then
  HAVEPERF=1
fi

declare -a LABELS=(ternary branchy)
declare -a ARGS=("" "--symmguess-branchy")

run_puzzle() {
  local prefix="$1" puzzle="$2" seq="$3"
  for rep in 1 2; do
    for i in 0 1; do
      local label="${LABELS[$i]}"
      local args="${ARGS[$i]}"
      local logfile="$LOGDIR/${prefix}_${label}_run${rep}.log"
      echo "=== $prefix run $rep: $label ($args) -> $logfile ==="
      local start end ec instr cyc
      start=$(date +%s)
      if [ "$OS" = "Darwin" ]; then
        # shellcheck disable=SC2086
        echo "$seq" | /usr/bin/time -l ./build/bin/twsearch_current \
          --nowrite -M 8000 $args -s "$puzzle" > "$logfile" 2>&1
        ec=$?
        instr=$(grep 'instructions retired' "$logfile" | awk '{print $1}')
        cyc=$(grep 'cycles elapsed' "$logfile" | awk '{print $1}')
      elif [ "$HAVEPERF" = "1" ]; then
        local statfile="$logfile.perfstat"
        # shellcheck disable=SC2086
        echo "$seq" | perf stat -e instructions,cycles -x, -o "$statfile" -- \
          ./build/bin/twsearch_current --nowrite -M 8000 $args -s "$puzzle" \
          > "$logfile" 2>&1
        ec=$?
        cat "$statfile" >> "$logfile"
        instr=$(grep ',instructions,' "$statfile" | cut -d, -f1)
        cyc=$(grep ',cycles,' "$statfile" | cut -d, -f1)
      else
        # shellcheck disable=SC2086
        echo "$seq" | ./build/bin/twsearch_current --nowrite -M 8000 $args \
          -s "$puzzle" > "$logfile" 2>&1
        ec=$?
        instr=""
        cyc=""
      fi
      end=$(date +%s)
      {
        echo "WALL_SECONDS: $(( end - start ))"
        echo "INSTRUCTIONS: ${instr:-NA}"
        echo "CYCLES: ${cyc:-NA}"
        echo "EXIT: $ec"
      } >> "$logfile"
      echo "  wall $(( end - start ))s, exit $ec"
    done
  done
}

SUMMARY="$LOGDIR/branchy_ab_summary.txt"
{
echo "branchy-vs-ternary A/B run started $(date)"
echo "host: $(uname -a)"
} > "$SUMMARY"

{
run_puzzle cube333 samples/symm/3x3x3.tws \
  "B F D' D2 D2 B2 B R' F' L2 F2 F F' D D2 R2 F R' D R' L2 D2 L' D' F B' L2 U2 D' B2 D U2 B F' L B U' U' D' F2 L' R' B L F' R' U U' D F'"
run_puzzle megaminx samples/symm/megaminx.tws \
  "R' E' BL2 I2 BF' L2' R2 E2' A' I2' BL I' BL R"
echo "all runs complete $(date)"
echo
python3 experiments/minrot/aggregate_megaminx_ab.py "$LOGDIR" 2 "${LABELS[@]}" --prefix cube333
echo
python3 experiments/minrot/aggregate_megaminx_ab.py "$LOGDIR" 2 "${LABELS[@]}" --prefix megaminx
} | tee -a "$SUMMARY"
