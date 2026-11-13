# Recipe — Add a Machine

A new board on the tutorial ladder (`docs/machines/NNN-name/`). New Parts first ([ADD_PART.md](ADD_PART.md), [ADD_BOARD_DEVICE.md](ADD_BOARD_DEVICE.md)); Puzzles last ([ADD_PUZZLE.md](ADD_PUZZLE.md)).

Reference commits (chaser, 020): `a9b1f20` netlist + manuals + fault-free test, `be36d29` Board, `cf595a6` Puzzles, `9ba532d` Session test. Dice (030, with a button): `b23b235`, `360a787`, `fad45c7`. Stopwatch (040, digits): `8a18984`, `8d3644a`, `ae506f8`. Bouncing ball (060, TV, picture-reading tests): `174aa85`, `c89e3f0`, `58b1aec`. Knob meter (045, a device behind a Connector): `d24f490`, `73063c0`, `e1468a0`. Paddle tennis (070, 58 chips, netlist grown in six stages, no Puzzles yet): stages `624fe37` to `fc268da`, manuals `892e7e6`, Board `741d2f7`. Message display (080, ROMs, `roms/`): `babed7b` netlist + ROM contents + fault-free test, `1635989` manuals, `3dc540c` Board. TV typewriter (090, a shift-register page written from a keyboard, P4q): stages `d14e18c`, `452af51`, manuals `bc291a0`, Board `2eb59e7`. Heartbeat (100, a 6502 on LEDs): `6344de2` netlist + manuals + fault-free test, `8bbee8d` Board (placed by `bw place`, tidied and routed by the user). LED computer (110, a program in ROM, RAM, a decoder and an output latch): `2e028aa` program + netlist + manuals + fault-free test, `4c34956` Board.

## Order

1. **Docs**: `docs/machines/NNN-name/NAME.md` (what it does, circuit, Puzzles) and `NAME_DESIGN.md` (netlist, Board notes, Puzzles with exact faults, manuals). No code design needed unless new Parts.
2. **Netlist + manuals + fault-free test** (headless, one commit).
3. **Board**: the agent places it with `bw place`, the user routes it in KiCad, the agent wires the tests (one commit).
4. **Puzzles** (needs the Board for cracks).
5. **Session test + release check**, then the docs rows.

## 2. Netlist, manuals, fault-free test

