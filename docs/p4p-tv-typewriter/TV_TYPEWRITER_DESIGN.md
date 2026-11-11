# P4p TV Typewriter — Detailed Design

Implements [TV_TYPEWRITER.md](TV_TYPEWRITER.md): the 090 Machine of [TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md), built as in [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md). No code in `src/`.

Recipe followed: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md), every step but 4 (Puzzles, P4r). This design gives only what the recipe leaves open. Tests follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

## 1. Files

```
netlists/typewriter.bwn                         grown in three stages (§2)
manuals/typewriter/user.md, service.md          TYPEWRITER_DESIGN.md § Manuals
boards/typewriter/typewriter.place, .kicad_pro, .kicad_pcb
tests/machines/typewriter_test.cpp              §3
tests/CMakeLists.txt                            the test file
tests/machines/manuals_test.cpp                 "typewriter" in the Machine list
tests/board/boards_test.cpp, tests/machine/electrical_test.cpp,
    tests/view/board_scene_test.cpp             "typewriter" in the per-Machine loops (ADD_MACHINE step 3)
tests/session/session_test.cpp                  §4
docs/recipes/ADD_MACHINE.md                     §6
```

## 2. Netlist stages

ADD_MACHINE step 2, "A big Machine": each stage a commit with its own test cases, earlier cases kept green, unused inputs of each stage's chips tied. The final netlist is TYPEWRITER_DESIGN.md § Netlist, word for word.

1. **Video from the RAMs.** `X1`, `U1`–`U21`, `J1`, `J2`. The multiplexer's select is `C5` already; its `B` inputs (the cursor) are tied to `GND`; `WE_N` (`U13` gate 4) has both inputs on `VCC`, so the RAMs only read; `U21` `3D` is tied to `GND`; `U32` is not there yet, so `VIDEO_D` goes straight to `U21.2D`; every input of a gate not used yet tied (`U5` gates 4–5, `U10` gate 2, `U11` gates 3–4, `U12` gates 2–4, `U13` gate 4 on `VCC`). The header comment says what is still missing. Release speed recorded here (most of the cost is in this stage): `bw run netlists/typewriter.bwn --time 2s`, the random page; the docs review measured the final netlist at 1.0× (spec).
2. **Keyboard, write logic, cursor counter, Clear.** `U22`–`U30`, `SW1`, `R1`, `J3`; `WE_N`, the multiplexer's `B` inputs, `U21.3D` and `U12` gate 2 wired as in the final netlist.
3. **Cursor on the screen.** `U31`–`U36`; `U32` gate 4 between `VIDEO_D` and `U21.2D`, `U12` gate 3, `U11` gates 3–4, `U34.1D` on `ACTIVE`, `DOT_END` to `U35`, `FRAME_END_N` to `U36`.

## 3. Tests: `tests/machines/typewriter_test.cpp`

Copy the picture helpers of `message_test.cpp` (`Lit`, `Matches`, `FindPage`, `LitOutside`, `LockedFrames`, `DesignCircuit`, `BoardCircuit`) with the 090 sizes: `kDot = 2` (0.25 µs / 125 ns), `kColumns = 32`, `kRows = 16`, `kCellDots = 6`, `kCellLines = 16`; `FindPage` searches `left` 0–127 and `top` 0–63 and accepts one run of at most `kDot` matches. The search's left edge holds: the text starts at about column 34 (TYPEWRITER_DESIGN.md, Where the page lands; the review saw column 34, first lit row 6). `LitOutside`'s rectangle is one dot wider on the right than message_test's, for a cursor in column 31, whose inverted sixth dot is lit. `LockedFrames(sim, tv, count)` continues a running `Simulator` (after Clear and typing) instead of powering one on.

Helpers:

