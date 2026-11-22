#!/usr/bin/env bash
# The BW-2001's debug ROMs: our own firmware (Tiny BASIC, the editor, the monitor, the self-test),
# run with --roms machines/130-bw2001/debug-roms. Assembles src/*.asm into src/*.rom (the listing,
# checked by the asm tests), then cuts the images into one 2 KB file per socket, UH1.bin to
# UH7.bin, as --roms takes them. UH2 and UH6 stay empty (every byte $FF). Run it after changing a
# .asm, from anywhere, with build/release built; commit the .rom and .bin files it writes.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
bw="$here/../../../build/release/src/cli/bw"
[[ -x "$bw" ]] || { echo "build.sh: build release first ($bw)"; exit 2; }
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

for name in bw2001_basic bw2001_edit bw2001_kernel bw2001_bench; do
    "$bw" asm "$here/src/$name.asm" -o "$here/src/$name.rom"
    "$bw" asm "$here/src/$name.asm" -o "$tmp/$name.bin" >/dev/null
done

# chip <image> <2 KB block> <socket>
chip() { dd if="$tmp/$1.bin" of="$here/$3.bin" bs=2048 skip="$2" count=1 status=none; }
chip bw2001_basic 0 UH1  # $C000
chip bw2001_basic 1 UH5  # $C800
chip bw2001_edit 0 UH3   # $E000
chip bw2001_kernel 0 UH4 # $F000
chip bw2001_kernel 1 UH7 # $F800
for socket in UH2 UH6; do # $D000, $D800: empty
    head -c 2048 /dev/zero | LC_ALL=C tr '\0' '\377' >"$here/$socket.bin"
done
echo "build.sh: UH1-UH7.bin written"
