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
| 1 | The Typewriter's Nets `GL1`–`GL3` | af03ba6 |
| 2 | LED Chaser, Dice, Stopwatch, Knob Meter | 399e2a3 |
| 3 | Test Pattern, Bouncing Ball, Tennis | this commit |
| 4 | Message, Typewriter | |
| 5 | Heartbeat, LED computer | |
| 6 | BW-1 | |
| 7 | BW-2001 | |
| 8 | BW-80 | |
| 9 | The sections required | |
| 10 | Docs | |

## Implementation notes

- Docs review (before the docs commit), fixed: the Typewriter's Service Manual renamed in phase 1 (an `R3` left would fail the checks); every test text the required sections break listed, `kGoodService` kept and a `kServiceTail` added; ADD_MACHINE §2 changed with the checks (phase 9); the Counter-only sections case folded into the every-Machine case (sections not empty); the macOS `sed -i ''`; the Typewriter docs' lines named; `drc_typewriter` the Board's check, not `place_drc_typewriter`.
- Phase 1: `check-board` ok; release `-R "typewriter|manual"` 36 passed.
- Phase 2: each Machine drafted by a sub-agent, read whole and kept (the Chaser's, Dice's, Stopwatch's and Knob Meter's prose needed no rewrite). Readings checked with `bw run --probe` (Chaser 1–2 s, every 33 ms over one run; Dice idle and with `ROLL` held, `--press`; Stopwatch running, stopped, each button held; Knob Meter at 0 %, 47 %, 100 % around a round's start at x.x425 s) and one analyzer or scope setup each, screenshot viewed; the times are in each design doc's `## Manuals`. Release `-R "manual|chaser|dice|stopwatch|knob"` 73 passed; the Stopwatch's Service Manual viewed. Found for STYLE: a run means the whole range (a pair is "`D1` and `D7`"); a pin name shaped like a reference (the 74LS90's R01, R92) is written plain or as `U8.R01`; button legends in plain capitals (`RESET` is also a Net); the probe's P lamp holds 100 ms; readings that need input say the state ("idle", "`ROLL` held").
- From phase 3 on, the Machines of phases 3–7 were drafted at once (they share no file), the brief carrying phase 2's findings; each phase is still reviewed and committed in ladder order.
- Phase 3: readings with `bw run --probe` (Test Pattern 0.3–1.1 s; Ball 0.5–2 s, the ball's directions every 0.2 s to 3.1 s; Tennis with START pressed, 150 ms–4 s, game over at 55 s, the next START at 57 s, `--vcd` records of 8, 42 and 60 s); analyzer and scope setups for a line and a frame on each, the Ball's ball, Tennis's controllers, screenshots viewed. Every fast TV signal shows P; the tables give the level it mostly reads ("mostly H, with P lit"). Release `-R "manual|test.pattern|ball|tennis"` 71 passed; Tennis's User Manual viewed (the keys table drawn). Seen: Tennis's knobs start at 0 %, both paddles at the bottom, so a fresh game misses every serve (the user's knobs decide; not a manual matter).
