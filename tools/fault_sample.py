"""Samples random Faults on a Machine and counts which ones show (P8d's research:
docs/p8d-procedural-puzzles/PROCEDURAL_PUZZLES.md).

    cmake --build --preset release
    python3 -I tools/fault_sample.py machines/140-bw80 OUT
    python3 -I tools/fault_sample.py machines/140-bw80 OUT2 --from OUT --times 2500ms \
        --inputs '--type "J3:350ms:10 REM ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"'
    python3 -I tools/fault_sample.py machines/010-counter OUT3 --times 300ms,600ms,900ms,1200ms

Each candidate is a Puzzle of one random `fault` line, its kind drawn first (--counts). It is run
headless (`bw run`) to each of --times, with --inputs, and what the Machine shows there is compared
with the fault-free Machine's, same seed: the TV's frame (`--tv`) on a Machine with one, else each
lamp's brightness (`--leds`). The table counts the candidates that show:

- nothing: every capture the same;
- small: a TV difference under --under pixels; no lamp off by more than --margin of the
  brightest fault-free lamp;
- dead: a clear difference with the picture lost (no sync), or every lamp dark;
- clear: any other clear difference.

--from runs again, with other inputs or times, the candidates of an earlier sample that showed
nothing or little. Nothing is written in the repository: OUT gets the Machine through symlinks,
the candidates (machine/puzzles/), the TV frames (frames/) and results.json. Run from the
repository's root.
"""

import argparse
import concurrent.futures
import json
import pathlib
import random
import re
import shlex
import struct
import subprocess

BW = pathlib.Path("build/release/src/cli/bw")
KINDS = ("stuck", "joint", "bridge", "crack", "column", "cell", "switch")
COUNTS = "stuck=110,joint=60,bridge=60,crack=80,column=16,cell=40,switch=12"
# A memory Part's data pins, and the words of each of its memories (`fault cell`, `fault column`;
# a chip with one memory takes no name).
OUTPUTS = ["O%d" % bit for bit in range(8)]
MEMORY = {"2114": (["IO1", "IO2", "IO3", "IO4"], {"": 1024}),
          "2332": (OUTPUTS, {"": 4096}),
          "2716": (OUTPUTS, {"": 2048}),
          "2316B": (OUTPUTS, {"": 2048}),
          "2513": (["O1", "O2", "O3", "O4", "O5"], {"": 512}),
          "6102": (["DOUT"], {"": 1024}),
          "6530": (["D%d" % bit for bit in range(8)], {"rom": 1024, "ram": 64})}
QUIET = 0.02  # of the brightest fault-free lamp: under it two lamps look the same


class Design:
    """A Machine's chips (reference: Part), their pin counts, its switches and its Nets' pins."""

    def __init__(self, directory: pathlib.Path):
        netlist, = directory.glob("*.bwn")
        self.stem = netlist.stem
        self.chips: dict[str, str] = {}
        self.switches: list[str] = []
        self.nets: dict[str, list[str]] = {}
        self.tv = False
        for line in netlist.read_text().splitlines():
            words = line.split("#")[0].split()
            if len(words) >= 3 and words[0] == "part":
                if words[2][0].isdigit() or words[2] in ("Z80", "MC3446"):  # 74LS161, 2114, 6502
                    self.chips[words[1]] = words[2]
                elif words[2] in ("BUTTON", "KEY"):
                    self.switches.append(words[1])
                elif words[2] in ("VIDEO", "MONITOR"):
                    self.tv = True
            elif len(words) >= 3 and words[0] == "net":
                self.nets.setdefault(words[1], []).extend(words[2:])
        # Pins of each chip: its footprint's highest pad number on the Board.
        self.pins: dict[str, int] = {}
        reference = ""
        board = directory / "board" / (self.stem + ".kicad_pcb")
        for line in board.read_text().splitlines():
            if found := re.search(r'\(property "Reference" "([^"]+)"', line):
                reference = found.group(1)
            elif found := re.search(r'\(pad "(\d+)"', line):
                self.pins[reference] = max(self.pins.get(reference, 0), int(found.group(1)))
        self.references = sorted(self.chips)
        self.memories = [r for r in self.references if self.chips[r] in MEMORY]

    def has(self, kind: str) -> bool:
        """Whether the Machine has anything a Fault of `kind` can sit on."""
        if kind in ("column", "cell"):
            return bool(self.memories)
        return bool(self.switches) if kind == "switch" else True


