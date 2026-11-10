# Recipe — Add a Puzzle

A hand-placed fault on an existing Machine. Content work: no new code.

Reference commits: `cf595a6` (four chaser Puzzles), `fad45c7` (dice). Crack syntax and placement: [CRACK_PLACEMENT.md](../p4e-crack-placement/CRACK_PLACEMENT.md).

## Design rules

- The fault is physical (a pin, a Trace, a joint), never a scripted symptom ([GAME_DESIGN.md](../GAME_DESIGN.md#core-principle)).
- The symptom is visible from outside (LEDs, TV) with the fault-free behaviour known from the User Manual.
- The player has a fair way to find it with the probe and the Board. Hard because the Machine is complex is fine; hard because the clue is missing is a bug.
- Seed-independent: the symptom shows for every power-on seed.
- Each Puzzle on a Machine should teach something different (stuck output, stuck input, broken trace, bridge, dead rail, dry Joint).
- Pin Faults (stuck input, dry Joint) show nothing on the Board: the clue is the leg against the pad (a touch on a chip pad reads the leg with F.Cu active, the pad with B.Cu). A stuck input's leg reads the right signal while the chip acts wrong; a dry Joint's leg differs from its pad.

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
  - `fault stuck U8.D 0`: chip input stuck at 0/1: the chip reads the value, its Net and the other pins on it are unchanged; repaired by swapping the chip;
  - `fault joint U15.13`: dry Joint on a chip signal pin: the pin is off its pad's Net (an input floats high, an output drives only its leg); repaired by resoldering, not by a swap;
  - `fault crack A B N%`: Broken Trace on the Copper between pads A and B, `N%` along its crackable straight stretches; repaired by mending;
  - `fault bridge U5.3 U5.4`: solder bridge between neighbouring pins; repaired by the Cut tool.
- Check it loads and shows the symptom: `./build/debug/src/cli/bw run puzzles/<name>.bwp --time 2s --probe U5.QD` (or `bw_ui` release with `--screenshot`). `--probe` reads the leg (the pin's own Net).
- `bw_ui --press` is read once per UI frame: hold a scripted press at least 100 ms (`--press SW1:50ms:200ms`), or it may be missed. `bw run --tv x.bmp` writes the last TV frame alone, quicker to read than a cropped screenshot.
- A short Trace stub at a large pad may have no crackable point: pick other pads or another `%`.
- A crack only cuts one pad off alone if that pad is at an end of the routed Trace. Read the routing order first (the `segment`s of the Net in the `.kicad_pcb`); with the pad in the middle, crack on the other side and write the Symptom for the pads that float together (stopwatch `nineteen`).

## Tests (in `tests/machines/<machine>_test.cpp`)

- Add the name to `<machine>: Puzzles have a title and a brief`.
- One `TEST_CASE("<machine>: <name>", "[machines]")` (add `[slow]` if it simulates more than ~1 s):
  1. `LoadPuzzle("<name>")`, record, check the symptom (and the floating/stuck Net directly);
  2. repair (`machine.swap("U5")`, `machine.mend(XCrack().layer, XCrack().at)`, `machine.removeBridge({"U5","3"},{"U5","4"})`, `machine.resolder({"U15","13"})`), check the fault-free behaviour is back.
- On a damaged Board the Nets are named from the Copper, not the netlist: check a fault by comparing pin Nets (`*PinNet(*loaded, "U12", "C") == *PinNet(*loaded, "U12", "LT_N")`), not `loaded->nets.at("L2")`.
- A crack Puzzle: add a cached `const Crack& XCrack()` (`static const Crack crack = PuzzleCrack("<name>");`) to `tests/machine/test_points.{hpp,cpp}`, with a comment naming the Net, pads and Puzzle file.

## Docs

- Add the Puzzle to the Machine's `docs/machines/NNN-name/NAME_DESIGN.md` Puzzle list.
