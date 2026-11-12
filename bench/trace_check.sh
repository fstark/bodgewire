#!/usr/bin/env bash
# P5b trace check: every Net of three runs, byte-compared with a reference kernel's, and the
# count of applied events. Evaluations are printed, not compared: sampled inputs (idea 5) spare
# evaluations that changed nothing.
# Usage: bench/trace_check.sh [commit]   (default 0757c02, the P5a kernel)
# Builds the reference once in a worktree under build/trace-ref/<commit>/, records its traces
# once, then records the current build/release traces and compares. Build release first.
set -euo pipefail

commit="${1:-0757c02}"
root="$(git rev-parse --show-toplevel)"
cd "$root"

ref="build/trace-ref/$commit"
now="build/trace-now"
current_bw="build/release/src/cli/bw"

runs=(bus_load test_pattern typewriter)
# run_args <run>: the run's own options (spec S1)
run_args() {
    case "$1" in
    bus_load) echo "--time 50ms" ;;
    test_pattern) echo "--time 100ms" ;;
    typewriter) echo "--time 300ms --type J3:100ms:HI" ;;
    esac
}

[[ -x "$current_bw" ]] || { echo "trace check: build release first ($current_bw)"; exit 2; }

if [[ ! -d "$ref/src" ]]; then
    mkdir -p "$ref"
    git worktree add --detach "$ref/src" "$commit" >/dev/null
fi
if [[ ! -x "$ref/build/src/cli/bw" ]]; then
    cmake -S "$ref/src" -B "$ref/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBW_BUILD_TESTS=OFF -DBW_BUILD_UI=OFF >/dev/null
    cmake --build "$ref/build" --target bw >/dev/null
fi

# record <bw> <netlist dir> <out dir> <run>
record() {
    local bw="$1" netlists="$2" out="$3" run="$4"
    # shellcheck disable=SC2046 # the options hold no spaces
    "$bw" run "$netlists/$run.bwn" $(run_args "$run") --seed 1 --record-all --vcd "$out/$run.vcd" |
        grep -E '^(applied_events|evaluations) ' >"$out/$run.txt"
}

mkdir -p "$now"
for run in "${runs[@]}"; do
    if [[ ! -f "$ref/$run.txt" ]]; then
        record "$ref/build/src/cli/bw" "$ref/src/netlists" "$ref" "$run"
    fi
    record "$current_bw" netlists "$now" "$run"
done

status=0
for run in "${runs[@]}"; do
    if ! diff -u <(grep '^applied_events ' "$ref/$run.txt") \
        <(grep '^applied_events ' "$now/$run.txt"); then
        echo "trace check: $run applied events differ"
        status=1
    fi
    echo "$run: evaluations $(sed -n 's/^evaluations //p' "$ref/$run.txt") ->" \
        "$(sed -n 's/^evaluations //p' "$now/$run.txt")"
    if ! out="$(cmp "$ref/$run.vcd" "$now/$run.vcd" 2>&1)"; then
        echo "trace check: $run VCD differs: $out"
        line="$(sed -nE 's/.* line ([0-9]+).*/\1/p' <<<"$out")"
        if [[ -n "$line" ]]; then
            for side in "$ref" "$now"; do
                echo "  $side: $(head -n "$line" "$side/$run.vcd" | grep '^#' | tail -1)" \
                    "line $line: $(sed -n "${line}p" "$side/$run.vcd")"
            done
        fi
        status=1
    fi
done
if [[ $status -eq 0 ]]; then
    echo "trace check: identical (${#runs[@]} runs)"
fi
exit $status