- `netlists/<m>.bwn`:
  - header comment: Machine number, spec path, one-paragraph circuit summary;
  - `part REF TYPE [param=value]` (types from `src/parts/catalogue.cpp`; `OSC freq=1MHz`, `R value=330`); `part J1 POWER` always (joins `VCC`/`GND`); `part J2 VIDEO` for TV Machines (the Session adds the TV by itself);
  - `net NAME REF.PIN …` with pin names from the datasheet; the same `net` name may repeat on several lines; rails are `VCC` and `GND`; short comments above groups;
  - ROMs: `part U14 2716 contents=../roms/<m>.rom`, the contents file in `roms/` (relative to the netlist's directory);
  - a program: `roms/<m>.asm` (header comment: Machine, spec path, memory map; syntax in `src/asm/asm6502.hpp`), then `./build/release/src/cli/bw asm roms/<m>.asm -o roms/<m>.rom`; commit both. `asm: every ROM program matches its committed .rom` and the ctest `asm_<m>` fail when they disagree. Let the test read the vectors from the `.rom` (`ReadRomContents`) rather than hard-code addresses; a label named `a`, `x` or `y` reads as a register;
  - references: `U` chips, `R` resistors, `D` LEDs, `DS` digits, `SW` buttons, `X` oscillator, `J` connectors.
- `manuals/<m>/user.md`: `# Name` then `## What it does`: one short paragraph, seen from outside only (power hookup, what the player sees). Backticked references must exist.
- `manuals/<m>/service.md`: `# Name — Service Manual` then `## Components`: one bullet per reference, every Part, netlist order: `` - `U1` short role: detail with `NET` names ``. Write addresses as plain `$F800`, not in backticks: a backticked `F800` reads as an unknown reference.
- `tests/machines/<m>_test.cpp` (add to `tests/CMakeLists.txt`): `FaultFree()` = `LoadNetlistMachine(BW_NETLIST_DIR / "<m>.bwn")`; `Record(machine, end, names)` = `machine.build()` + `Simulator` + `powerOn(1)`; check the visible behaviour after settling. Tag `[machines][slow]` if it simulates more than ~1 s. Copy `tests/machines/chaser_test.cpp`.
- A big Machine (070): grow the netlist in stages, each one a commit with its own picture cases, earlier cases kept green. Tie the unused gates of each stage's chips. Measure release headless speed (`bw run … --time 2s`) at every stage. A slow drop points at a signal switching every dot across the screen; gate it to where it is needed.
- Add the Machine to the list in `tests/machines/manuals_test.cpp` (`every Machine's manuals pass their checks`).

## 3. Board (user + agent)

- Agent: write `boards/<m>/<m>.place` from the Machine design's panel parts (connectors, LEDs, digits, buttons and the resistors beside them; grammar in [BOARD_PLACEMENT.md](../p4k-board-placement/BOARD_PLACEMENT.md#what-is-new)), then `./build/release/src/cli/bw place netlists/<m>.bwn boards/<m>/<m>.place -o boards/<m>/<m>.kicad_pcb`. It writes the Board with every part placed (the chips annealed on wire length) and a `.kicad_pro` if there is none, and prints the wire length and the grid. The `place_drc_<m>` ctest appears by itself (reconfigure). Release takes under a second on 070; debug about 20 s.
- User: opens `<m>.kicad_pro` and routes it ([KICAD_GUIDE.md](../p2a-board-layout/KICAD_GUIDE.md) steps 7–8: F.Cu/B.Cu only, no zones, DRC 0 errors), keeping any route order a crack Puzzle needs. Placement is not theirs to change; if it looks wrong, fix the `.place` and run `bw place` again. On a routed Board, `bw place` refuses without `--force`.
- Freerouting may leave one connection (P2a, 080: a ROM pad boxed in by its neighbours' tracks). DRC says "1 unconnected items" and `check-board` prints `open NET: …`. The agent can add it: find a free path from the tracks around it, write the segments and vias into a scratch copy of the `.kicad_pcb` (KiCad closed), and check it with `kicad-cli pcb drc` and `check-board` before copying it back.
- Agent, once it is saved: read the routing order of every Net a crack Puzzle needs (ADD_PUZZLE.md) before writing Puzzles. A "pad at an end of the route" constraint is easy to miss in KiCad (060 `V2`); a crack on a two-pad Net needs no constraint at all.
- Agent: `./build/debug/src/cli/bw check-board netlists/<m>.bwn boards/<m>/<m>.kicad_pcb` must print `ok`. Commit only `<m>.place`, `<m>.kicad_pro` and `<m>.kicad_pcb`.
- The `drc_<m>` ctest appears by itself (glob in `tests/CMakeLists.txt`; reconfigure).
- Add `"<m>"` to the per-Machine loops:
  - `tests/board/boards_test.cpp`: a `<m> matches its netlist` case + the courtyard loop;
  - `tests/machine/electrical_test.cpp`: `uncracked board gives the design Nets`;
  - `tests/view/board_scene_test.cpp`: `outline of every Board is a rectangle`, `every Trace and via has a design Net`.
- Switch `<m>_test.cpp` to `LoadNetlistMachine` if it was using `LoadNetlistFile`. Keep the netlist (`LoadNetlistFile`) for the `loads without warnings` case and build the others from the Board (`BoardCircuit`, as in `ball_test.cpp`); seeds that a case searches still hold, hard-coded ones do not.

## 5. Session test and release check

- `tests/session/session_test.cpp`: a `[session]` case running the Machine through `Session` (cf. `session: chaser with every instrument`); this checks the `kMaxWatched` budget.
- Release: `cmake --build --preset release && build/release/src/ui/bw_ui puzzles/<one>.bwp --screenshot /tmp/<m>.png --after 1s`, attach every instrument (`--probe`, `--clip`, `--hook`, `--meter`), and check the ratio stays 1.00× and the UI time stays under ~1 ms (about 1.5 ms on the 58-chip 070 Board). View the PNG. A Machine without Puzzles runs from its netlist (`bw_ui netlists/<m>.bwn`, the Board is found by itself). `--press` held for less than a UI frame at the very start is missed: press after 300 ms.
- Docs: roadmap row in `docs/ARCHITECTURE.md` (done + date + one-line summary), user check pending until the user plays it.

## Gotchas

- Clocks decoded from the dot counter (090 `PHI1_N`, `PHI2_N`, `ROW_CLK`): make each edge come from one signal, and check that the gating signals (`ACTIVE`, line decodes) change only while that signal holds the gate off; the delays are in `src/parts/delays.hpp`. Keep the counter's lowest bit, which changes every dot, off gates where a slower bit will do: at 4 MHz each gate on it costs several evaluations a character.
- A keyboard key written when its cell comes round (090): the `KEYBOARD strobe=` must outlast the longest wait (a frame) plus the time to re-arm, or keys are lost.
- Seeded random power-on: the fault-free test must not depend on the seed (settle first, or check every phase).
- Measure speed on an idle machine: a debug `ctest -j 12` running alongside makes a release `bw_ui` read 0.8× for a 1.00× Machine.
- Debug is about 0.07× real time (a TV Machine at 2 MHz, 0.03×): keep simulated time short, and use the release build for screenshots that show readings.
- A 74LS161's `RCO` follows `ENT` combinationally: with a decoded signal on `ENT` (a line end), `RCO` pulses for ~20 ns when the count reaches 15 before that signal falls. Never clock or clear anything from such an `RCO`; put the decoded enable on `ENP` and keep `ENT` for the carry chain (060 `VRCO`).
- A processor Machine: a `POR` on `RES_N` (open collector, with the pull-up and the Reset button on the same Net) starts the CPU at its reset vector; without one it runs from a random state. Gates start with random outputs too, so a strobe built from them can pulse once in the first ns: count strobes only after the reset.
- Power-on draws a random bit per output and Net, and every new chip changes every seed's draws: search seeds in the test (a `FindSeed` over the state the case needs), never hard-code them.
- Picture-reading cases that simulate several seconds cost minutes in debug (070: 4–10 s each, the debug suite about 9 minutes); keep each case to what it shows.
- The logic probe shows no level on a pin with no copper (an unconnected output, 100 `U1` `RW`, 080 `U1` `QA`): hook the scope on it instead. Seen in P5c; to be fixed in P5e.
- Board re-layouts move cracks: Puzzles use `fault crack A B N%`, so only which far pads lose the signal may change.
