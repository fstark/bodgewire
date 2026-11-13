# P5e — Instruments for buses

Roadmap step P5e ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the Logic Analyzer and Oscilloscope made fit for a 6502 bus, the items deferred from P3c and P3d, and the logic probe on a pin with no copper. Design: [BUS_INSTRUMENTS_DESIGN.md](BUS_INSTRUMENTS_DESIGN.md). Plan: [BUS_INSTRUMENTS_PLAN.md](BUS_INSTRUMENTS_PLAN.md). Recipe: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

From the 110 LED computer on, faults live on buses: an address line stuck, two data lines bridged, a chip select that never falls. Finding them means watching the 16 address lines, the 8 data lines, `RW` and the clock at once, reading the address as one number, and stopping when the CPU reaches a given address. The analyzer has 8 Channels and 4096 points (410 µs at 10 MHz, 400 6502 cycles), no way to read a bus as a value and no Trigger on one. The scope has 2 Hooks and always shows the Trigger at the first division, so nothing long after an edge can be seen. P3c's user check also asked for deeper memory and for navigation that does not misbehave, and the probe stays dark on a pin with no copper (100 `U1` `RW`) while a Hook on the same pin works (found in P5c).

## User decisions (2026-11-13)

| Question | Decision |
|---|---|
| Analyzer size | 32 Channels × 65 536 points (6.5 ms at 10 MHz); the watch budget `kMaxWatched` 48 → 96 Nets |
| Making a bus | shift-click a pin with the Clip tool clips every pin of that chip sharing its name stem (`A0`–`A15`, `D0`–`D7`) as one named Channel Group; the window can also group or ungroup hand-picked Channels |
| Bus Trigger | a hex pattern with `x` digits, such as `E0xx` or `8000`, ANDed with the other Channels' conditions (bus = `8000` and `RW` = 0) |
| Oscilloscope | 4 Hooks, Trigger source any of the 4, and a Trigger delay (horizontal position) to look well after the edge |

Decided while designing (2026-11-13), for the user to overrule:

