#!/usr/bin/env bash
# Trace check (P5b, extended in P5z): the Nets of each run byte-compared with a reference
# kernel's, and the count of applied events. Evaluations are printed, not compared: sampled
# inputs (P5b idea 5) and quiet Pins (P5z Z2) spare evaluations that changed nothing.
# Usage: bench/trace_check.sh [commit]   (default f849080, the last commit before P5z)
# Builds the reference once in a worktree under build/trace-ref/<commit>/, records its traces
# once per set of run options, then records the current build/release traces and compares.
# Short runs record every Net; long runs (the computers booted and typed on) record a list,
# since a full trace of the BW-2001 is about 1.4 GB a simulated second. Build release first.
set -euo pipefail

commit="${1:-f849080}"
root="$(git rev-parse --show-toplevel)"
cd "$root"

ref="build/trace-ref/$commit"
now="build/trace-now"
current_bw="build/release/src/cli/bw"

runs=(bus_load test_pattern typewriter bw2001 bw2001_long bw80 bw80_long bw1)

# The long runs' Nets: what shows the computer working (buses, selects, video out).
bw2001_nets="A0,A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11,A12,A13,A14,A15"
bw2001_nets+=",D0,D1,D2,D3,D4,D5,D6,D7"
bw2001_nets+=",BA0,BA1,BA2,BA3,BA4,BA5,BA6,BA7,BA8,BA9,BA10,BA11"
bw2001_nets+=",BBD0,BBD1,BBD2,BBD3,BBD4,BBD5,BBD6,BBD7"
bw2001_nets+=",SA0,SA1,SA2,SA3,SA4,SA5,SA6,SA7,SA8,SA9,SD0,SD1,SD2,SD3,SD4,SD5,SD6,SD7"
bw2001_nets+=",RAM0_N,RAM1_N,RAM2_N,RAM3_N,RAM4_N,RAM5_N,RAM6_N,RAM7_N"
bw2001_nets+=",SEL8,SELC_N,SELD_N,SELE_N,SELF_N,TV_VIDEO,HORZ_DRIVE,VERT_DRIVE"
bw80_nets="A0,A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11,A12,A13,A14,A15"
bw80_nets+=",D0,D1,D2,D3,D4,D5,D6,D7,MD0,MD1,MD2,MD3,MD4,MD5,MD6,MD7"
bw80_nets+=",ROM_CS1,RAM_CS_N,KB0,KB1,KB2,KB3,KB4,VIDEO,SYNC,VS_N"

# run_netlist <run>: the netlist a run simulates
run_netlist() {
    case "$1" in
    *_long) echo "${1%_long}" ;;
    *) echo "$1" ;;
    esac
}

# run_args <run>: the run's own options (P5z spec S1); no spaces inside an option
run_args() {
    case "$1" in
    bus_load) echo "--time 50ms" ;;
    test_pattern) echo "--time 100ms" ;;
    typewriter) echo "--time 300ms --type J3:100ms:HI" ;;
    bw2001) echo "--time 150ms" ;;
    bw2001_long) echo "--time 2500ms --type J5:1600ms:PRINT7\\n --record $bw2001_nets" ;;
    bw80) echo "--time 100ms" ;;
    bw80_long) echo "--time 1s --type J3:400ms:P --record $bw80_nets" ;;
    bw1) echo "--time 200ms" ;;
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

# netlist_path <tree> <name>: the netlist in a checkout, before P6a (netlists/) or after
# (machines/NNN-name/, bench/); empty if it has none.
netlist_path() {
    local f
    for f in "$1/netlists/$2.bwn" "$1"/machines/*/"$2.bwn" "$1/bench/$2.bwn"; do
        [[ -f "$f" ]] && { echo "$f"; return; }
    done
}

# tree_args <tree> <run>: options a checkout needs to run what the reference ran. The BW-2001
# runs Commodore's BASIC 1 since 2026-11-22; our firmware, which the reference runs, is then
# its debug ROMs.
tree_args() {
    case "$(run_netlist "$2")" in
    bw2001) [[ -f "$1/machines/130-bw2001/debug-roms/sockets.txt" ]] &&
        echo " --roms $1/machines/130-bw2001/debug-roms" ;;
    esac
    return 0
}

# record <bw> <tree> <out dir> <run>: the VCD, then the options and counts in <run>.txt
record() {
    local bw="$1" tree="$2" out="$3" run="$4"
    local args
    args="$(run_args "$run")$(tree_args "$tree" "$run")"
    if [[ "$args" != *--record* ]]; then
        args+=" --record-all"
    fi
    set -f # typed text may hold glob characters
    # shellcheck disable=SC2086 # the options hold no spaces
    "$bw" run "$(netlist_path "$tree" "$(run_netlist "$run")")" $args --seed 1 --vcd "$out/$run.vcd" |
        grep -E '^(applied_events|evaluations) ' >"$out/$run.counts"
    set +f
    { echo "args $(run_args "$run")"; cat "$out/$run.counts"; } >"$out/$run.txt"
    rm "$out/$run.counts"
}

mkdir -p "$now"
active=()
for run in "${runs[@]}"; do
    if [[ -z "$(netlist_path "$ref/src" "$(run_netlist "$run")")" ]]; then
        echo "trace check: $run skipped (no netlist at $commit)"
        continue
    fi
    active+=("$run")
    # A reference recorded with other options is recorded again.
    if [[ ! -f "$ref/$run.txt" ]] || [[ "$(head -1 "$ref/$run.txt")" != "args $(run_args "$run")" ]]; then
        record "$ref/build/src/cli/bw" "$ref/src" "$ref" "$run"
    fi
    record "$current_bw" . "$now" "$run"
done

status=0
for run in "${active[@]}"; do
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
    echo "trace check: identical (${#active[@]} runs)"
fi
exit $status
