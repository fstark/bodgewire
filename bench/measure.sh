#!/usr/bin/env bash
# Measurement (P5b, headline moved to the BW-2001 in P5z): best of 3 ratios of every netlist
# (2 s simulated, alternated), and instructions retired, cycles and evaluations per applied
# event on bw2001 and bus_load (1 s each). Build release first; run on an otherwise idle machine.
# Usage: bench/measure.sh [previous.txt]   a previous run's output: each netlist's ratio is
# printed against it, so a slowdown stands out (P5z spec, Guard 3).
set -euo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"
bw="build/release/src/cli/bw"
[[ -x "$bw" ]] || { echo "measure: build release first ($bw)"; exit 2; }
previous="${1:-}"

netlists=()
for f in netlists/*.bwn; do
    netlists+=("$(basename "$f" .bwn)")
done

best=() # parallel to netlists (bash 3.2: no associative arrays)
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

fmt() { awk -v r="$1" 'BEGIN{printf "%.3f×", r}'; }

# per_event <netlist>: "instr cycles evals" per applied event over 1 s
per_event() {
    local out
    out="$( { /usr/bin/time -l "$bw" run "netlists/$1.bwn" --time 1s; } 2>&1)"
    awk '/instructions retired/{i=$1} /cycles elapsed/{c=$1}
         /^applied_events /{e=$2} /^evaluations /{v=$2}
         END{printf "%.0f %.0f %.2f", i/e, c/e, v/e}' <<<"$out"
}

read -r instr cycles evals <<<"$(per_event bw2001)"
read -r bl_instr bl_cycles bl_evals <<<"$(per_event bus_load)"
echo "| bw2001 $(fmt "$(best_of bw2001)") | $instr instr./event | $cycles cycles/event" \
    "| $evals eval./event | bw80 $(fmt "$(best_of bw80)") | bus_load $(fmt "$(best_of bus_load)") |"
echo "  bus_load: $bl_instr instr./event, $bl_cycles cycles/event, $bl_evals eval./event"
for i in "${!netlists[@]}"; do
    case "${netlists[$i]}" in bw2001 | bw80 | bus_load) continue ;; esac
    line="  ${netlists[$i]} $(fmt "${best[$i]}")"
    if [[ -n "$previous" ]]; then
        before="$(sed -nE "s/^  ${netlists[$i]} ([0-9.]+)×.*/\1/p" "$previous")"
        if [[ -n "$before" ]]; then
            line+="$(awk -v a="${best[$i]}" -v b="$before" \
                'BEGIN{d=(a/b-1)*100; printf " (%+.1f %%%s)", d, (d < -5 ? ", SLOWER" : "")}')"
        fi
    fi
    echo "$line"
done
