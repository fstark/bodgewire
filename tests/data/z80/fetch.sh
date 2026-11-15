#!/bin/sh
# Fetches the full Z80 conformance data into external/z80/ (git-ignored), for the
# "Z80 core: Harte full", "Z80 core: ZEXDOC" and "Z80 core: ZEXALL" cases, which skip without it.
#   external/z80/harte/<name>.txt   all 1,000 cases of each of Harte's 1,604 files, converted
#   external/z80/zex/               zexdoc.com and zexall.com (Frank Cringle, GPL; never committed)
#   external/z80/SOURCES            the commits fetched
# With --subset, also rewrites the committed subset tests/data/z80/harte/ (25 cases each).
# About 1.4 GB is downloaded, one file at a time; each JSON is deleted once converted.
set -eu

root=$(cd "$(dirname "$0")/../../.." && pwd)
out="$root/external/z80"
python="$root/venv/bin/python"
[ -x "$python" ] || python=python3
convert="$root/tests/data/z80/convert_harte.py"
subset=no
[ "${1:-}" = "--subset" ] && subset=yes

commit() {  # owner/repo branch → the branch's commit hash
    curl -fsSL "https://api.github.com/repos/$1/commits/$2" |
        sed -n 's/^  "sha": "\([0-9a-f]*\)".*/\1/p' | head -n 1
}

harte_commit=$(commit SingleStepTests/z80 main)
zex_commit=$(commit agn453/ZEXALL main)
[ -n "$harte_commit" ] && [ -n "$zex_commit" ] || { echo "fetch.sh: no commit hash" >&2; exit 1; }

mkdir -p "$out/harte" "$out/zex" "$out/json"
# The file names (with spaces: "dd cb __ 06.json") from the repository's tree at that commit.
curl -fsSL -o "$out/json/tree.json" \
    "https://api.github.com/repos/SingleStepTests/z80/git/trees/$harte_commit?recursive=1"
"$python" -I -c '
import json, sys
tree = json.load(open(sys.argv[1]))
for entry in tree["tree"]:
    path = entry["path"]
    if path.startswith("v1/") and path.endswith(".json"):
        print(path[3:-5])
' "$out/json/tree.json" > "$out/json/names"
rm "$out/json/tree.json"
count=$(wc -l < "$out/json/names" | tr -d ' ')
[ "$count" -eq 1604 ] || { echo "fetch.sh: expected 1604 files, found $count" >&2; exit 1; }

source="https://github.com/SingleStepTests/z80 v1 @ $harte_commit"
while IFS= read -r name; do
    json="$out/json/$name.json"
    url_name=$(printf '%s' "$name" | sed 's/ /%20/g')
    curl -fsSL -o "$json" \
        "https://raw.githubusercontent.com/SingleStepTests/z80/$harte_commit/v1/$url_name.json"
    "$python" "$convert" "$json" "$out/harte" --source "$source" >/dev/null
    if [ "$subset" = yes ]; then
        "$python" "$convert" "$json" "$root/tests/data/z80/harte" --cases 25 \
            --source "$source" >/dev/null
    fi
    rm "$json"
    echo "harte $name"
done < "$out/json/names"
rm "$out/json/names"
rmdir "$out/json"

for f in zexdoc.com zexall.com; do
    curl -fsSL -o "$out/zex/$f" "https://raw.githubusercontent.com/agn453/ZEXALL/$zex_commit/$f"
done

printf 'harte %s\nzexall %s\n' "$harte_commit" "$zex_commit" > "$out/SOURCES"
echo "done: $out"
