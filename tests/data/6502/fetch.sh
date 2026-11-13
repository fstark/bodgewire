#!/bin/sh
# Fetches the full 6502 conformance data into external/6502/ (git-ignored), for the
# "6502 core: Harte full" and "6502 core: Dormann functional test" cases, which skip without it.
#   external/6502/harte/<hh>.txt   all 10,000 cases of each opcode, converted (convert_harte.py)
#   external/6502/dormann/         6502_functional_test.bin and .lst, and `success` (its trap)
#   external/6502/SOURCES          the commits fetched
# With --subset, also rewrites the committed subset tests/data/6502/harte/ (100 cases each).
# About 1 GB is downloaded, one opcode file at a time; each JSON is deleted once converted.
set -eu

root=$(cd "$(dirname "$0")/../../.." && pwd)
out="$root/external/6502"
python="$root/venv/bin/python"
[ -x "$python" ] || python=python3
convert="$root/tests/data/6502/convert_harte.py"
subset=no
[ "${1:-}" = "--subset" ] && subset=yes

commit() {  # owner/repo branch → the branch's commit hash
    curl -fsSL "https://api.github.com/repos/$1/commits/$2" |
        sed -n 's/^  "sha": "\([0-9a-f]*\)".*/\1/p' | head -n 1
}

harte_commit=$(commit SingleStepTests/65x02 main)
dormann_commit=$(commit Klaus2m5/6502_65C02_functional_tests master)
[ -n "$harte_commit" ] && [ -n "$dormann_commit" ] || { echo "fetch.sh: no commit hash" >&2; exit 1; }

mkdir -p "$out/harte" "$out/dormann" "$out/json"
source="https://github.com/SingleStepTests/65x02 6502/v1 @ $harte_commit"
for i in $(seq 0 255); do
    hh=$(printf '%02x' "$i")
    json="$out/json/$hh.json"
    curl -fsSL -o "$json" \
        "https://raw.githubusercontent.com/SingleStepTests/65x02/$harte_commit/6502/v1/$hh.json"
    "$python" "$convert" "$json" "$out/harte" --source "$source" >/dev/null
    if [ "$subset" = yes ]; then
        "$python" "$convert" "$json" "$root/tests/data/6502/harte" --cases 100 \
            --source "$source" >/dev/null
    fi
    rm "$json"
    echo "harte $hh"
done
rmdir "$out/json"

base="https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/$dormann_commit"
for f in 6502_functional_test.bin 6502_functional_test.lst; do
    curl -fsSL -o "$out/dormann/$f" "$base/bin_files/$f"
done
# The success trap: the `jmp *` the listing marks "test passed, no errors".
sed -n 's/^\([0-9a-f]\{4\}\) : .*jmp \*.*test passed, no errors.*/\1/p' \
    "$out/dormann/6502_functional_test.lst" | head -n 1 > "$out/dormann/success"
[ -s "$out/dormann/success" ] || { echo "fetch.sh: no success trap in the listing" >&2; exit 1; }

printf 'harte %s\ndormann %s\n' "$harte_commit" "$dormann_commit" > "$out/SOURCES"
echo "done: $out (success trap \$$(cat "$out/dormann/success"))"
