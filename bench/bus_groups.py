#!/usr/bin/env python3
"""Counts what a Bus kept as one entity would save (docs/PERFORMANCE.md,
"Bus as one entity"). A count of events, not a speed: nothing in the kernel changes.

Runs a netlist with every Net recorded, then gives, per simulated µs and per Bus:
  changes      Net changes of its lines
  groups       sets of its lines changing at one timestamp: one event each if the Bus were one
               entity with its timing unchanged
  same chips   the same, grouping only lines that join the same chips (one chip's Bus pins fully
               connected to another's)
  transitions  groups within 10 ns of a transition's first taken as one (rise and fall delays
               land the rising and the falling bits apart)
A Bus is the Nets named PREFIX<number>, two or more (A0-A15); --buses names the prefixes instead.

Run from the repository root, release built:
  bench/bus_groups.py bench/bus_load.bwn [--time 60ms] [--seed 1] [--buses A,D,MD]
Options it does not know go to `bw run` (--type J3:100ms:HI).
"""

import argparse
import collections
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TRANSITION_PS = 10_000
BUSIEST = 5


def sanitize(name):
    """A Net's name as the VCD writer spells it (src/sim/vcd.cpp)."""
    return re.sub(r"[^A-Za-z0-9_.]", "_", name)


def chips_by_net(netlist):
    """Net name -> the chips with a Pin on it."""
    chips = collections.defaultdict(set)
    for line in pathlib.Path(netlist).read_text().splitlines():
        tokens = line.split("#", 1)[0].split()
        if tokens and tokens[0] == "net":
            chips[sanitize(tokens[1])].update(pin.split(".", 1)[0] for pin in tokens[2:])
    return chips


def run(bw, netlist, time, seed, extra, vcd):
    """Runs the netlist with every Net recorded; gives bw's counters."""
    command = [str(bw), "run", str(netlist), "--time", time, "--seed", seed]
    result = subprocess.run(
        command + ["--record-all", "--vcd", str(vcd)] + extra, capture_output=True, text=True
    )
    if result.returncode != 0:
        raise SystemExit(f"bus groups: bw run failed\n{result.stdout}{result.stderr}")
    counters = {}
    for line in result.stdout.splitlines():
        key, _, value = line.partition(" ")
        counters[key] = value
    return counters


def bus_of(names, prefixes):
    """Net name -> its Bus (the name's prefix), for the Nets that are lines of a Bus."""
    prefix_of = {}
    for name in names:
        match = re.fullmatch(r"(.*\D)(\d+)", name)
        if match:
            prefix_of[name] = match.group(1)
    if prefixes is None:
        sizes = collections.Counter(prefix_of.values())
        prefixes = {prefix for prefix, lines in sizes.items() if lines >= 2}
    return {name: prefix for name, prefix in prefix_of.items() if prefix in prefixes}


class Counts:
    def __init__(self):
        self.changes = collections.Counter()      # per Bus
        self.groups = collections.Counter()       # per Bus
        self.pieces = collections.Counter()       # per Bus: same-chip groups
        self.transitions = collections.Counter()  # per Bus
        self.single = collections.Counter()       # per Net outside a Bus
        self.nets = 0                             # recorded
        self.end_ps = 0


def count(vcd, chips, prefixes):
    """Reads the VCD once; gives the Counts and each Net's Bus."""
    ids = {}
    bus = {}
    value = {}
    counts = Counts()
    transition_start = {}
    now = 0
    changed = collections.defaultdict(set)  # at this timestamp: Bus -> same-chip pieces

    def close_timestamp():
        for name, pieces in changed.items():
            counts.groups[name] += 1
            counts.pieces[name] += len(pieces)
            if name not in transition_start or now - transition_start[name] > TRANSITION_PS:
                counts.transitions[name] += 1
                transition_start[name] = now
        changed.clear()

    with open(vcd) as lines:
        for line in lines:
            line = line.rstrip("\n")
            if line.startswith("$var"):
                fields = line.split()
                ids[fields[3]] = fields[4]
            elif line.startswith("$enddefinitions"):
                bus = bus_of(ids.values(), prefixes)
            elif line.startswith("#"):
                close_timestamp()
                now = int(line[1:])
            elif line and not line.startswith("$"):
                name = ids[line[1:]]
                before = value.get(name)
                value[name] = line[0]
                if before is None or before == line[0]:
                    continue  # the first sample, or only the floating flag moved
                if name in bus:
                    counts.changes[bus[name]] += 1
                    changed[bus[name]].add(frozenset(chips[name]))
                else:
                    counts.single[name] += 1
    close_timestamp()
    counts.nets = len(ids)
    counts.end_ps = now
    return counts, bus


