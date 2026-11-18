# P6k — Manuals for every Machine — Implementation Plan

Spec: [MACHINE_MANUALS.md](MACHINE_MANUALS.md). Design: [MACHINE_MANUALS_DESIGN.md](MACHINE_MANUALS_DESIGN.md).

Every batch phase follows the design's "Writing a Machine's manuals" steps 1–6 for each of its Machines; its fence is those checks plus the batch's tests. One commit per phase, only this step's paths (the user edits docs in parallel).

## Phases

### Phase 1 — The Typewriter's Nets `GL1`–`GL3`

The design's "The Typewriter's rename".

With the Service Manual's `U13` and `U23` bullets.

Fence: `./build/release/src/cli/bw check-board machines/090-typewriter/typewriter.bwn machines/090-typewriter/board/typewriter.kicad_pcb` prints `ok`; `ctest --preset release -j -R "typewriter|manual"` (`drc_typewriter`, `boards: typewriter matches its netlist` among them). Commit: `typewriter: Nets R1–R3 renamed GL1–GL3, R1 only the resistor (P6k phase 1)`.

### Phase 2 — The LED Machines: LED Chaser, Dice, Stopwatch, Knob Meter

Fence: `ctest --preset release -j -R "manual|chaser|dice|stopwatch|knob"`; both manuals of each viewed in screenshots. Commit: `manuals: LED Chaser, Dice, Stopwatch, Knob Meter explain how they work (P6k phase 2)`.

### Phase 3 — The first TV Machines: Test Pattern, Bouncing Ball, Tennis

The first analyzer setups (a line's sync, the beam counters).

Fence: `ctest --preset release -j -R "manual|test.pattern|ball|tennis"`; manuals and each analyzer setup's screenshot viewed. Commit: `manuals: Test Pattern, Bouncing Ball, Tennis (P6k phase 3)`.

### Phase 4 — The text Machines: Message, Typewriter

Fence: `ctest --preset release -j -R "manual|message|typewriter"`; screenshots viewed. Commit: `manuals: Message, Typewriter (P6k phase 4)`.

### Phase 5 — The first processors: Heartbeat, LED computer

Fence: `ctest --preset release -j -R "manual|heartbeat|led.computer"`; screenshots viewed. Commit: `manuals: Heartbeat, LED computer (P6k phase 5)`.

### Phase 6 — The BW-1

Fence: `ctest --preset release -j -R "manual|bw1"`; screenshots viewed. Commit: `manuals: BW-1 (P6k phase 6)`.

### Phase 7 — The BW-2001

Fence: `ctest --preset release -j -R "manual|bw2001"`; screenshots viewed. Commit: `manuals: BW-2001 (P6k phase 7)`.

### Phase 8 — The BW-80

Fence: `ctest --preset release -j -R "manual|bw80"`; screenshots viewed. Commit: `manuals: BW-80 (P6k phase 8)`.

### Phase 9 — The sections required

The design's "The sections required", with ADD_MACHINE §2.

Fence: release `-R manual`; debug build, `ctest --preset debug -j -R "manual"`; full release suite. Commit: `manual: How it works, Test points and Troubleshooting required; ADD_MACHINE §2 (P6k phase 9)`.

### Phase 10 — Docs

The design's "Docs (last phase)".

Fence: full release suite. Commit: `docs: P6k done, every Machine's manuals explain it`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The Typewriter's Nets `GL1`–`GL3` | |
| 2 | LED Chaser, Dice, Stopwatch, Knob Meter | |
| 3 | Test Pattern, Bouncing Ball, Tennis | |
| 4 | Message, Typewriter | |
| 5 | Heartbeat, LED computer | |
| 6 | BW-1 | |
| 7 | BW-2001 | |
| 8 | BW-80 | |
| 9 | The sections required | |
| 10 | Docs | |

## Implementation notes

- Docs review (before the docs commit), fixed: the Typewriter's Service Manual renamed in phase 1 (an `R3` left would fail the checks); every test text the required sections break listed, `kGoodService` kept and a `kServiceTail` added; ADD_MACHINE §2 changed with the checks (phase 9); the Counter-only sections case folded into the every-Machine case (sections not empty); the macOS `sed -i ''`; the Typewriter docs' lines named; `drc_typewriter` the Board's check, not `place_drc_typewriter`.
