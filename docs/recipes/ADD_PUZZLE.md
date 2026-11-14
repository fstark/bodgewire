# Recipe — Add a Puzzle

A hand-placed fault on an existing Machine. Content work: no new code.

Reference commits: `cf595a6` (four chaser Puzzles), `fad45c7` (dice). Crack syntax and placement: [CRACK_PLACEMENT.md](../p4e-crack-placement/CRACK_PLACEMENT.md).

## Design rules

- The fault is physical (a pin, a Trace, a joint), never a scripted symptom ([GAME_DESIGN.md](../GAME_DESIGN.md#core-principle)).
- The symptom is visible from outside (LEDs, TV) with the fault-free behaviour known from the User Manual.
- The player has a fair way to find it with the probe and the Board. Hard because the Machine is complex is fine; hard because the clue is missing is a bug.
- Seed-independent: the symptom shows for every power-on seed.
- A memory Fault belongs where the Machine shows the memory (a character or message ROM, video RAM): in program memory it mostly crashes the program, which only tells the player "swap". Check which words the program really uses: an address line that aliases unused words shows nothing.
- On a CPU's bus a cut data line keeps its bus charge (the last value driven or pulled): where only resistors drive it (100 Heartbeat) a crack shows nothing; use `stuck` on the CPU's pin. A Fault that sends a CPU wandering through the open bus and random RAM is seed-dependent: run several seeds (`bw run --seed N`), write the brief for what every seed shares and test a seed of each kind (110 `blackout`).
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
  - `fault cell U3.IO2 $1FF 0`: inside a RAM or ROM (2114, 2716, 2513), bit `IO2` of word `$1FF` (the chip's own address: its `A` pins, bit 0 first; `$` hex or decimal) always reads 0; writes do not change it; repaired by swapping the chip;
  - `fault column U15.O1 0`: the same bit stuck in every word, inside the chip: the pin is fine while another chip drives the line; repaired by swapping;
  - an internal address line (the chip ignores `A0`, words pair up) is a stuck input: `fault stuck U3.A0 0`.
- Check it loads and shows the symptom: `./build/debug/src/cli/bw run puzzles/<name>.bwp --time 2s --probe U5.QD` (or `bw_ui` release with `--screenshot`). `--probe` reads the leg (the pin's own Net).
- `bw_ui --press` is read once per UI frame: hold a scripted press at least 100 ms (`--press SW1:50ms:200ms`), or it may be missed. `bw run --tv x.bmp` writes the last TV frame alone, quicker to read than a cropped screenshot. Convert it to view it (`sips -s format png x.bmp --out x.png`).
- A crack or dry Joint leaves a TTL input high, but a Net with only MOS inputs (2513, 2518, 2524) reads X, a random bit at every read: on a data or control input that gives noise, not a symptom. Cut a MOS clock (it never rises: a 2524 fades), or use `stuck` on a MOS input (typewriter `lookalikes`, `copies`).
- A short Trace stub at a large pad may have no crackable point: pick other pads or another `%`.
- A crack only cuts one pad off alone if that pad is at an end of the routed Trace. Read the routing order first (the `segment`s of the Net in the `.kicad_pcb`); with the pad in the middle, crack on the other side and write the Symptom for the pads that float together (stopwatch `nineteen`).

## Tests (in `tests/machines/<machine>_test.cpp`)

A Fault can be tested before its Puzzle file exists: `InlinePuzzle("machine led_computer\nfault cell U4.IO4 $1FF 0\n")` (`tests/machine/test_points.hpp`).

- Add the name to `<machine>: Puzzles have a title and a brief`.
- One `TEST_CASE("<machine>: <name>", "[machines]")` (add `[slow]` if it simulates more than ~1 s):
  1. `LoadPuzzle("<name>")`, record, check the symptom (and the floating/stuck Net directly);
  2. repair (`machine.swap("U5")`, `machine.mend(XCrack().layer, XCrack().at)`, `machine.removeBridge({"U5","3"},{"U5","4"})`, `machine.resolder({"U15","13"})`), check the fault-free behaviour is back.
- On a damaged Board the Nets are named from the Copper, not the netlist: check a fault by comparing pin Nets (`*PinNet(*loaded, "U12", "C") == *PinNet(*loaded, "U12", "LT_N")`), not `loaded->nets.at("L2")`.
- A crack Puzzle: add a cached `const Crack& XCrack()` (`static const Crack crack = PuzzleCrack("<name>");`) to `tests/machine/test_points.{hpp,cpp}`, with a comment naming the Net, pads and Puzzle file.

## Docs

- Add the Puzzle to the Machine's `docs/machines/NNN-name/NAME_DESIGN.md` Puzzle list.
