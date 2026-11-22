"""Takes the README's screenshots (docs/README_SPEC.md) into docs/readme/.

    cmake --build --preset release && python3 -I tools/readme_shots.py [NAME...]

Each shot is a bw_ui command line (--screenshot, --after); names given: only those. The PNGs
are shrunk to 1280 px wide with sips (macOS) or ImageMagick, else kept full size. Run from the
repository's root. View every picture after: a slow Machine needs more --after on a slower Mac.
"""

import pathlib
import shutil
import subprocess
import sys

BW_UI = pathlib.Path("build/release/src/ui/bw_ui")
OUT = pathlib.Path("docs/readme")
WIDTH = 1280
DARK_LED = "machines/010-counter/puzzles/01-dark-led.bwp"

# name: (arguments, --after). The Dark LED's crack is half way along the Trace from U5 pin 11
# (187.0, 121.92) to D1 pad 1 (197.96, 122.0), Board mm.
SHOTS = {
    "launcher": ([], "1s"),
    "bench": ([DARK_LED], "1s"),
    "traces": ([DARK_LED, "--probe", "U5.11"], "1s"),
    "continuity": ([DARK_LED, "--meter", "U5.11,D1.1"], "1s"),
    "crack": ([DARK_LED, "--zoom", "192.6:122:6", "--focus", "iron"], "1s"),
    "mended": ([DARK_LED, "--repair", "mend:F.Cu:192.6:121.96"], "0.7s"),
    # The tools (README §7). Pencil marks: checks on U1-U4's bodies, a cross on U5-D1's Trace.
    "pencil": (
        [DARK_LED, "--focus", "pencil", "--mark",
         "check:124.5:118.1,check:139.2:118.1,check:153.8:118.1,check:168.5:118.1,"
         "cross:190:122"], "1s"),
    # The 6502 starting: the reset vector fetched (FFFC, FFFD), then F800's first instructions.
    "analyzer": (
        ["machines/110-led-computer/led_computer.bwn", "--group", "U1.A0,U1.D0",
         "--clip", "U1.SYNC:r,U1.PHI2,U1.RW", "--sample", "100ns", "--view", "1us:+3us"], "1s"),
    # Triggered on VIDEO: a visible line's vertical-line pulses, then the horizontal sync.
    "scope": (
        ["machines/050-test-pattern/test_pattern.bwn", "--hook", "J2.VIDEO,J2.SYNC_N",
         "--timebase", "10us"], "1s"),
    # The binder on the BW-2001, 6 s in so BASIC's banner is on the TV (0.25x).
    "binder-manual": (["machines/130-bw2001/bw2001.bwn", "--manual", "service"], "6s"),
    "binder-datasheet": ([DARK_LED, "--probe", "U5.11", "--manual", "datasheet"], "1s"),
    "binder-code": (["machines/130-bw2001/bw2001.bwn", "--contents", "UH7:code"], "6s"),
    "binder-glyphs": (["machines/130-bw2001/bw2001.bwn", "--contents", "UA2:glyphs"], "6s"),
    "counter": (["machines/010-counter/counter.bwn"], "0.7s"),
    "led-chaser": (["machines/020-led-chaser/chaser.bwn"], "1s"),
    "dice": (["machines/030-dice/dice.bwn", "--press", "SW1:300ms:800ms"], "1.5s"),
    "stopwatch": (["machines/040-stopwatch/stopwatch.bwn"], "2s"),
    "knob-meter": (["machines/045-knob-meter/knob.bwn", "--knob", "J2:42%"], "1s"),
    "test-pattern": (["machines/050-test-pattern/test_pattern.bwn"], "1s"),
    "bouncing-ball": (["machines/060-bouncing-ball/ball.bwn"], "1s"),
    "tennis": (["machines/070-tennis/tennis.bwn"], "2s"),
    "message": (["machines/080-message/message.bwn"], "1s"),
    "typewriter": (
        ["machines/090-typewriter/typewriter.bwn", "--press", "SW1:500ms:700ms",
         "--type", "J3:1s:HELLO, WORLD"], "2.5s"),
    "led-computer": (["machines/110-led-computer/led_computer.bwn"], "1s"),
    "bw1": (["machines/120-bw1/bw1.bwn"], "2s"),
    # 0.25x: 1.4 s simulated is 6 s of wall time, then the typing and a screenful.
    "bw2001": (
        ["machines/130-bw2001/bw2001.bwn",
         "--type", 'J5:1.4s:10 PRINT "BODGEWIRE ";\n20 GOTO 10\nRUN\n'], "25s"),
    # 0.16x, about 0.18 s simulated a key. A two-line program entered, not run: at the K cursor
    # O is PRINT and G is GOTO; NEW LINE is \r.
    "bw80": (
        ["machines/140-bw80/bw80.bwn", "--type", 'J3:400ms:10O"BODGEWIRE ";\r20G10\r'], "40s"),
}


def shrink(png: pathlib.Path) -> None:
    if shutil.which("sips"):
        subprocess.run(["sips", "--resampleWidth", str(WIDTH), str(png)], check=True,
                       stdout=subprocess.DEVNULL)
    elif shutil.which("magick"):
        subprocess.run(["magick", str(png), "-resize", f"{WIDTH}x", str(png)], check=True)
    else:
        print(f"{png}: kept full size (no sips or magick)", file=sys.stderr)


def main() -> int:
    names = sys.argv[1:] or list(SHOTS)
    unknown = [name for name in names if name not in SHOTS]
    if unknown:
        print(f"unknown shots: {', '.join(unknown)}; known: {', '.join(SHOTS)}", file=sys.stderr)
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    for name in names:
        args, after = SHOTS[name]
        png = OUT / f"{name}.png"
        command = [str(BW_UI), *args, "--screenshot", str(png), "--after", after]
        result = subprocess.run(command, capture_output=True, text=True)
        lines = (result.stdout + result.stderr).strip().splitlines()
        print(f"{name}: {lines[-1] if lines else ''}")
        if result.returncode != 0:
            print(result.stderr, file=sys.stderr)
            return 1
        shrink(png)
    return 0


if __name__ == "__main__":
    sys.exit(main())
