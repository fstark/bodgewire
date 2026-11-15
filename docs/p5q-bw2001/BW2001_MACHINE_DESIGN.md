# P5q BW-2001 — Detailed Design

Implements [BW2001_MACHINE.md](BW2001_MACHINE.md): the port drive strength, the `74LS139` and `MC3446`, the passive PET connectors, the `MONITOR` connector and the TV's separate drives, the `MATRIXKB` keyboard and its window, `grid pet` in `bw place`, the editor's retrace wait, and Machine 130, whose netlist, Board, manuals and tests are in [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md).

Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (catalogue row, datasheet, delays, `PartPinNames`), [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (the chain from Part to window), [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md) (switches), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (§2 netlist in stages, §3 Board, §5 Session test and release check). Code follows [STYLE.md](../STYLE.md), [NAMING.md](../NAMING.md).

Sources: **SCH** Commodore 320135 sheets 1–3 for board 320137 ([schematic/](../machines/130-bw2001/schematic/README.md)); **MC** Motorola MC3446A data sheet (1979 Interface Integrated Circuits, pp. 5-54, 5-55, bitsavers); **TI139** TI SN74LS139A data sheet; **B1** the BASIC 1 ROMs (`external/roms/pet`).

---

## 1. Port drive strength — `src/sim/logic.{hpp,cpp}`

`enum class DriveStrength : std::uint8_t { None, Weak, Port, Strong, Supply };` with a comment: `Port`, an NMOS port's low output, beats a pull-up and loses to any output's high.

`DriveOf(PinKind::PullUp, …)`: 0 → `Port` (was `Strong`), X → `Port` (was `Strong`: an output's 1 then beats a port's X too), 1 → `Weak`, Z → `None`. The comment on `PinKind::PullUp` says so. The resolver needs nothing else: strengths are compared by order, and `ttlContention` stays for `Strong` only.

Why: after `POKE 59458,62` the VIA's `PB5` drives 0 onto `VIDEO_ON`, which `UC6`, an LS gate, drives high for 200 of 260 lines. An LS output's high sources tens of mA; the 6522's port sinks about 2 mA at 0.4 V. The gate wins on the real board, PIA1's `CB1` still sees the retrace, and the keyboard goes on. Today both are `Strong` and the Net goes X.

Who else sees it: every `PullUp` Pin (6520 `PA`, `CA2`; 6522 ports and `CB1`, `CB2`; 6530 ports; the new MC3446 bus pins). Against a resistor pull-up (`Weak`) and a switch network group (which resolves its Nets' drives and drives the result at `Supply`) nothing changes. Against a TTL or NMOS output driving 1 the 1 now wins instead of X: the BW-1's ports face no output; P5n's bench drives VIA `PB5` from `X2` (the plan's note: the bench's power-on contention reads 1, not X, until `RES_N`). A stuck Fault on a `PullUp` Pin ties its Net at `Strong` (builder.cpp:84; its own read Net at `Supply`) and is unchanged.

`datasheets/6520.md`, `6522.md`, `6530.md`: "a port output's low is weaker than a TTL output's high: an output driving the same line high wins".

Tests (`tests/sim/logic_test.cpp`): `resolve: a port's 0 loses to an output's 1` (TTL and MOS), `… beats a pull-up`, `… beats a port's 1`, two ports' 0 and 1 → 0; the existing case `logic: DriveOf pull-up drives 0 strongly…` (lines 82–86) rewritten for `Port` (0 and X); the other contention cases unchanged.

## 2. `74LS139` — `src/parts/` (ADD_PART)

Dual 2-to-4 decoder, DIP-16. Pins (TI139): 1 `1G_N`, 2 `1A`, 3 `1B`, 4–7 `1Y0_N`–`1Y3_N`, 8 `GND`, 9–12 `2Y3_N`–`2Y0_N`, 13 `2B`, 14 `2A`, 15 `2G_N`, 16 `VCC`. A half: `Yn_N` low when `G_N` is low and `B A` = n; X on a selected input gives X on the outputs it could select (as `Ls138`). Delays (`delays.hpp`, TI typicals, rise = output to 1): `kLs139Select{13'000, 22'000}` (A, B → Y), `kLs139Enable{16'000, 21'000}` (G → Y, 2 levels). Class `Ls139` in `src/parts/ls139.{hpp,cpp}`, modelled on the hand-written `Ls138` (`src/parts/ls138.cpp`), both halves in one evaluation. The delay figures are checked against TI139 at implementation (its select figures are for two gate levels) and the comment says which. `datasheets/74LS139.md`.

Tests (`tests/parts/ls139_test.cpp`): truth table both halves; enable high → all high; delays; X.

## 3. `MC3446` — `src/parts/mc3446.{hpp,cpp}`

Quad IEEE-488 bus transceiver, DIP-16, logic only. Pins (MC p. 5-54): 1 `RA` (receiver output A), 2 `BUSA`, 3 `DA` (driver input A), 4 `ENABC_N`, 5 `DB`, 6 `BUSB`, 7 `RB`, 8 `GND`, 9 `RC`, 10 `BUSC`, 11 `DC`, 12 `END_N`, 13 `DD`, 14 `BUSD`, 15 `RD`, 16 `VCC`.

- A bus pin is `PullUp` (the chip's own termination is its weak 1): driven 0 while its driver input is 0 and its enable is 0, else released (MC: V_OL with V_IL(D), V_OH with V_IH(D): non-inverting, open collector, enable active low). X on the driver or enable: X.
- A receiver output (`Output`, TTL) follows its bus pin's Net (MC: non-inverting).
- Delays (MC p. 5-55, maximums, no typicals published): driver to bus low 50 ns, high 40 ns; enable 50 ns both; bus to receiver high 50 ns, low 40 ns. `kMc3446Driver{40'000, 50'000}`, `kMc3446Enable{50'000, 50'000}`, `kMc3446Receiver{50'000, 40'000}`.
- The real chip's termination pulls to about 3.3 V and its driver sinks 48 mA: as a `PullUp` its 0 is `Port` strength. On the BW-2001 nothing else drives the bus, so the difference does not show; the datasheet file says so.

`datasheets/MC3446.md`. Tests (`tests/parts/mc3446_test.cpp`): each channel drives and releases; the enables; a receiver follows a bus pulled low from outside (a second Part's output on the same Net); delays.

## 4. Passive connectors — `src/parts/connector.cpp`, `catalogue.cpp`, `kicad_netlist.cpp`

Three catalogue rows, `PartKind::Connector`, `&CreateConnector<"…">`, no behaviour (`Connector`), every pin an `Input` with `Family::Mos` (no "floats high" warning, never pulls a Net) and `sampled` (an 8 MHz `VIDEO` on `J2` must not wake an empty `evaluate` every dot: circuit.cpp:83), the pin names the PET's labels:

| Part | Package | Footprint | Pins (pad order) |
|---|---|---|---|
| `IEEE488` "IEEE-488 edge connector" | `HDR-2x12` | `Connector_PinHeader_2.54mm:PinHeader_2x12_P2.54mm_Vertical` | `DIO1`–`DIO4`, `EOI_N`, `DAV_N`, `NRFD_N`, `NDAC_N`, `IFC_N`, `SRQ_N`, `ATN_N`, `CHASSIS`, `DIO5`–`DIO8`, `REN`, `GND1`–`GND7` |
| `USERPORT` "user port edge connector" | `HDR-2x12` | as above | `GND1`, `VIDEO`, `SRQ_N`, `EOI_N`, `DIAG`, `READ1`, `WRITE`, `READ2`, `HORZ`, `VERT`, `GND2`, `GND3`, `GND4`, `CA1`, `PA0`–`PA7`, `CB2`, `GND5` |
| `CASSETTE` "cassette connector" | `HDR-1x06` | `Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical` | `GND`, `VCC`, `MOTOR`, `READ`, `WRITE`, `SENSE` |

The PET's edge connector has its number row 1–12 on one side and its letter row A–N (A B C D E F H J K L M N: no G, I) on the other, A opposite 1. On the 2 × 12 header (KiCad numbering: pads 1 and 2 face each other) PET pin n is pad 2n − 1 and the k-th letter pad 2k, so each letter still faces its number. Pin names are by the PET's label, so netlists and manuals never use pad numbers; the service manual gives both. `GND`/`VCC` names on these are plain `Input`s: they are on the rails' Nets but neither supply nor are powered (`ConnectorPowersDevice` false; `ConnectorVolts` nullopt). The footprints are copied unchanged from KiCad 10's library into `boards/footprints/` (its README's rule). Datasheets `datasheets/IEEE488.md`, `USERPORT.md`, `CASSETTE.md` (every catalogue Part needs one: `manuals: every catalogue Part has a Datasheet`): the pins with the PET's labels and pad numbers, what the PET puts on each.

Tests: `tests/parts/connector_test.cpp` pin names and kinds; `catalogue_test` rows; `footprint: every catalogue package loads`.

## 5. `MONITOR` and the TV's separate drives

### 5.1 Part

Catalogue row `MONITOR` "monitor connector, separate drives", package `HDR-1x07` (`PinHeader_1x07_P2.54mm_Vertical`), `Connector`. Pins: `VIDEO_N`, `GND1`, `VDRIVE`, `GND2`, `HDRIVE`, `NC6`, `GND3` (SCH: J7 1 video, 3 vertical drive, 5 horizontal drive, 2, 4, 7 ground, 6 unused). Signal pins `Input` TTL like `VIDEO`'s; the grounds plain `Input`s (`Family::Mos`) listed on the `GND` Net in the netlist, as the passive connectors' (a `Power` pin named `GND1` would need `PowerPinRail`/`PowerPinVolts` entries: builder.cpp:266, 289); `NC6` `PinKind::NoConnect` (ADD_PART). Unpowered like `VIDEO`. Signal pins not `sampled`: the TV reads its Nets through the consumer, not the Part. `datasheets/MONITOR.md`: the dot lit while `VIDEO_N` is low; a line starts at `HDRIVE`'s rise, a frame at `VDRIVE`'s fall.

### 5.2 `Tv` — `src/devices/tv.{hpp,cpp}`

`enum class TvSync : std::uint8_t { Composite, Separate };` (`Composite` today's; not `TvInput`, which is the headless plug class in `tv_input.hpp`). `explicit Tv(TvSync sync = TvSync::Composite)`. New:

```cpp
void setHorizontalDrive(Time time, LogicValue value);  // Separate: a line at each accepted rise
void setVerticalDrive(Time time, LogicValue value);    // Separate: a frame at each accepted fall
```

Each `advanceTo(time)` first (as `setSyncN`); a rise of `HDRIVE` at least `kLineAccept` after the line start calls `startLine(time, true)`; a fall of `VDRIVE` at least `kFrameAccept` after the frame start calls `startFrame(time, true)`. The free-running periods, `paint` and the lock rule are shared. In `Separate`, `setSyncN` is not called (Check). `setVideo` keeps its meaning (high = white); the inversion is the consumer's (§5.3).

On the BW-2001 a frame starts at `VERT_DRIVE`'s fall: 40 lines (vertical drive, top blank) before the text, which fills TV rows 40–239 of 320; a line starts at `HORZ_DRIVE`'s rise (character 24), the text from character 43 (the dots one character behind `DIS_ON`), TV columns 152–471 of 512.

Tests (`tests/devices/tv_test.cpp`): separate drives at 64 µs and 260 lines lock in 2 frames; `VDRIVE` stopped: frames free-run every 21 ms, unlocked; `HDRIVE` stopped: lines every 66 µs; video painted where set. The composite cases unchanged.

### 5.3 Consumers, Session, CLI

- `src/runner/tv_consumer.{hpp,cpp}`: a second constructor `TvConsumer(NetId videoN, NetId hdrive, NetId vdrive)` (`nets_` becomes a small vector; `Tv(TvSync::Separate)`); `sample` routes by Net; the `VIDEO_N` sample is inverted (`Not`, Z and X kept) before `setVideo`. `powerOn` rebuilds the Tv in the consumer's own mode (today `tv_ = Tv{}`, tv_consumer.cpp:5, would turn it back into a composite one).
- `src/devices/tv_input.{hpp,cpp}` (headless, `bw run --tv`): the same second constructor.
- `src/session/session.cpp` (line 67) and `src/cli/main.cpp` (line 552): `VIDEO` first, else `MONITOR` (`ConnectorNet(…, "MONITOR", "VIDEO_N")` and the drives); `bw run --tv` with neither: the error becomes "netlist has no VIDEO or MONITOR connector" (cli/main.cpp:555).

Tests: `tests/runner/tv_consumer_test.cpp` (new, added to `tests/CMakeLists.txt`) a MONITOR consumer paints an inverted dot and stays separate across power-off and on; a `[session]` case comes with the Machine (§9).

## 6. `MATRIXKB` — the BW-2001 keyboard

### 6.1 Switch contacts — `src/parts/switch.{hpp,cpp}`

The network's members become contacts, so a device can own many:

```cpp
// One pair of legs the SwitchNetwork joins while closed (a Switch's, or one key of a MATRIXKB).
class SwitchContact {
public:
    bool closed() const { return closed_; }
    bool attached() const { return network_ != nullptr; }
    // Simulation thread, from its owner's evaluate: wakes the network when it changes.
    void set(EvalContext& ctx, bool closed);
private:
    friend class SwitchNetwork;
    bool closed_ = false;
    SwitchNetwork* network_ = nullptr;
};
```

`Switch` holds one `SwitchContact contact_` (its `closed()`, `attached()` forward; `evaluate` calls `contact_.set(ctx, closed)`; `powerOn` sets it stuck-closed as now). `SwitchNetwork::add(SwitchContact&, PinIndex, PinIndex)`; `Member{SwitchContact*, a, b}`; `rebuild` reads `m.contact->closed()`. `set`: unchanged value → nothing; else `closed_`, `network_->rebuild_ = true`, `ctx.wake(network_->id(), kSwitchDelay)`. Behaviour and random draws unchanged (the P5i tests and every switch Machine pass as they are).

### 6.2 Part — `src/parts/matrix_keyboard.{hpp,cpp}`

`class MatrixKeyboard final : public Component`, the BW-2001 keyboard, 20 pins (pin index = pad − 1): `ROW0`–`ROW9`, `COL0`–`COL7`, `NC19`, `NC20`; the row and column pins `Input` `Family::Mos`, `sampled` (the keyboard has no electronics: the network drives the Nets, and a row's change must not wake the Part and disturb its `nextAt_` timing), the last two `NoConnect`.

- **Keys**: `kBw2001Keys`, 73 `{row, column, legend, shifted}` in P5p's matrix order (BW2001_FIRMWARE.md, "The PET it runs on": row 0 `PB0` `!` … row 9 `PB7` `=`; P5p's netlist `SW1`–`SW73` order), `std::span<const MatrixKey> Bw2001Keys()`. Each key owns a `SwitchContact`.
- **Held from outside** (UI, any thread): `void setHeld(std::size_t key, bool held)` (an `std::atomic<bool>` per key), then a wake (`Runner::wake(id())`).
- **Typed in simulation time**: `bool type(char32_t c)`, SPSC queue of 4096 key presses (paste); `dropWaiting()` as `Keyboard`'s. A character becomes a press of its key, with SHIFT (left) held for the same time when `MatrixKeyFor` says so: `std::optional<MatrixPress> MatrixKeyFor(char32_t c)` (free function, tested; not the harness's `Bw2001KeyFor(char)`, which stays as P5p's typist and, from phase 4, reads the Part's table): `0x20`–`0x5F` by the key whose legend it is (unshifted where the PET has one: `"` is row 1's own key); `a`–`z` as `A`–`Z`; `\n`/`\r` RETURN; the PET's named keys by private codes (`kKeyHome` …, used by the PC keys, §6.4): HOME, CLR (SHIFT+HOME), CRSR ↓/↑ (↑ shifted), CRSR →/← (← shifted), DEL, INST (shifted), STOP, RVS. Anything else: nullopt (skipped).
- **evaluate**: a press is held `kMatrixHold` = 40 ms, then released `kMatrixGap` = 40 ms (more than two retraces each: our editor and BASIC 1 read keys once a frame); its own wakes told from the Session's by time (`nextAt_`, as `Keyboard`). Each evaluation sets every contact to `held || scripted` (`SwitchContact::set`).
- No Faults: `fault switch J5…` is refused (it is not a switch reference), and no Fault names a key (floating devices faultless).
- `powerOn`: contacts open, the queue kept (as `Keyboard`); `ConnectorPowersDevice("MATRIXKB")` false (a switch matrix needs no power).

Catalogue row `MATRIXKB` "BW-2001 keyboard (10 x 8 key matrix)", package `HDR-1x20` (`PinHeader_1x20_P2.54mm_Vertical`), `PartKind::Connector`. `ConnectorPins("MATRIXKB")`. `datasheets/MATRIXKB.md`: the pins, the matrix table, "a held key joins its row to its column".

### 6.3 Builder — `src/netlist/builder.cpp` `wireSwitches`

Besides `PartKind::Switch` Parts, every `MATRIXKB` Part adds its 73 contacts: legs = the Nets of its `ROWr` and `COLc` Pins (`pinNet_`), skipped when either is unconnected or both are one Net. The Nets join the network's Pin list as a switch's do. `MatrixKeyboardOf(loaded, ref)` finder beside `PaddleOf`.

Tests (`tests/parts/matrix_keyboard_test.cpp`, on a small netlist with a 74LS145 and pull-ups as the PET's): pins; a held key pulls its column low while its row is selected and not otherwise; three keys in a rectangle ghost the fourth; `type("A")` holds `A` 40 ms then releases; `"` unshifted, `CLR` with SHIFT, lower case as upper; a queue of 4096; `dropWaiting`.

### 6.4 Session, CLI

- `Session`: `matrixKeyboards()`; `setMatrixKeyHeld(reference, key, held)` (any time; released at power-off as switch presses are, session.hpp:171; ignored while unpowered); `type(reference, char32_t)` (existing) routes to a `MATRIXKB` when the reference is one (`typeCode` stays KEYBOARD-only); dropped at power-off as KEYBOARD keys are.
- `bw run --type J5:500ms:TEXT` and `bw_ui --type` work on a `MATRIXKB` through `Session::type` / the Part's `type` (headless `bw run` calls the Part directly, as for `Keyboard`). In `bw_ui`, `--type`'s reference check (`src/ui/main.cpp:910`, against `session.keyboards()`) also accepts `session.matrixKeyboards()`, and the scripted send (`main.cpp:688`, `typeCode`) uses `Session::type` for a `MATRIXKB`.

### 6.5 Window — `src/view/matrix_keyboard_layout.{hpp,cpp}`, `src/ui/matrix_keyboard_window.{hpp,cpp}`

- Layout (pure, tested headless): the PET 2001's keyboard as photographed: the main block (five rows: `! " # $ % ' & \ ( ) ←`; RVS `Q`–`P` `↑`; `A`–`L` `:` RETURN; SHIFT `Z`–`M` `,` `;` `?` SHIFT; `@` `[` `]` SPACE `<` `>` STOP) and the numeric pad (CLR/HOME, CRSR ↓, CRSR →, INST/DEL; `7 8 9 /`; `4 5 6 *`; `1 2 3 +`; `0 . - =`), every one of the 73 keys once, positions in key widths. Test: every key of `Bw2001Keys()` has exactly one cap.
- Window: a floating window titled by the reference, drawn with ImGui draw lists as the PET 2001's chiclet keys (small square caps in a grid, the legends on the cap and the shifted legend above; the main block and the pad as separate groups on a dark bezel), tuned on release screenshots (memory: drawn hardware, floating window). A click holds a key (`setMatrixKeyHeld`), right-click latches (SHIFT for a sequence). Held keys drawn down.
- Capture (`src/ui/keyboard_capture.{hpp,cpp}` generalised to a target kind): while the window is focused, a PC key held holds the BW-2001 key(s) for its character (`MatrixKeyFor` of the shifted character, as the ASCII window takes the character typed): letters, digits, punctuation; Return RETURN; Backspace DEL; arrows CRSR (left and up with SHIFT); Home HOME; Escape STOP. Released with the PC key. Cmd+V (macOS) or Ctrl+V: `SDL_GetClipboardText`, each character to `Session::type` (queued, 80 ms a character in simulation time). F2 toggles as today; the banner as today.
- `main.cpp`: the window opens for a Machine with a `MATRIXKB`, as the keyboard window for a `KEYBOARD`.

## 7. `grid pet` — `src/board/place_rules.{hpp,cpp}`, `placer.{hpp,cpp}`, `kicad_pcb_writer.cpp`

- Rule: `grid pet` (beside `grid COLUMNS ROWS`): `PlaceRules::petGrid = true`. With it:
  - every Part whose reference is `U` + a row letter `A`–`J` + a column `1`–`9` (and `UDE9`: between rows D and E) is placed at its cell, turned 90° (a new field `angle` in `PlacedPart`, placer.hpp:17; pin 1 at the bottom left, every chip the same way);
  - column widths: the longest courtyard (along the chip) in that column plus `kPartGap`; row heights: the widest courtyard across plus a resistor's courtyard and two gaps (so a `next … back` resistor fits between rows); row A at the back;
  - `next R UXn SIDE` may name a grid chip: the resistor goes beside it (after the grid is placed), as beside a panel part; a row of resistors chains (`next R29 R28 right`, as `boards/bw1/bw1.place`), since `next` centres a part on its `other`; parts placed by `next` keep 0° (resistors lie along the rows), annealed parts in PET cells are turned 90° as the chips;
  - other grid packages without a rule (resistors without `next`, `X1`) are annealed into the cells the PET leaves empty, panel and PET chips fixed; the LED `DS1` (not a grid package, placer.cpp:54) needs its own rule;
  - a PET reference outside A–J/1–9, or two Parts in one cell: an error naming them.
- Writer: a footprint at an angle is written `(at x y 90)` and its pads `(at px py 90)` (KiCad keeps the footprint's angle in each pad's), the courtyard, silkscreen and `(point)` rotated about the origin; the reader (`kicad_board.cpp`) already reads angles (user-turned digits).
- Tests (`tests/board/placer_test.cpp`): `UC9` in row C column 9 at 90°; a DIP-40 column wider; `next R12 UC8 back` between rows B and C; an unplaced resistor in an empty cell; two Parts in one cell refused; the written Board read back with the same pad positions (`check-board` on a small PET-grid netlist).

## 8. The editor's retrace wait — `roms/bw2001_edit.asm`, `roms/bw2001.inc`, `tests/machines/bw2001_model.{hpp,cpp}`

- `bw2001.inc`: `VIA = $E840` (`ORB` at `+0`).
- `bw2001_edit.asm`: every access to the screen, read or write (`SEL8` comes from the 74LS154 on `A12`–`A15` with no R/W or φ2 gating, so a read switches the multiplexers as a write does), is preceded by a wait until `PB5` reads low, as BASIC 1's `$E7AD` (`LDA $E840`, `AND #$20`, `BNE`). Sites: print at the cursor, the cursor's read-invert-write, clear, the scroll copy (`lda SCREEN+$028,x` / `sta SCREEN,x`, lines 161–174), RETURN reading the row into the line buffer, the DEL/INST shifts; the monitor prints through the editor. The wait sits in the innermost loop, inline right before the access (in a copy, before the read; the write follows within a few cycles). `EDIRQ` runs at the start of the blank and keeps its wait anyway. The self-test (`bw2001_kernel.asm`) does not wait: it must run with a dead VIA; it may snow.
- **Timing budget**: `PB5` is `VIDEO_ON`; on its last blank line it rises at about character 19 of the line and `DIS_ON` at character 42 (SHEET3.md, timing), so an access within about 22 cycles of the `LDA $E840` that read 0 is safe. The inline wait's exit (`BNE` not taken) and a read and a write of the copy take about 12 cycles: inside. A `JSR` to a wait subroutine (`PHA`, loop, `PLA`, `RTS`) and a store is about 20: inside, barely; inline is preferred.
- **Character set**: our reset (`bw2001_kernel.asm`) writes VIA `PCR` = `$0C` (`CA2` low: `GRAPHIC` low, the 901447-08's first half, upper case and graphics), as BASIC 1 does (`$E22D`). Without it `CA2` floats to its pull-up's 1 and the TV shows the second half, where `$41`–`$5A` are lower case. The model accepts that write.
- Cost: about 35 cycles a byte with the wait, 3 840 µs of blank a frame: a scroll (960 bytes copied, 40 blanked) about 9 frames (0.15 s), a clear about 9 frames. A 30-line listing about 4–5 s.
- The model: `kFrameCycles = 16'640` (260 lines of 64 µs); `PB5` (VIA `$E840` bit 5, and its mirrors in `$E800`–`$EFFF` that select the VIA) reads 0 for the first 3 840 cycles of each frame (the blank, which starts at the retrace edge on PIA1 `CB1`) and 1 otherwise; the VIA's other bits read 1. Reads of `ORB` (`$E840` and mirrors) and the write of `PCR` no longer set `touched_` (`touchedUnmodelled()` is asserted in the power-on, conformance and self-test cases: bw2001_firmware_test.cpp:128, 297, 467; bw2001_model_test.cpp:89); any other VIA access still does. The model counts screen accesses (reads and writes of `$8000`–`$8FFF`) that fall in the display: a frame cycle in the 200 video lines (after the 3 840-cycle blank and the 1 280-cycle top blank: lines 80–259 of the model's frame) at a line cycle in the 40-character window, `snowAccesses()`.
- Tests: every P5o session and P5p model case passes (their typed-key timing is by the harness's `WAITING`/`LINES` signal, so slower printing only lengthens them); `snowAccesses()` is 0 at the end of each (the self-test cases excepted); the P5p minimal netlist's cases (its `X2` 60 Hz square wave on `PB5`: low half the time, so the wait works there too). The `.asm`/`.rom` pairs regenerated and committed.

## 9. Machine 130 BW-2001

[BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md): netlist in three stages, connectors, Board, manuals, tests. `tests/machines/bw2001_screen.{hpp,cpp}` reads the TV picture into screen codes. The P5p harness (`tests/machines/bw2001_harness`) gains a Machine target (`Bw2001MachineTarget`): run; `setKey(Bw2001Key{row, column}, held)` → `MatrixKeyboard::setHeld` of the key at that row and column (a lookup on `Bw2001Keys()`) and a wake, as `Bw2001Netlist::setKey` does with switches (tests/netlist/bw2001_netlist_test.cpp:84–89); a RAM byte and a screen byte through `MemoryCells`. A session then runs on the Machine as on the model.

Speed: the netlist is Commodore's, so nothing is gated that the PET does not gate (the 070/090 gating changed our own circuits). Each stage's ratio is recorded: headless `bw run netlists/bw2001.bwn --time 2s` at power-on and idle at `READY`, `bw_ui` with and without instruments, with the event count of the busiest Nets (`--record-all --vcd`, counted) for the kernel row that follows.

## 10. Docs

- [ADD_PART.md](../recipes/ADD_PART.md): the port strength (`PullUp` 0 is `Port`); a transceiver as `PullUp` bus pins; references from a real board's grid.
- [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md): a matrix device through switch contacts; passive multi-pin connectors; a monitor on separate drives; paste.
- [ADD_MACHINE.md](../recipes/ADD_MACHINE.md): transcribing a schematic (three sheets, joined by Net name, run with a reference ROM before designing); `grid pet`; the BW-2001's references.
- [GLOSSARY.md](../GLOSSARY.md): Snow.
- [GAME_DESIGN.md](../GAME_DESIGN.md) ladder: the BW-2001 row points at 130.
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P5q row done with links; a kernel speed row after the measured ratios (its content from §9's counts).

## 11. Release check

Shared code (kernel strength, switch network, TV, Session, UI, placer): the full debug suite at the end. Release: each stage's ratios (§9); `bw_ui netlists/bw2001.bwn` on the Board with every instrument (32 Clips on `BD0`–`BD7`, `BA0`–`BA9` and the video Nets, 4 Hooks, probe, meter); screenshots viewed: the power-on garbage, the banner, a listing, the killer poke's snow (BASIC 1 in the sockets by a local netlist copy, not committed), the keyboard window with a key down.