def line_number(name):
    return int(re.search(r"\d+$", name).group())


def report(args, counters, counts, bus, chips):
    us = counts.end_ps / 1e6
    if us == 0:
        raise SystemExit("bus groups: the run recorded no time")

    def rate(n):
        return f"{n / us:.1f}"

    def ratio(a, b, digits=1):
        return f"{a / b:.{digits}f}" if b else "-"

    bus_changes = sum(counts.changes.values())
    single_changes = sum(counts.single.values())
    total = bus_changes + single_changes
    groups = sum(counts.groups.values())
    pieces = sum(counts.pieces.values())
    lines = collections.Counter(bus.values())

    print(f"{args.netlist}, {args.time}, seed {args.seed}: per simulated µs")
    print(
        f"applied events {rate(int(counters['applied_events']))}, "
        f"evaluations {rate(int(counters['evaluations']))}, Net changes {rate(total)}"
    )
    print()

    row = "{:<8} {:>5} {:>8} {:>7} {:>11} {:>11} {:>12}"
    print(row.format("Bus", "lines", "changes", "groups", "bits/group", "same chips", "transitions"))

    def bus_row(name, line_count, changes, group_count, piece_count, transitions):
        print(
            row.format(
                name,
                line_count,
                rate(changes),
                rate(group_count),
                ratio(changes, group_count),
                rate(piece_count),
                rate(transitions),
            )
        )

    for name in sorted(lines):
        bus_row(
            name,
            lines[name],
            counts.changes[name],
            counts.groups[name],
            counts.pieces[name],
            counts.transitions[name],
        )
    bus_row("Buses", len(bus), bus_changes, groups, pieces, sum(counts.transitions.values()))
    print(f"{'single':<8} {counts.nets - len(bus):>5} {rate(single_changes):>8}")
    busiest = ", ".join(f"{n} {rate(c)}" for n, c in counts.single.most_common(BUSIEST))
    print(f"busiest single Nets: {busiest}")
    print()

    def fewer(events):
        return f"{rate(total)} → {rate(events)} ({ratio(total, events, 2)}× fewer)"

    print(f"Net changes, each Bus one entity:      {fewer(single_changes + groups)}")
    print(f"Net changes, same-chip lines grouped:  {fewer(single_changes + pieces)}")
    print()

    print("Lines joining the same chips:")
    by_bus = collections.defaultdict(lambda: collections.defaultdict(list))
    for name, prefix in bus.items():
        by_bus[prefix][frozenset(chips[name])].append(name)
    for prefix in sorted(by_bus):
        parts = [sorted(names, key=line_number) for names in by_bus[prefix].values()]
        parts.sort(key=lambda names: line_number(names[0]))
        print(f"  {prefix}: " + " | ".join(" ".join(names) for names in parts))


def main():
    parser = argparse.ArgumentParser(description="Bus bits changing together, from a recorded run.")
    parser.add_argument("netlist")
    parser.add_argument("--time", default="60ms")
    parser.add_argument("--seed", default="1")
    parser.add_argument("--buses", help="Bus name prefixes, comma-separated (default: found)")
    parser.add_argument("--bw", default=str(ROOT / "build/release/src/cli/bw"))
    args, extra = parser.parse_known_args()
    if not pathlib.Path(args.bw).is_file():
        raise SystemExit(f"bus groups: build release first ({args.bw})")
    prefixes = set(args.buses.split(",")) if args.buses else None

    chips = chips_by_net(args.netlist)
    with tempfile.TemporaryDirectory() as directory:
        vcd = pathlib.Path(directory) / "run.vcd"
        counters = run(args.bw, args.netlist, args.time, args.seed, extra, vcd)
        counts, bus = count(vcd, chips, prefixes)
    report(args, counters, counts, bus, chips)


if __name__ == "__main__":
    main()
