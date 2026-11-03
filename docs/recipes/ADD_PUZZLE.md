# Recipe — Add a Puzzle

A hand-placed fault on an existing Machine. Content work: no new code.

Reference commits: `cf595a6` (four chaser Puzzles), `fad45c7` (dice). Crack syntax and placement: [CRACK_PLACEMENT.md](../p4e-crack-placement/CRACK_PLACEMENT.md).

## Design rules

- The fault is physical (a pin, a Trace, a joint), never a scripted symptom ([GAME_DESIGN.md](../GAME_DESIGN.md#core-principle)).
- The symptom is visible from outside (LEDs, TV) with the fault-free behaviour known from the User Manual.
- The player has a fair way to find it with the probe and the Board. Hard because the Machine is complex is fine; hard because the clue is missing is a bug.
- Seed-independent: the symptom shows for every power-on seed.
- Each Puzzle on a Machine should teach something different (stuck output, broken trace, bridge, dead rail).

## File: `puzzles/<name>.bwp`

```
# Gap: the LED_A5 Trace from R6 pin 2 to D5's anode is cracked; D5's anode floats.
machine chaser
title "Gap"
brief "The light runs along the row, but D5 never lights: the light skips it."
fault crack R6.2 D5.2 51%
```

- Line 1: comment for authors only: the cause and why it gives the symptom.
- `brief`: what the owner sees, never the cause.
- Faults (pins by number or name):
  - `fault stuck U5.QD 0`: chip output stuck at 0/1; repaired by swapping the chip;
  - `fault crack A B N%`: Broken Trace on the Copper between pads A and B, `N%` along its crackable straight stretches; repaired by mending;
  - `fault bridge U5.3 U5.4`: solder bridge between neighbouring pins; repaired by the Cut tool.
- Check it loads and shows the symptom: `./build/debug/src/cli/bw run puzzles/<name>.bwp --time 2s --probe U5.QD` (or `bw_ui` release with `--screenshot`).
- A short Trace stub at a large pad may have no crackable point: pick other pads or another `%`.

## Tests (in `tests/machines/<machine>_test.cpp`)

- Add the name to `<machine>: Puzzles have a title and a brief`.
- One `TEST_CASE("<machine>: <name>", "[machines]")` (add `[slow]` if it simulates more than ~1 s):
  1. `LoadPuzzle("<name>")`, record, check the symptom (and the floating/stuck Net directly);
  2. repair (`machine.swap("U5")`, `machine.mend(XCrack().layer, XCrack().at)`, `machine.removeBridge({"U5","3"},{"U5","4"})`), check the fault-free behaviour is back.
- A crack Puzzle: add a cached `const Crack& XCrack()` (`static const Crack crack = PuzzleCrack("<name>");`) to `tests/machine/test_points.{hpp,cpp}`, with a comment naming the Net, pads and Puzzle file.

## Docs

- Add the Puzzle to the Machine's `docs/machines/NNN-name/NAME_DESIGN.md` Puzzle list.
