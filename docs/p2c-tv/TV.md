# TV — Specification (P2c)

Third milestone of P2 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Component models and Roadmap). The monitor the player watches a Machine through: it turns the VIDEO connector's signals into a picture, and loses lock like a real set when sync is wrong. Headless and testable; drawing it on screen is the UI's job. Builds on [CONNECTORS.md](../p2b-connectors/CONNECTORS.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md). The TV is an Output Device.

## Decisions

- **Plugged into the VIDEO connector**, never into Nets by name: it reads the Nets on J2's VIDEO and SYNC_N Pins. Outside the Board: not a Component of the Circuit, adds no load or event.
- **Period monitor, functional model**: separate TTL video and composite sync, like the PET 2001's built-in monitor. No RF, no colour, no analog levels.
- **Triggered oscillators, as in real sets.** Horizontal and vertical deflection free-run slightly slow; a sync pulse arriving late enough in the cycle triggers retrace early. Correct sync locks the picture; missing, early or late sync makes it roll, tear or slant on its own. Nothing is scripted.
- **The picture is the beam's whole sweep**, blanking included: a rolling picture shows the black vertical-blanking bar crossing the screen, as on a real set. The UI may crop to a visible area.
- **No signal = black screen** (a monitor, not an RF TV: no snow).

## Scope

In: TV model in the headless library; `bw run --tv FILE` writes the last frame as an image; tests on the test pattern and on synthetic sync.

Out: the UI window, phosphor glow and persistence (UI), brightness/contrast/V-hold knobs (later), LED displays.

## Behaviour

### S1 Sync separation

- Horizontal sync: each falling edge of SYNC_N.
- Vertical sync: SYNC_N continuously low for more than 16 µs (horizontal pulses are ~4 µs); detected the moment the low time passes 16 µs.

### S2 Deflection

| | Free-running period | Sync accepted after | Nominal |
|---|---|---|---|
| Horizontal | 66 µs | 56 µs since the last line start | 64 µs (15.625 kHz) |
| Vertical | 21 ms | 16 ms since the last frame start | 20 ms (50 Hz) |

- A line starts at an accepted horizontal sync, or when the free-running period ends without one. Same for frames with vertical sync.
- Locked: the last 2 frames started on an accepted vertical sync, and in the last completed frame at least 90% of lines started on an accepted horizontal sync (lines inside vertical sync may free-run, as on a real set).

### S3 Picture

- Frame buffer: 512 columns × 320 rows, values 0.0 (black) to 1.0 (white).
- Column = time since line start in steps of 125 ns (512 columns = 64 µs); row = lines since frame start. Times beyond the buffer are dropped.
- Each cell is the fraction of its 125 ns that VIDEO was 1. X counts as 0.5, Z as 0.
- At each frame start, the frame just drawn becomes the completed frame and the drawing buffer is cleared to black.
- The TV processes Net changes as the simulation advances; memory stays bounded over any run length.

### S4 CLI

`bw run <file.bwn> --tv FILE.bmp` writes the last completed frame as a greyscale BMP (opens in Preview). The netlist must have a VIDEO connector.

### S5 Checks

- Test pattern, after 50 ms, several seeds: locked; the completed frame shows the grid: white cells where the golden function of [TEST_PATTERN.md](../p1c-test-pattern/TEST_PATTERN.md) is white, at the column/row the timing predicts (one 1 µs pixel = 8 columns, one line = one row), black elsewhere.
- Synthetic sync, fed directly to the TV model: 50 Hz frames lock; vertical sync missing → frames every 21 ms, not locked (picture rolls); horizontal sync missing → lines every 66 µs, not locked; no signal at all → black frames.

## Done when

- S1–S5 implemented; tests pass in debug and release.
- `bw run netlists/test_pattern.bwn --time 100ms --tv /tmp/tv.bmp` shows the grid in Preview.