def candidate(design: Design, kind: str, rng: random.Random) -> str:
    """One random `fault` line of `kind`; the Machine may still refuse it (a power pin)."""
    if kind == "crack":  # between two pads of one Net, half way
        name = rng.choice([n for n in sorted(design.nets) if len(design.nets[n]) >= 2])
        return "fault crack %s %s 50%%" % tuple(rng.sample(design.nets[name], 2))
    if kind == "switch":
        return "fault switch %s %s" % (rng.choice(design.switches),
                                       rng.choice(("closed", "open")))
    if kind in ("column", "cell"):
        reference = rng.choice(design.memories)
        data, memories = MEMORY[design.chips[reference]]
        memory = rng.choice(sorted(memories)) if len(memories) > 1 else ""
        pin = "%s.%s%s" % (reference, rng.choice(data), " " + memory if memory else "")
        if kind == "column":
            return "fault column %s %d" % (pin, rng.randint(0, 1))
        word = rng.randrange(memories[memory])
        return "fault cell %s $%X %d" % (pin, word, rng.randint(0, 1))
    reference = rng.choice(design.references)  # every chip as likely, then every pin of it
    count = design.pins[reference]
    if kind == "bridge":  # two neighbours of one row
        pin = rng.choice([p for p in range(1, count) if p != count // 2])
        return "fault bridge %s.%d %s.%d" % (reference, pin, reference, pin + 1)
    pin = rng.randint(1, count)
    if kind == "joint":
        return "fault joint %s.%d" % (reference, pin)
    return "fault stuck %s.%d %d" % (reference, pin, rng.randint(0, 1))


def draw(design: Design, counts: dict[str, int], seed: int) -> list[dict]:
    """The sample: `counts` distinct candidates of each kind, in KINDS' order."""
    rng = random.Random(seed)
    jobs: list[dict] = []
    seen: set[str] = set()
    for kind in KINDS:
        wanted = counts.get(kind, 0) if design.has(kind) else 0
        tries = 0
        while wanted and tries < 2000:
            tries += 1
            fault = candidate(design, kind, rng)
            if fault not in seen:
                seen.add(fault)
                jobs.append({"index": len(jobs), "kind": kind, "fault": fault})
                wanted -= 1
    return jobs


def pixels(frame: pathlib.Path) -> bytes:
    raw = frame.read_bytes()
    offset, = struct.unpack_from("<I", raw, 10)
    return raw[offset:]


class Sampler:
    """Runs the fault-free Machine, then candidates, to each capture time."""

    def __init__(self, args: argparse.Namespace, design: Design):
        self.design = design
        self.times = args.times.split(",")
        self.inputs = shlex.split(args.inputs)
        self.under = args.under
        self.copy = args.out / "machine"
        self.frames = args.out / "frames"
        (self.copy / "puzzles").mkdir(parents=True, exist_ok=True)
        self.frames.mkdir(exist_ok=True)
        for name in (design.stem + ".bwn", "board", "roms"):
            if (args.machine / name).exists() and not (self.copy / name).exists():
                (self.copy / name).symlink_to((args.machine / name).resolve())
        self.golden = []
        for time in self.times:
            capture = self.capture(args.machine / (design.stem + ".bwn"), "golden", time)
            assert isinstance(capture, dict), "the fault-free Machine does not run: " + capture
            if design.tv and not capture["locked"]:
                print("the fault-free Machine has no locked picture at", time)
            print("fault-free to %s: %.1f s (%.2fx)" % (time, capture["wall"], capture["ratio"]))
            self.golden.append(capture)
        lamps = [b for capture in self.golden for b in capture.get("leds", [])]
        self.full = max(lamps, default=0.0) or 1.0  # the brightest fault-free lamp
        self.margin = args.margin * self.full

    def capture(self, file: pathlib.Path, name: str, time: str) -> dict | str:
        """What the Machine shows at `time`; bw's last line when it refuses the file."""
        frame = self.frames / ("%s_%s.bmp" % (name, time))
        shown = ["--tv", str(frame)] if self.design.tv else ["--leds"]
        done = subprocess.run([str(BW), "run", str(file), "--time", time, *shown, *self.inputs],
                              capture_output=True, text=True, timeout=1800)
        if done.returncode != 0:
            return (done.stderr.strip().splitlines() or [""])[-1]
        said = dict(line.split(" ", 1) for line in done.stdout.splitlines()
                    if not line.startswith("led "))
        capture = {"wall": float(said["wall_s"]), "ratio": float(said["ratio"])}
        if self.design.tv:
            capture["locked"] = said["tv_locked"] == "true"
            capture["pixels"] = pixels(frame)
        else:
            capture["leds"] = [float(line.split()[2]) for line in done.stdout.splitlines()
                               if line.startswith("led ")]
        return capture

    def attempt(self, job: dict) -> dict:
        puzzle = self.copy / "puzzles" / ("c%04d.bwp" % job["index"])
        puzzle.write_text("machine %s\n%s\n" % (self.design.stem, job["fault"]))
        name = "c%04d" % job["index"]
        captures = []
        for time in self.times:
            capture = self.capture(puzzle, name, time)
            if isinstance(capture, str):
                return job | {"loaded": False, "error": capture}
            captures.append(capture)
        job["loaded"] = True
        if self.design.tv:
            diffs = [sum(1 for mine, good in zip(c["pixels"], g["pixels"]) if mine != good)
                     for c, g in zip(captures, self.golden)]
            job |= {"diffs": diffs, "diff": max(diffs),
                    "locked": all(c["locked"] or not g["locked"]
                                  for c, g in zip(captures, self.golden))}
            job["shows"] = ("nothing" if job["diff"] == 0 else
                            "small" if job["diff"] < self.under else
                            "clear" if job["locked"] else "dead")
        else:
            off = [abs(mine - good) for c, g in zip(captures, self.golden)
                   for mine, good in zip(c["leds"], g["leds"])]
            dark = all(b <= QUIET * self.full for c in captures for b in c["leds"])
            job |= {"leds": [c["leds"] for c in captures], "off": max(off),
                    "diff": sum(1 for delta in off if delta > self.margin)}
            job["shows"] = ("nothing" if job["off"] <= QUIET * self.full else
                            "small" if job["diff"] == 0 else
                            "dead" if dark else "clear")
        return job


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("machine", type=pathlib.Path, help="machines/NNN-name")
    parser.add_argument("out", type=pathlib.Path)
    parser.add_argument("--times", "--time", default="300ms",
                        help="simulated capture times, a run each: 300ms,600ms")
    parser.add_argument("--inputs", default="", help="bw run options: --press, --knob, --type")
    parser.add_argument("--counts", default=COUNTS, help="candidates drawn of each kind")
    parser.add_argument("--seed", type=int, default=1, help="of the draw, not of the power-on")
    parser.add_argument("--from", dest="earlier", type=pathlib.Path, help="an earlier OUT")
    parser.add_argument("--under", type=int, default=100, help="TV pixels: a small difference")
    parser.add_argument("--margin", type=float, default=0.25,
                        help="of the brightest fault-free lamp: a small difference")
    parser.add_argument("--jobs", type=int, default=6)
    args = parser.parse_args()

    design = Design(args.machine)
    sampler = Sampler(args, design)
    if args.earlier:
        earlier = json.loads((args.earlier / "results.json").read_text())["results"]
        jobs = [{"index": r["index"], "kind": r["kind"], "fault": r["fault"]} for r in earlier
                if r["loaded"] and r.get("shows", "small") in ("nothing", "small")
                and ("shows" in r or r["diff"] < args.under)]
    else:
        counts = {kind: int(count) for kind, count in
                  (pair.split("=") for pair in args.counts.split(","))}
        jobs = draw(design, counts, args.seed)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(sampler.attempt, jobs))
    (args.out / "results.json").write_text(json.dumps(
        {"machine": design.stem, "times": sampler.times, "inputs": args.inputs,
         "golden": [{key: value for key, value in capture.items() if key != "pixels"}
                    for capture in sampler.golden],
         "results": results}, indent=1) + "\n")

    print("%-7s %6s %6s %8s %6s %5s %6s" %
          ("kind", "drawn", "loaded", "nothing", "small", "dead", "clear"))
    for kind in (*KINDS, "all"):
        mine = [r for r in results if kind in (r["kind"], "all")]
        loaded = [r for r in mine if r["loaded"]]
        if mine:
            print("%-7s %6d %6d %8d %6d %5d %6d" % (
                kind, len(mine), len(loaded),
                *(sum(1 for r in loaded if r["shows"] == shows)
                  for shows in ("nothing", "small", "dead", "clear"))))


if __name__ == "__main__":
    main()
