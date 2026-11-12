#!/usr/bin/env bash
# P5b measurement: best of 3 ratios of every netlist (2 s simulated, alternated), and
# instructions retired and cycles per applied event on bus_load. Build release first.
set -euo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"
bw="build/release/src/cli/bw"
[[ -x "$bw" ]] || { echo "measure: build release first ($bw)"; exit 2; }

netlists=()
for f in netlists/*.bwn; do
    netlists+=("$(basename "$f" .bwn)")
done

best=()  # parallel to netlists (bash 3.2: no associative arrays)
for round in 1 2 3; do
    for i in "${!netlists[@]}"; do
        ratio="$("$bw" run "netlists/${netlists[$i]}.bwn" --time 2s | sed -n 's/^ratio //p')"
        if [[ -z "${best[$i]:-}" ]] || awk -v a="$ratio" -v b="${best[$i]}" 'BEGIN{exit !(a>b)}'; then
            best[$i]="$ratio"
        fi
    done
done
best_of() {
    for i in "${!netlists[@]}"; do
        [[ "${netlists[$i]}" == "$1" ]] && echo "${best[$i]}"
    done
}

out="$( { /usr/bin/time -l "$bw" run netlists/bus_load.bwn --time 1s; } 2>&1)"
instructions="$(awk '/instructions retired/{print $1}' <<<"$out")"
events="$(sed -n 's/^applied_events //p' <<<"$out")"
cycles="$(awk '/cycles elapsed/{print $1}' <<<"$out")"
per_event="$(awk -v i="$instructions" -v e="$events" 'BEGIN{printf "%.0f", i/e}')"
cycles_per_event="$(awk -v c="$cycles" -v e="$events" 'BEGIN{printf "%.0f", c/e}')"

fmt() { awk -v r="$1" 'BEGIN{printf "%.3f×", r}'; }
echo "| bus_load $(fmt "$(best_of bus_load)") | $per_event instr./event | $cycles_per_event cycles/event" \
    "($(awk -v i="$instructions" 'BEGIN{printf "%.2f G", i/1e9}') for $events events, 1 s)" \
    "| test pattern $(fmt "$(best_of test_pattern)") | typewriter $(fmt "$(best_of typewriter)") |"
for i in "${!netlists[@]}"; do
    case "${netlists[$i]}" in bus_load | test_pattern | typewriter) continue ;; esac
    echo "  ${netlists[$i]} $(fmt "${best[$i]}")"
done
