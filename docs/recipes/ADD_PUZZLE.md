# Recipe — Add a Puzzle

A hand-placed fault on an existing Machine. Content work: no new code.

Reference commits: `cf595a6` (four chaser Puzzles), `fad45c7` (dice), `b1c73bc` (BW-1 RAM Puzzles, the self-test as a symptom). Crack placement (P4e, `fault crack A B N%`): under Faults below.

## Design rules

- The fault is physical (a pin, a Trace, a joint), never a scripted symptom ([GAME_DESIGN.md](../GAME_DESIGN.md#core-principle)).
- The symptom is visible from outside (LEDs, TV) with the fault-free behaviour known from the User Manual.
- The player has a fair way to find it with the probe and the Board. Hard because the Machine is complex is fine; hard because the clue is missing is a bug.
- Seed-independent: the symptom shows for every power-on seed.
- A memory Fault belongs where the Machine shows the memory (a character or message ROM, video RAM): in program memory it mostly crashes the program, which only tells the player "swap". Check which words the program really uses: an address line that aliases unused words shows nothing.
- On a CPU's bus a cut data line keeps its bus charge (the last value driven or pulled): where only resistors drive it (the Heartbeat, now the fixture `tests/data/heartbeat`) a crack shows nothing; use `stuck` on the CPU's pin. A Fault that sends a CPU wandering through the open bus and random RAM depends on the power-on seed. `bw_ui` always powers on with seed 1 (another only from the Debug panel), so the brief describes seed 1; run other seeds too (`bw run --seed N`) as a robustness check, note what they do in the Machine design and test a seed of each kind (110 `blackout`).
- Each Puzzle on a Machine should teach something different (stuck output, stuck input, broken trace, bridge, dead rail, dry Joint).
- Showing a fault may take an action the manuals document: pressing a button, typing, running a self-test (BW-1: "it works, but the self-test fails" is a Puzzle). The User Manual then says how to run it and read it (BW-1: the self-test at the end of What it does): the player reads both manuals in the binder, but the Service Manual only lists roles: how to run and read a test belongs in the User Manual.
- A self-test points at a place, not a part: pair a Puzzle where the swap it suggests repairs it with one where that swap fails (BW-1 `dead_machine` and `wrong_suspect`: the same bad bit, a chip and a cracked Trace).
- A stuck cell shows only against what the program writes there: a vector written `$00` needs a stuck 1; a ROM's unused fill is `$FF`, so a cell there needs a stuck 0. A fault on the CPU's bus also breaks the ROM fetches, so a self-test in ROM cannot run either.
- A wandering CPU is not always seed-dependent: a few faults land on the same wrong state every seed (BW-1 `aliased_page` starts on `1D27`, `half_byte` on `PA55`). Run seeds 1–3 before writing "random" into a brief.
- A fault on a signal the program reads belongs on that signal's path (the chip that reads it, its Trace), not in the program that reads it: a ROM bit in a waiting loop is a scripted symptom in disguise (BW-2001 `snow`: the VIA's retrace input, not the editor's wait).
- A memory fault in what the self-test reports through (a video RAM column) garbles the report too: fine, the garbling is a clue (BW-2001 `garbled_text`); check the report as it shows.
- A character ROM cell changes one dot: a lit dot in a glyph's empty space shows; a gap in a stroke is hidden by its lit neighbours on the TV (BW-2001 `odd_e`).
- The binder's Contents page shows a ROM's stuck cell or column as the chip holds it (P6p): a fault the views give away at a glance needs a symptom that already points at that chip (BW-2001 `odd_e`: one glyph wrong; BW-1 `rom_test_fails`: the self-test names the ROM's sum; BW-1 `odd_seven`: only by finding the segment table and decoding it). A pin Fault does not show there (LED Computer `blackout`).
- A fault that misbehaves on every seed but differently (BW-80: the NOP condition ignoring `MD6` draws the cursor in the top right corner on seeds 2 and 3, not on seed 1) is out, as a seed-dependent one; a dead one whose clue differs by seed loses the clue (BW-80 `white_screen`: the memory side of the bus moves on seeds 2 and 3 only, so the clue is the CPU side still while the clock runs).
- A chain's fault on a Machine whose CPU draws the picture (BW-80) shows in the picture while the display file in RAM stays right: the case compares a locked frame with the page the display file predicts through the fault's glyph set (rows permuted for a line counter bit, codes mapped for a latch bit), and the cells no code is loaded into stay blank whatever the chain does to a space.
- Two Faults in one Puzzle (a `fault` line each; `completed()` wants every one gone), from the stopwatch on. Most are masking: one Fault hides the other, so the first Repair changes the symptom and the player reads the new one (stopwatch `second_opinion`: the seconds digit unpowered hides its cracked segment). A same-symptom double, where the first Repair changes nothing and the player must look again as on a real vintage bench, is a rare spice: at most one per Machine, never two in a row on the ladder (the user, 2026-11-28). Two Faults with two separate symptoms are two Puzzles stapled together. Don't stage a gotcha from a single Puzzle: its exact Fault and Repair again, plus a hidden second, does not land (the player redoes a known fix, bored, then is told it was a trap); a symptom like another Puzzle's is fine (later Machines have many black screens). Mix the kinds: a crack is welcome, but not two cracks every time (a crack with a stuck pin, a dry Joint, a bridge or a cell). The brief warns without counting ("a little trickier than it looks"): a player who repairs one and still sees a fault must not undo the good Repair. The test checks each stage, and the other Repair order (not completed after either one alone). The Faults' own single Puzzles' crack helpers serve (`DarkSecondsCrack()`, `BrokenBarCrack()`).
- Pin Faults (stuck input, dry Joint) show nothing on the Board: the clue is the leg against the pad (a touch on a chip pad reads the leg with F.Cu active, the pad with B.Cu). A stuck input's leg reads the right signal while the chip acts wrong; a dry Joint's leg differs from its pad.

## File: `machines/NNN-name/puzzles/NN-name.bwp`

```
# Gap: the LED_A5 Trace from R6 pin 2 to D5's anode is cracked; D5's anode floats.
machine chaser
title "Gap"
brief "The light runs along the row, but D5 never lights: the light skips it."
fault crack R6.2 D5.2 51%
```

- File name: `NN-` its place among the Machine's Puzzles, easiest first and in the order of the Machine doc's Puzzle list (the Launcher sorts by name; one inserted between two takes a number ending in 5, as a line in an old BASIC program, `025-` between `02-` and `03-`, or the later ones are renumbered: saved progress keys a Puzzle by its name without the number, `PuzzleName`, so neither loses a tick), then the name in lowercase with hyphens, unique in its Machine (`ListMachines` leaves out a second one) and never changed once published: a renamed Puzzle loses the player's tick. The number is never shown to the player; tests find a Puzzle by its name alone (`test::PuzzlePath("gap")`, `tests/paths.hpp`).
- `machine`: the Machine's netlist name; the Puzzle must sit in that Machine's `puzzles/` (the Launcher leaves out, and reports on stderr, a Puzzle naming another Machine or failing to parse).
- The Launcher (P7a) lists a Machine's Puzzles in file name order with their `title` and `brief`: both are needed (`machine list: the repository` checks). Since P7b each is a post-it 190 px wide, every one the same height: a one-line title and eight lines of brief (about 160 characters; 95 % of today's fit, the longest, the BW-80's Blank page, 204, scrolls with the wheel). Keep the brief short, the title on one line (about 18 characters), and remember that the Launcher runs the Puzzle untouched from power-on, so a symptom that needs a key or a knob does not show there: the brief says what to do. `bw_ui --select machines/NNN-name/puzzles/NN-name.bwp --screenshot /tmp/x.png --after 2s` shows it.
- Line 1: comment for authors only: the cause and why it gives the symptom.
- `brief`: what the owner sees, never the cause.
- Faults (pins by number or name):
  - `fault stuck U5.QD 0`: chip output stuck at 0/1; repaired by swapping the chip;
  - `fault stuck U8.D 0`: chip input stuck at 0/1: the chip reads the value, its Net and the other pins on it are unchanged; repaired by swapping the chip;
  - `fault stuck UA5.PB5 0 input`: the same on a pin that can also drive (a port pin): the chip reads the value, the Net is left alone, the pin drives nothing; without `input` such a pin forces its Net;
  - `fault joint U15.13`: dry Joint on a chip signal pin: the pin is off its pad's Net (an input floats high, an output drives only its leg); repaired by resoldering, not by a swap;
  - `fault crack A B N%`: Broken Trace on the Copper between pads A and B, `N%` along its crackable straight stretches; repaired by mending. Two different pads joined by Copper on the fault-free Board; `N` from 0 to 100, decimals allowed; the Layer is that of the Trace it lands on. The path is the Copper from A to B with the fewest pieces; its crackable stretches (straight Trace whose gap touches no other Copper: away from pads, vias, corners, junctions; never arcs) are put end to end, `0%` nearest A. The crack must separate A from B, or the Puzzle does not load (as for pads not joined or nothing crackable). A new layout keeps the crack between the same two pads, so it breaks the same connection; only which other pads lose the signal may change;
  - `fault bridge U5.3 U5.4`: solder bridge between neighbouring pins of one chip (not a resistor's or a connector's); repaired by the Cut tool.
  - `fault cell U3.IO2 $1FF 0`: inside a RAM or ROM (2114, 2716, 2513, 2316B, 6102, 6530), bit `IO2` of word `$1FF` (the chip's own address: its `A` pins, bit 0 first; `$` hex or decimal) always reads 0; writes do not change it; repaired by swapping the chip;
  - `fault column U15.O1 0`: the same bit stuck in every word, inside the chip: the pin is fine while another chip drives the line; repaired by swapping;
  - `fault cell U2.D3 ram $3F 0`, `fault column U2.D0 rom 1`: a chip with several memories (the 6530's `rom` and `ram`) needs the memory's name before the word; the word is that memory's own address (`ram` `$00`–`$3F`); a chip with one memory takes no name;
  - an internal address line (the chip ignores `A0`, words pair up) is a stuck input: `fault stuck U3.A0 0`;
  - `fault switch SW1 closed` (stuck closed: joined from power-on, a press changes nothing) or `fault switch SW1 open` (never closes): on a `BUTTON` or `KEY`; repaired by swapping the switch. A stuck-closed key in a scanned matrix shows on every scan, and with two other held keys can make a ghost.
  - `fault clock X1 3MHz` (a Wrong Clock, P8k): the `OSC` can runs at that frequency (as `freq=` takes it), every chip right but at the wrong pace; the can on the Board still shows its design marking, so the player measures it (the scope's band reads the frequency); repaired by swapping the can. The simulation's cost follows the clock: faster only where the Machine keeps at least 2.5× real time natively at the new rate (the Counter, Stopwatch, Knob Meter at 3×: 4.0×, 3.6×, 5.9×), slower anywhere. On a TV Machine a slower can takes the line out of the TV's hold (66 µs): the picture tears, its letters slanted or stretched (Message `slanted_page`, Typewriter `giant_letters`); at half speed the BW-2001 stays black. Never drawn at random (P8d): it shows on every Machine to Vetting, even where the player cannot see it (the Dice).
- Check it loads and shows the symptom: `./build/release/src/cli/bw run machines/NNN-name/puzzles/NN-name.bwp --time 2s --probe U5.QD` (or `bw_ui` release with `--screenshot`). `--probe` reads the leg (the pin's own Net). `--leds` prints each lamp as the eye sees it at the end of the run, 0.00 dark to 1.00 lit over the last 20 ms (`led D5 0.00`, a digit's segments `led DS1.a 1.00` to `led DS1.dp`): a blink needs several runs with other `--time`s.
- A typed command on a ZX80-like editor: the first key after a report is taken to leave the report, so a case types one command from a fresh power-on.
- `bw_ui --press` is read once per UI frame: hold a scripted press at least 100 ms (`--press SW1:50ms:200ms`), or it may be missed. `bw run --tv x.bmp` writes the last TV frame alone, quicker to read than a cropped screenshot. Convert it to view it (`sips -s format png x.bmp --out x.png`).
- A crack or dry Joint leaves a TTL input high, but a Net with only MOS inputs (2513, 2518, 2524) reads X, a random bit at every read: on a data or control input that gives noise, not a symptom. Cut a MOS clock (it never rises: a 2524 fades), or use `stuck` on a MOS input (typewriter `lookalikes`, `copies`).
- `stuck` on a 6520/6522 port pin drives its Net (the port is bidirectional); a port pin cracked off reads high (its pull-up). For a port pin whose input has failed, add `input` (below).
- A brief is one quoted string: no `\"` inside (write "the double quote").
- `bw_ui --after` is wall time: on a slow Machine (BW-2001 0.15× in `bw_ui`) 3.5 s reaches about 0.5 s simulated; `bw run --tv` is quicker for trying Faults.
- A short Trace stub at a large pad may have no crackable point: pick other pads or another `%`.
- A crack only cuts one pad off alone if that pad is at an end of the routed Trace. Read the routing order first (the `segment`s of the Net in the `.kicad_pcb`: a pad touched by one segment is an end, by two or more a junction; a via can be the junction); with the pad in the middle, crack on the other side and write the Symptom for the pads that float together (stopwatch `nineteen`). Then probe both sides (`bw run --probe A.PIN,B.PIN`: `-` is floating) to see which pads the position really cuts off (BW-80 `rolling_screen`: 90 % cuts the latch alone, 10 % the keyboard buffer too).

## Tests (in `tests/machines/<machine>_test.cpp`)

A Fault can be tested before its Puzzle file exists: `InlinePuzzle("machine led_computer\nfault cell U4.IO4 $1FF 0\n")` (`tests/machine/test_points.hpp`).

- Add the name to `<machine>: Puzzles have a title and a brief`.
- Counts pinned by tests: `puzzle: every Puzzle's faults format back` (every `fault` line in `machines/`), `machine list: the repository` (the Counter's and the BW-2001's Puzzles, the Counter's titles) and `machine list: next puzzle` (the Counter's last Puzzle).
- One `TEST_CASE("<machine>: <name>", "[machines]")` (add `[slow]` if it simulates more than ~1 s):
  1. `LoadPuzzle("<name>")`, `CHECK_FALSE(machine.completed())` (Repair Validation, P8c), record, check the symptom (and the floating/stuck Net directly);
  2. repair (`machine.swap("U5")`, `machine.mend(XCrack().layer, XCrack().at)`, `machine.removeBridge({"U5","3"},{"U5","4"})`, `machine.resolder({"U15","13"})`), `CHECK(machine.completed())`, check the fault-free behaviour is back. A tempting wrong Repair tried first is checked `CHECK_FALSE(machine.completed())`. A Puzzle that is completed as loaded (a crack on a Trace with a second path) is a bug: `repair validation: no Puzzle completed as loaded` catches it.
- On a damaged Board the Nets are named from the Copper, not the netlist: check a fault by comparing pin Nets (`*PinNet(*loaded, "U12", "C") == *PinNet(*loaded, "U12", "LT_N")`), not `loaded->nets.at("L2")`.
- A crack Puzzle: add a cached `const Crack& XCrack()` (`static const Crack crack = PuzzleCrack("<name>");`) to `tests/machine/test_points.{hpp,cpp}`, with a comment naming the Net, pads and Puzzle file.

- Multiplexed digits (BW-1): read a digit while it alone is selected, and its segments by its own pins, not by Net name (`tests/machines/bw1_rig.hpp`: `display()`, `segments()`); a digit stuck selected is read by `segments()`.

## Docs

- Add the Puzzle to the Machine's `machines/NNN-name/docs/NAME_DESIGN.md` Puzzle list and the count in its Files block, and to `NAME.md`'s Puzzles table (and its count, where written).
- The README's Puzzle counts (overall and the Machine's: [README_SPEC.md](../README_SPEC.md) §5 and §8), and its line of fault kinds when a Puzzle brings a new kind.

## A new kind of Fault

When no `fault` line can express it (2026-11-23, review C10):

- `src/machine/puzzle.hpp`: its `…Decl` (with `line`), an alternative of `FaultDecl`; the parser's line in `puzzle.cpp`. `Puzzle::faults` holds every kind, in file order.
- `src/machine/machine.hpp`: its checked form in `FaultSet`. In `machine.cpp`: its `Machine::resolve` (the check, with the error text a Puzzle line reports) and its `Add` into a `FaultSet`. `LoadMachine` and `Machine::inject` need no change.
- Where it acts: in the Copper (`rebuildBoard`, as cracks and bridges), in the electrical netlist (`ElectricalNetlist`, as dry Joints) or in the Components (`BuildFaults`, `src/netlist/builder.hpp`, as stuck pins, cells and switches). A swap or another Repair that removes it erases it from `faults_`.
- `Machine::completed()` (P8c): a Fault in a Component or a Joint is added to its list of what must be gone; one in the Copper is judged by the `Wiring` it leaves, with no change.
- The parser's catch-all error (`expected: fault stuck|crack|…`, `puzzle.cpp`) names the new kind; `puzzle: errors` checks its text.
- Procedural Puzzles (P8d): its line in `FormatFault` (`puzzle.cpp`: what `ParsePuzzle` reads back; `puzzle: every Puzzle's faults format back` checks it), `Machine::check` needs nothing (it visits `resolve`); to be drawn at random, a `FaultKind` with its share in `kFaultWeights` and its draw in `src/machine/fault_draw.cpp` (an object of the Machine, each as likely), and its Repair in `tests/session/procedural_puzzle_test.cpp`. That `Repair` visitor covers every alternative, so a kind never drawn (the Wrong Clock, P8k) needs its line there too, or the test does not compile.
