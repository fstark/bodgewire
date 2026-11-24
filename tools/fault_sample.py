"""Samples random Faults on a Machine and counts which ones show (P8d's research:
docs/p8d-procedural-puzzles/PROCEDURAL_PUZZLES.md).

    cmake --build --preset release
    python3 -I tools/fault_sample.py machines/140-bw80 OUT
    python3 -I tools/fault_sample.py machines/140-bw80 OUT2 --from OUT --time 2500ms \
        --inputs '--type "J3:350ms:10 REM ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"'

Each candidate is a Puzzle of one random `fault` line, its kind drawn first (--counts). It is run
headless (`bw run --tv`) and its last TV frame compared with the fault-free Machine's: same seed,
same inputs, same time. --from runs again, with other inputs or another time, the candidates of an
earlier sample that showed fewer than --under pixels. Nothing is written in the repository: OUT
gets the Machine through symlinks, the candidates (machine/puzzles/), the frames (frames/) and
results.json. Run from the repository's root. Machines with a TV only.
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
KINDS = ("stuck", "joint", "bridge", "crack", "column", "cell")
COUNTS = "stuck=110,joint=60,bridge=60,crack=80,column=16,cell=40"
# A memory Part's data pins and its words (`fault cell`, `fault column`).
MEMORY = {"2114": (["IO1", "IO2", "IO3", "IO4"], 1024),
          "2332": (["O%d" % i for i in range(8)], 4096)}


class Design:
    """A Machine's chips (reference: Part), their pin counts and its Nets' pins."""

    def __init__(self, directory: pathlib.Path):
        netlist, = directory.glob("*.bwn")
        self.stem = netlist.stem
        self.chips: dict[str, str] = {}
        self.nets: dict[str, list[str]] = {}
        for line in netlist.read_text().splitlines():
            words = line.split("#")[0].split()
            if len(words) >= 3 and words[0] == "part":
                if words[2][0].isdigit() or words[2] == "Z80":  # 74LS161, 2114, 6502: a chip
                    self.chips[words[1]] = words[2]
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


def candidate(design: Design, kind: str, rng: random.Random) -> str:
    """One random `fault` line of `kind`; the Machine may still refuse it (a power pin)."""
    if kind == "crack":  # between two pads of one Net, half way
        name = rng.choice([n for n in sorted(design.nets) if len(design.nets[n]) >= 2])
        return "fault crack %s %s 50%%" % tuple(rng.sample(design.nets[name], 2))
    if kind in ("column", "cell"):
        reference = rng.choice([r for r in design.references if design.chips[r] in MEMORY])
        data, words = MEMORY[design.chips[reference]]
        pin = rng.choice(data)
        if kind == "column":
            return "fault column %s.%s %d" % (reference, pin, rng.randint(0, 1))
        word = rng.randrange(words)
        return "fault cell %s.%s $%X %d" % (reference, pin, word, rng.randint(0, 1))
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
    has_memory = any(part in MEMORY for part in design.chips.values())
    for kind in KINDS:
        wanted = counts.get(kind, 0) if has_memory or kind not in ("column", "cell") else 0
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("machine", type=pathlib.Path, help="machines/NNN-name")
    parser.add_argument("out", type=pathlib.Path)
    parser.add_argument("--time", default="300ms", help="simulated time of each run")
    parser.add_argument("--inputs", default="", help="bw run options: --press, --knob, --type")
    parser.add_argument("--counts", default=COUNTS, help="candidates drawn of each kind")
    parser.add_argument("--seed", type=int, default=1, help="of the draw, not of the power-on")
    parser.add_argument("--from", dest="earlier", type=pathlib.Path, help="an earlier OUT")
    parser.add_argument("--under", type=int, default=100, help="pixels: a small difference")
    parser.add_argument("--jobs", type=int, default=6)
    args = parser.parse_args()

    design = Design(args.machine)
    copy = args.out / "machine"
    frames = args.out / "frames"
    (copy / "puzzles").mkdir(parents=True, exist_ok=True)
    frames.mkdir(exist_ok=True)
    for name in (design.stem + ".bwn", "board", "roms"):
        if (args.machine / name).exists() and not (copy / name).exists():
            (copy / name).symlink_to((args.machine / name).resolve())
    run = ["--time", args.time, *shlex.split(args.inputs)]

    golden = frames / "golden.bmp"
    done = subprocess.run([str(BW), "run", str(args.machine / (design.stem + ".bwn")), "--tv",
                           str(golden), *run], check=True, capture_output=True, text=True)
    if "tv_locked true" not in done.stdout:
        print("the fault-free Machine has no locked picture at", args.time)
    gold = pixels(golden)

    if args.earlier:
        earlier = json.loads((args.earlier / "results.json").read_text())["results"]
        jobs = [{"index": r["index"], "kind": r["kind"], "fault": r["fault"]}
                for r in earlier if r["loaded"] and r["diff"] < args.under]
    else:
        counts = {kind: int(count) for kind, count in
                  (pair.split("=") for pair in args.counts.split(","))}
        jobs = draw(design, counts, args.seed)

    def attempt(job: dict) -> dict:
        puzzle = copy / "puzzles" / ("c%04d.bwp" % job["index"])
        puzzle.write_text("machine %s\n%s\n" % (design.stem, job["fault"]))
        frame = frames / ("c%04d.bmp" % job["index"])
        done = subprocess.run([str(BW), "run", str(puzzle), "--tv", str(frame), *run],
                              capture_output=True, text=True, timeout=900)
        job["loaded"] = done.returncode == 0
        if job["loaded"]:
            job["locked"] = "tv_locked true" in done.stdout
            job["diff"] = sum(1 for mine, good in zip(pixels(frame), gold) if mine != good)
        else:
            job["error"] = (done.stderr.strip().splitlines() or [""])[-1]
        return job

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(attempt, jobs))
    (args.out / "results.json").write_text(json.dumps(
        {"machine": design.stem, "time": args.time, "inputs": args.inputs, "results": results},
        indent=1) + "\n")

    print("%-7s %6s %6s %8s %9s %8s %6s" %
          ("kind", "drawn", "loaded", "nothing", "under %d" % args.under, "no sync", "clear"))
    for kind in (*KINDS, "all"):
        mine = [r for r in results if kind in (r["kind"], "all")]
        loaded = [r for r in mine if r["loaded"]]
        big = [r for r in loaded if r["diff"] >= args.under]
        if mine:
            print("%-7s %6d %6d %8d %9d %8d %6d" % (
                kind, len(mine), len(loaded), sum(1 for r in loaded if r["diff"] == 0),
                sum(1 for r in loaded if 0 < r["diff"] < args.under),
                sum(1 for r in big if not r["locked"]), sum(1 for r in big if r["locked"])))


if __name__ == "__main__":
    main()
