#!/usr/bin/env python3
"""Convert Tom Harte's 6502 SingleStepTests JSON into Bodgewire's compact case format.

Usage: convert_harte.py SRC OUT [--cases N] [--source TEXT]

SRC is a JSON file (one opcode, e.g. a9.json) or a folder of them; OUT is a folder. Each
<hh>.json becomes OUT/<hh>.txt. --cases N keeps the first N cases of each file. --source
TEXT goes into each output's header (repository and commit).

Format (docs/p5c-6502/MOS6502_DESIGN.md, Test data): '#' starts a comment line; a case is
three lines, every number hex:
  I <pc> <s> <a> <x> <y> <p> <address>:<value> ...
  F <pc> <s> <a> <x> <y> <p> <address>:<value> ...
  C <address>:<value>:<r|w> ...
"""

import argparse
import json
import pathlib
import sys


def state_line(tag, state):
    head = [tag] + [f"{state[k]:x}" for k in ("pc", "s", "a", "x", "y", "p")]
    ram = [f"{address:x}:{value:x}" for address, value in state["ram"]]
    return " ".join(head + ram)


def cycles_line(cycles):
    words = ["C"]
    for address, value, kind in cycles:
        if kind not in ("read", "write"):
            raise ValueError(f"unknown cycle kind {kind!r}")
        words.append(f"{address:x}:{value:x}:{kind[0]}")
    return " ".join(words)


def convert(src, out, cases, source):
    with open(src, encoding="utf-8") as f:
        tests = json.load(f)
    if cases is not None:
        tests = tests[:cases]
    lines = [
        f"# 6502 SingleStepTests (Tom Harte, MIT licence), opcode {src.stem}",
        f"# source: {source}",
        f"# {len(tests)} cases; written by tests/data/6502/convert_harte.py",
    ]
    for test in tests:
        lines.append(f"# {test['name']}")
        lines.append(state_line("I", test["initial"]))
        lines.append(state_line("F", test["final"]))
        lines.append(cycles_line(test["cycles"]))
    (out / f"{src.stem}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return len(tests)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("src", type=pathlib.Path)
    parser.add_argument("out", type=pathlib.Path)
    parser.add_argument("--cases", type=int)
    parser.add_argument("--source", default="https://github.com/SingleStepTests/65x02 6502/v1")
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