- `Font()`: `ReadRomContents(roms/2513.rom, 512, 5)`.
- `ExpectedPage(codes, cursor)`: the page as dots [line][dot] from 512 6-bit codes, as message_test's `ExpectedPage` but from the codes instead of the message ROM; with `cursor` set, that cell's 6 dots × 16 lines inverted.
- `struct Typewriter` (as the bench's `Bench` in `tests/netlist/ram_bench_test.cpp`): the `LoadedCircuit` (design netlist until the Board, then `BoardCircuit`) and its `Simulator`; `powerOn(seed)` runs 1 ms; `clear()` presses `SW1` for 25 ms then releases it and runs 1 ms (`ButtonOf`, `wake`); `type(text)` hands each character to `KeyboardOf(J3)->type` and wakes it, then runs 250 µs a key (at most 79 µs to the write and step, plus the 101 µs gap to the next key), or with `queued = true` hands them all over first and runs 250 µs a key after; `cell(i)` = `U17.word(i) | (U18.word(i) & 3) << 4`; `codes()` the 512 cells; `cursor()` = (`X0`–`X4`, `Y0`–`Y3`) read from the Nets, `std::nullopt` if one is not 0 or 1.
- Expected cell for a typed character: its code `& 0x3F`.

Cases (stage that brings them in brackets; `[machines]`, `[slow]` where the TV is read):

- [1] `typewriter: netlist loads without warnings`.
- [1] `typewriter: the TV shows the RAMs` (`[slow]`), seeds 1 and 2: the second and third locked frames are `ExpectedPage(codes())` (codes read after the frames, the RAMs only read), found at `left` 30–38, `top` ≤ 8, nothing lit outside; the third frame equals the second. In stage 3 this case gives `ExpectedPage` the cursor's cell and accepts either the inverted or the plain page on each frame.
- [2] `typewriter: Clear blanks the page and homes the cursor`, seeds 1–3: every cell 0x20, cursor (0, 0).
- [2] `typewriter: keys land at the cursor`: after Clear, `"HELLO, WORLD"`: cells 0–11 its codes, cell 12 a space, cursor (12, 0).
- [2] `typewriter: Return starts a line, other control keys do nothing`: after Clear, `"AB\rC"`, then Backspace (8), Esc (27) and CTRL-A (1) as raw codes through `Keyboard::type` (`KeyboardCode` maps only `\r`, `\n`, `\b`): row 0 `AB`, row 1 `C`, every other cell a space, cursor (1, 1).
- [2] `typewriter: lines and the page wrap`: after Clear, 33 letters: cells 0–32 (row 1 starts with the 33rd), cursor (1, 1); then Clear, 16 Returns: cursor (0, 0); `Z`: cell 0 is `Z`.
- [2] `typewriter: keys typed at once all land`: after Clear, 32 keys queued in one go (`queued = true`): cells 0–31 in order, cursor (0, 1).
- [3] `typewriter: the typed page and a blinking cursor` (`[slow]`), seed 1: Clear, `"HELLO\rWORLD"`, then 20 locked frames: each is `ExpectedPage(codes(), cursor (5, 1))` inverted or plain at the cursor, both seen, the rest of each frame identical to the first's.

Debug runs this Machine at about 0.016×: each fast case (about 30 ms simulated, Clear most of it) takes about 2 s; the `[slow]` cases about 0.5 s simulated, 30 s each.

## 4. Session test

`session: typewriter with every instrument` (`[session][slow]`), as `session: message with every instrument`: 8 clips (`U19` `O1`–`O5`, `U17.IO1`, `U20.SH_LD_N`, `U20.QH`), hooks on `U1.RCO` and `U21.3Q`, the probe on `U20.QH`, the meter on `U19.VCC`; `session.keyboards() == {"J3"}`; `powerOn(1, 500_ms)`, then `CHECK(session.type("J3", U'A'))` before `waitIdle` (the key is accepted; whether it is written is the machine test's job); the TV locked and more than 15 frames. This checks the watch budget. About 1 min in debug.

## 5. Board

TYPEWRITER_DESIGN.md § Board, ADD_MACHINE step 3. Before the Board the tests use `DesignCircuit()`; once it is routed, every case but `loads without warnings` builds from the Board (`BoardCircuit()`), seeds unchanged (the fast cases do not depend on them).

## 6. Recipe

ADD_MACHINE.md:

- Reference commits: TV typewriter (090, RAM written from a keyboard), its stage commits and Board.
- Gotchas: "A dot clock at 4 MHz: clock the line counter and anything else that steps once a line by `LINE_END`, not by the dot clock with an enable; each chip on the dot clock costs two evaluations a dot" and "A write strobe decoded from counter bits glitches at character boundaries: re-time it on the clock (090 `WEK_N`), and keep the address steady through it."
- Step 5: a Machine with a keyboard runs in the release check with `--type REF:START:TEXT` (one key every 100 ms).

## 7. Release check

ADD_MACHINE step 5:

```
build/release/src/cli/bw run netlists/typewriter.bwn --time 2s
build/release/src/ui/bw_ui netlists/typewriter.bwn --press SW1:300ms:400ms \
    --type 'J3:500ms:HELLO, WORLD\nTV TYPEWRITER' --after 4500ms --screenshot /tmp/typewriter.png \
    --probe U20.QH --clip … --hook … --meter …
```

(`--after` is wall-clock time and the last key is typed at 3.0 s simulated, so 4.5 s leaves room below 1×.) View the PNG: the two typed lines at the top left, the cursor block after `TV TYPEWRITER` (on or off, depending on the frame), the keyboard window. Ratio and UI time recorded, with and without the instruments (spec: below 1× accepted).
