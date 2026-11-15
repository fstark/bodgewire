#!/usr/bin/env python3
"""Convert Tom Harte's Z80 SingleStepTests JSON into Bodgewire's compact case format.

Usage: convert_harte.py SRC OUT [--cases N] [--source TEXT]

SRC is a JSON file (one instruction sequence, e.g. "dd cb __ 06.json") or a folder of them; OUT
is a folder. Each file becomes OUT/<name>.txt with its spaces as '_' ("dd_cb____06.txt").
--cases N keeps the first N cases of each file. --source TEXT goes into each output's header.

Format (docs/p5s-z80/Z80_DESIGN.md, Test data): '#' starts a comment line; a case is three or
four lines, every number hex, '-' for a null address or data:
  I <pc> <sp> <a> <f> <b> <c> <d> <e> <h> <l> <i> <r> <ix> <iy> <wz> <af'> <bc'> <de'> <hl'>
    <im> <iff1> <iff2> <ei> <p> <q> <address>:<value> ...
  F (the same fields)
  C <address>:<data>:<flags> ...      one per T-state; flags are Harte's "rwmi" with '-'
  P <port>:<value>:<r|w> ...          only when the case has ports
"""

import argparse
import json
import pathlib
import sys

FIELDS = ("pc", "sp", "a", "f", "b", "c", "d", "e", "h", "l", "i", "r", "ix", "iy", "wz",
          "af_", "bc_", "de_", "hl_", "im", "iff1", "iff2", "ei", "p", "q")


def hex_or_dash(value):
    return "-" if value is None else f"{value:x}"


def state_line(tag, state):
    head = [tag] + [f"{state[k]:x}" for k in FIELDS]
    ram = [f"{address:x}:{value:x}" for address, value in state["ram"]]
    return " ".join(head + ram)


def cycles_line(cycles):
    words = ["C"]
    for address, data, flags in cycles:
        if len(flags) != 4 or any(c not in "rwmi-" for c in flags):
            raise ValueError(f"unknown pin flags {flags!r}")
        words.append(f"{hex_or_dash(address)}:{hex_or_dash(data)}:{flags}")
    return " ".join(words)


def ports_line(ports):
    words = ["P"]
    for port, value, direction in ports:
        if direction not in ("r", "w"):
            raise ValueError(f"unknown port direction {direction!r}")
        words.append(f"{port:x}:{value:x}:{direction}")
    return " ".join(words)


def out_name(src):
    return src.stem.replace(" ", "_") + ".txt"


def convert(src, out, cases, source):
    with open(src, encoding="utf-8") as f:
        tests = json.load(f)
    if cases is not None:
        tests = tests[:cases]
    lines = [
        f"# Z80 SingleStepTests (Tom Harte, MIT licence), {src.stem}",
        f"# source: {source}",
        f"# {len(tests)} cases; written by tests/data/z80/convert_harte.py",
    ]
    for test in tests:
        lines.append(f"# {test['name']}")
        lines.append(state_line("I", test["initial"]))
        lines.append(state_line("F", test["final"]))
        lines.append(cycles_line(test["cycles"]))
        if test.get("ports"):
            lines.append(ports_line(test["ports"]))
    (out / out_name(src)).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return len(tests)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("src", type=pathlib.Path)
    parser.add_argument("out", type=pathlib.Path)
    parser.add_argument("--cases", type=int)
    parser.add_argument("--source", default="https://github.com/SingleStepTests/z80 v1")
    args = parser.parse_args()
    files = sorted(args.src.glob("*.json")) if args.src.is_dir() else [args.src]
    if not files:
        sys.exit(f"{args.src}: no JSON files")
    args.out.mkdir(parents=True, exist_ok=True)
    for src in files:
        count = convert(src, args.out, args.cases, args.source)
        print(f"{src.name}: {count} cases")


if __name__ == "__main__":
    main()