- **Channel Group** is the name of a set of Channels read as one value (GLOSSARY): a Bus is the circuit's, a Channel Group the analyzer's view of it. The window calls it a group. Its bit 0 is the pin with the lowest name index (`A0`, `D0`, `1Q`; not the lowest pin number: the 6502's `D7` is pin 26, `D0` pin 33); a hand-made group orders its Channels by their pins' trailing numbers when they are all different, otherwise top row = most significant bit.
- **Name stem**: a pin name with its last run of digits replaced, so `A12` and `A3` share `A#`, `1Q` and `8Q` share `#Q`, `1A3` and `1A1` share `1A#`. Power and no-connect pins never join. A group needs at least 2 pins; shift-click on a pin with no stem partner clips that pin alone. The group is named after the chip and stem: `U1 A`, `U9 Q`. On the 6502 `PHI0`, `PHI1`, `PHI2` share `PHI#` and make a group `U1 PHI` of 3: harmless, kept. Pins of the group already clipped keep their Channel (and leave any group they were in); only the others need free Channels.
- **Bus Trigger as Channel conditions**: a pattern sets each member's condition to `0`, `1` or `any`; the engine is unchanged in kind and checks all 32 conditions at once with bit masks. While grouped, members' conditions are edited only through the pattern (edges on a member are not offered).
- **Clicks**: a click on a clipped pin unclips that Channel (it leaves its group; a group left with one Channel dissolves); a shift-click on a grouped pin unclips the whole group.
- **Colours**: 8 lane colours as today, Channel `c` takes colour `c % 8`; a group's members and their Board tags take the group's colour, by its index (`(g + 2) % 8`): by first Channel, three groups clipped in a row all came out magenta (2, 18, 26).
- **Scope delay**: from 0 to 1000 divisions of the current timebase, 0 by default (today's view: the Trigger at the first division). With a delay, the Sweep's first division starts `delay` after the Trigger, minus one division; the window shows `T + delay` and an arrow where the Trigger is off screen. Typed as a time (`12.5us`) or stepped by one division.
- **Scope colours**: yellow, cyan, magenta, blue (as on bench scopes); bands shrink from 110 to 70 px so 4 fit the same window height.
- **Navigation** (P3c "intermittent bugs"): the waveforms become one canvas, so pan and zoom change the view once per frame before anything is drawn (today rows drawn before the hovered one use the old view); the mouse wheel over the waveforms zooms and never scrolls the window, over the labels it scrolls the rows; `Fit` and `Trigger` buttons (whole Capture; centre on the Trigger at the current zoom). Zoom from 16 points to the whole Capture as today.
- **Discoverability**: the empty analyzer shows "Clip (C), then click a chip pin. Shift-click: the pin's whole bus." The empty scope shows the same for Hooks. The tool buttons in the toolbar get tooltips naming their window.
- **Refusal feedback**: a refusal also shows its reason next to the cursor on the Board for 1.5 s (the banner keeps its 2 s), not only in the banner, which is far from where the player looks. A repair refused with the power on says "Power off first" there.
- **The probe fix** applies to every held tip: the probe and the meter's leads read the pin's own Net on a pad whose copper island the netlist does not name (an unconnected pin), as the Hooks and Clips already do.
- **`bw_ui`**: `--group REF.PIN[:PATTERN]` clips that pin's group as a shift-click does, with an optional bus Trigger; `--hook` takes up to 4; `--delay 10us` sets the scope delay.

## What is new

1. **Analyzer engine** — 32 Channels, 65 536 points (`kChannels`, `kDepth`), Channel bits as 32-bit masks, Triggers checked as masks. Pre-Trigger points 6553, 32 768, 58 982 (10, 50, 90 %). `ParseBusPattern` and `FormatBusPattern` turn a pattern into member conditions and back.
2. **Channel Groups** — in the Session: shift-click (`clipGroup`), hand grouping (`group`, `ungroup`), unclip rules, the bus pattern (`setGroupTrigger`), kept across rebuilds like the Clips.
3. **Analyzer window** — rows for groups (one lane with hex values, a pattern box, expandable to members) and Channels; one canvas for the waveforms; the value under the cursor in each row's label; `Fit`, `Trigger`; hand grouping by selecting rows; the empty hint. Waveform columns cached per Capture and view, so 32 lanes over 65 536 points draw within the UI budget.
4. **Oscilloscope** — 4 Hooks (`kScopeChannels`), Trigger on any, `ScopeSettings::delay`, the Sweep's delay for the window, the window's delay field and off-screen Trigger arrow.
5. **Watch budget** — `kMaxWatched` 96. Rings allocate their memory without writing it, so slots not used cost no memory. The Dispatcher merges with a heap over the slots holding Samples and finds each slot's Consumers once a slice.
6. **Probe on an unconnected pin** — `Machine::probeNet` falls back to the pin's own Net.
7. **Board** — `BoardClick::shift`; the refusal reason next to the cursor.
8. **Recipes** — ADD_INSTRUMENT (budget, groups, the canvas, short tests at 100 MHz: a Capture is 655 µs, 6.5 ms at 10 MHz), ADD_BOARD_DEVICE and ADD_MACHINE (budget 96, the probe note removed).

Nothing else changes: the kernel, Parts, Machines and Puzzles are untouched.

## Tests

- **Engine:** existing cases on the new depth (pre-Trigger counts from `PreTriggerPoints`); Channel 31 recorded and triggered; a Trigger on 32 conditions (levels on some, an edge on one); the mask Trigger matches the per-Channel rule on a random sequence (the old evaluator kept in the test as the reference).
- **Bus pattern:** `8000` on 16 members; `E0xx`; fewer digits padded with leading zeros; `x` and `X`; too many digits, a non-hex digit, a set bit above the width refused; `FormatBusPattern` round trip, `?` for a nibble mixing `x` and levels or holding an edge.
- **Name stems:** `A0`/`A15`, `1Q`/`8Q`, `1A1`/`1A4` vs `2A1`, `I/O1`, `Y7_N`, `RW` (none). Power and no-connect pins (`GND2`, `NC2`) are left out by the Session, tested there.
- **Session:** shift-click on `U1` `A3` of the 100 clips 16 Channels in bit order as `U1 A`, an `A5` clipped before keeping its Channel; `PHI0` makes `U1 PHI` of 3; not enough free Channels refused with the count; click unclips one member, shift-click the group; a group of one dissolves; hand grouping and ungrouping; `setGroupTrigger` sets members' conditions; groups survive a repair.
- **Consumer:** 32 Channels on distinct Nets and two Channels on one Net (mask over 32 bits).
- **Headless Machine:** on the 110, a Capture at 10 MHz triggered on `U1 A` = `8000` and `RW` = 0 (the first write to the latch) shows `U1 D` = `01` (the light's first value) at the last point of that cycle with `PHI2` high (after `PHI0` falls the CPU lets go of the data 90 ns before it changes the address: the cycle's very last point reads a floating bus); a Capture triggered on `U1 A` = `FFFC` with `RW` = 1 shows `U1 D` = `00` at the same point (the reset vector's low byte, `$F800`).
- **Scope:** 4 Channels, Trigger on Channel 4; a delay of 5 divisions puts the Sweep start at `T + delay - 1 div` with the right samples; Auto with a delay.
- **Watch budget:** a Runner with 96 watched Nets of the tennis netlist, each fed; the Dispatcher merges Samples with a heap over the slots that hold some, so 96 slots cost no more per Sample than 48 did.
- **Probe:** `probeNet` on the 100's `U1` `RW` pad equals `PinNet(U1, RW)`; a Session probe on it lights `HIGH` or `LOW` once powered.
- **Session with every instrument:** the 110 with 32 Clips (`U1 A`, `U1 D`, `RW`, `PHI0`, `PHI1`, `PHI2`, `SYNC`, `RES_N`, `IRQ_N`, `NMI_N`), 4 Hooks, the probe and the meter: at most 16 for the LEDs + 32 + 4 + 2 = 54 ≤ 96.
- **Release:** `bw_ui` on the 110 with every instrument, 32 Clips at 10 MHz with the `8000` Trigger, 4 Hooks with a delay: 1.00×, UI under ~1 ms; screenshots of the analyzer (groups in hex) and of the scope (4 bands, delayed) viewed.

## Later

- Decoding (6502 mnemonics under the data bus), state mode (sampling on a clock edge), sequenced Triggers (address A then B), search in a Capture.
- Saving or exporting a Capture.
- A Trigger from the analyzer to the scope.
