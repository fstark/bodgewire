# P5m BW-1 Puzzles — Detailed Design

Implements [BW1_PUZZLES.md](BW1_PUZZLES.md): the BW-1's Puzzles, its manuals' self-test, the tests. No product code.

Recipes: [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (design rules, the `.bwp` file, Fault syntax, tests, docs). Machine: [BW1.md](../machines/120-bw1/BW1.md), [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md). Programs: `roms/bw1_002.asm` (monitor), `roms/bw1_003.asm` (self-test, its header the failure codes).

---

## 1. Manuals

`manuals/bw1/service.md`, a section **Self-test** after Components:

- Reaching it: a Bodge Wire from `U24` pad 4 (`STRAP`) to `PA0` (any pad on it: `U2` pad 2, `U17`, the keys of the `PA0` column), then RS or power-on; or `1800` GO from a working monitor. Take the wire off to return to the monitor.
- While it runs: digit 1 shows the test number, `1` main RAM, `2` `U2`'s RAM (`$17C0`–`$17FF`), `3` `U3`'s RAM (`$1780`–`$17BF`), `4` `U2`'s ROM, `5` `U3`'s ROM; then every segment of every digit (`888888`), each digit alone, then `PA55` and the code of a held key (`GETKEY` codes: `0`–`F`, AD `10`, DA `11`, `+` `12`, GO `13`, PC `14`).
- A failure stays: digit 1 the test, digits 2–4 the address's low 12 bits, digits 5–6 the bad bits (written XOR read; for a ROM, its sum, which should be `00`). Examples from the `bw1_003.asm` header: `123408` is `$0234` bit 3; `27D501` `$17D5` bit 0; `4C0001` `U2`'s ROM summing to 1. Bit `n` of main RAM is held by one chip (the Components list: `U5` bit 7 … `U12` bit 0).
- SST must be at OFF.

`manuals/bw1/user.md`: one line at the end: if the BW-1 will not start or behaves oddly, the Service Manual's self-test checks its memory and display.

Check the manuals test (`manuals` in ctest) for backtick references (P5l note: pin names like `RS0` not backticked).

## 2. The test rig

The Puzzle cases need the P5l `Bw1` class (keys, `display()`), which is in `tests/machines/bw1_test.cpp`'s anonymous namespace.

- `tests/machines/bw1_rig.{hpp,cpp}` (new): `Bw1`, `KeyReference`, `IsHex`, `CheckShows`, `Started`, `NetlistPath`, moved from `bw1_test.cpp` unchanged but for one point: `Bw1` finds `DIG1`–`DIG6` and `SEG_A`–`SEG_G` by pin (`PinNet(*loaded, "DS<n>", "CC1")`, `PinNet(*loaded, "DS1", "A"…"G")`), not by name: on a damaged Board Nets are named from the Copper (ADD_PUZZLE Tests). A cut `SEG_x` leaves the far digits on another Net: `display()` reads `DS1`'s segment Nets, so Half a segment reads each digit's own (`segments_` per digit, from `DS<n>`'s pins).
- `bw1_test.cpp` includes it; `tests/CMakeLists.txt` adds `machines/bw1_rig.cpp` and `machines/bw1_puzzles_test.cpp`.
- A Puzzle's Circuit: `Machine machine = LoadPuzzle("<name>")` (a local helper in the test file, as `led_computer_test.cpp`); `machine.build()`; strapped: `machine.addWire({"U24", "4"}, {"U2", "2"})` before `build()` (as `BoardCircuit(true)`). `Machine` cannot be copied (it holds a `unique_ptr`): a helper `Bw1 Run(Machine&, std::uint64_t seed, bool strap = false)` adds the wire if asked, builds, removes it again (`removeWire`) and powers on.
- `display()` drops polls where two digits are selected, and decodes through `Bw1DigitChar`. Bright mess needs a reader of each digit's segments while its own cathode is low, whoever else is selected (`segmentsWhileSelected(d)`); Missing bar and Odd seven check raw segment masks where the glyph is not a known character.
- Times: the self-test fails test 1 within about 100 ms of reset and passes tests 1–5 by `888888` at about 300 ms; `PA55` is about 3.2 s simulated, about 83 s in debug a seed. Repair checks use the monitor's `0200` (40 ms) or `888888` (300 ms); only Deaf keypad runs to `PA55`. The self-test writes before it reads, so its codes are checked on seed 1; monitor symptoms on seeds 1–3.
- The self-test's code: `display()` after the failure is reached (test 1 within about 100 ms of reset; tests 2–3 and the ROM sums later: times measured in the phase and written in the case).

## 3. The Puzzles, file by file

Each Puzzle: `puzzles/<file>.bwp` (ADD_PUZZLE File: comment line, `machine bw1`, `title`, `brief` symptom only), one `TEST_CASE("bw1: <file>", "[machines][slow]")` in `tests/machines/bw1_puzzles_test.cpp`, its name in `bw1: Puzzles have a title and a brief`. Each is first tried as an `InlinePuzzle` (or a scratch `.bwp` and `bw_ui --screenshot`) on seeds 1–3; what it shows decides the brief.

Cracks: read the Net's routing order in `boards/bw1/bw1.kicad_pcb` (its `segment`s) before choosing pads and `%` (ADD_PUZZLE); a cached `const Crack& <Name>Crack()` in `tests/machine/test_points.{hpp,cpp}` for each.

| # | File | Fault line (to be confirmed) | Notes |
|---|---|---|---|
| 1 | `bad_byte` | `fault cell U5.DOUT $300 1` | a word outside `$0000`–`$01FF` (the monitor's zero page and stack) and outside `$0200`–`$0206` (where the brief has the owner type), so the monitor works. The brief: `AD 0300 DA 00` shows `0300 80`. |
| 2 | `odd_seven` | `fault cell U2.D<n> rom $3EE <v>`, `n` 0–6 | `TABLE` + 7 is `$1FEE`, ROM word `$3EE` (`$87`: a, b, c). Bit 7 drives the unconnected `PA7`: shows nothing. A bit whose glyph is no other digit's (`D3` set: a, b, c, d). The brief: `AD 0277` shows the 7s wrong. |
| 3 | `missing_bar` | `fault stuck U17.2Y3 0` | `0200`: the `2`s lose their middle bar. |
| 4 | `half_segment` | `fault crack DSa.G DSb.G N%` (or another segment) | the `SEG_x` Trace's order along the digits; the dark digits are those past the crack. |
| 5 | `bright_mess` | `fault stuck U24.Y6_N 0` | `DS3` selected with every digit: its segments the OR of all six. |
| 6 | `dead_da` | `fault crack U17.8 SW18.2 N%` | `PA3` routed `U2.38` → `U17.8` → `SW18.2` → `SW11.2` → `SW4.2`: the crack after `U17` kills DA, A and 3 and leaves `SEG_D` lit (a crack before `U17` floats `U17.1A4` high and lights `SEG_D` instead). |
| 7 | `dead_row` | `fault crack U24.3 SW20.1 N%` | `ROW2` routed `U24.3` → `SW20` → `SW17` → `SW18` → `SW21` → `SW15` → `SW16` → `SW19`: this crack kills all seven. |
| 8 | `deaf_keypad` | `fault switch SW<n> closed` | the monitor's `START` waits for every key to be released (`@up`), so one key held forever blocks all. Self-test: `PA55` and the key's code with no key held. A key whose code is not a hex digit's shows clearer (`PA5512` for `+`). |
| 9 | `self_test_fails` | `fault cell U3.D<n> ram $xx <v>` | `U3`'s RAM is used by nothing else; any word. Code `37xxbb`. |
| 10 | `no_stop` | `fault cell U2.D<n> ram $3A 1` | `$17FA` is `U2` RAM `$3A`; `COLD` writes `$00` there (`NMIV` = `$1C00`), so only a stuck 1 shows. The bit chosen so the NMI lands somewhere seen (a hang, garbage, not a silent stop): run. `BRK` uses `IRQV` (`$17FE`) and still stops. Code `27FAbb`. |
| 11 | `dead_machine` | `fault column U9.DOUT 0` | seen on seed 1: digits dark. Code `110008`: test 1 writes `$00` at `$0000` (reads right), `$FF` at `$0100` reads `$F7`. |
| 12 | `wrong_suspect` | `fault crack U9.DOUT U13.3A N%` | `DOUT3` is a two-pad Net: `U13`'s `3A` (TTL) floats high, bit 3 reads 1: code `100008`. The case also swaps `U9` and checks the failure stays. |
| 13 | `half_byte` | `fault crack` on `BUF_N`'s F.Cu stretch at x 160.41 (from near `U14.1`, y 74.96, to `U13.4`, y 43.58) | it leaves all four `U13` enables off `U15`'s side (they float high: disabled) and `U14` on. Bits `D0`–`D3` read bus charge. |
| 14 | `two_bits` | `fault bridge U1.31 U1.30` | a bridge joins pins of one chip within 2.6 mm (`machine.cpp`). On `U13`/`U14` no two neighbours carry two bits; only the data bus pins of `U1`, `U2`, `U3` (26–33). On `U1` (`D2`, `D3`) the bridge breaks every ROM fetch: the self-test dies too, so the Puzzle is group C (the logic analyzer shows `D2` = `D3`). |
| 15 | `aliased_page` | `fault stuck U7.A8 0` | pages 0 and 1 (2 and 3) alias in bit 5: test 1 writes `$0100` after `$0000` into the same cell, so it fails at `$0000`: code `100020`. The monitor's zero page and stack alias in one bit: it may die or not. Dropped if the symptom depends on the seed beyond what a brief can say. |
| 16 | `rom_test_fails` | `fault cell U3.D<n> rom $3xx 0`, a word `$375`–`$3FF` | the `$FF` fill after the checksum byte (`$1B74`; `TABLE3` at `$364` is read to draw digits): read only by the sum, so stuck 0. Code `5800xx`. |
| 17 | `wrong_block` | `fault stuck U4.A 0` (or 1) | stuck 0: `K5` and `K7` never decode (no ports, no RIOT RAM, no monitor ROM), the vectors come from the 003's `$FF` fill at `$1BFA`: dead, strapped or not. Stuck 1: `$1800` decodes as `K7`, so the strap's `jmp SELFTEST` runs `U2`'s word `$000`: the self-test unreachable too. Run both; the clearer on the probe (`K7` silent or `K6` silent) is the Puzzle. |
| 18 | `held_reset` | `fault switch SW23 closed` | `RES_N` low: the 6502 and both 6530s held. |
| 19 | `sst_kills` | `fault stuck U15.3B 1` | `SST_N` falls on every opcode fetch, the monitor's too: with SST at ON each NMI lands in the NMI routine. The case slides `SW24` and checks the display stops. |

## 4. Machine docs

- [BW1.md](../machines/120-bw1/BW1.md) `## Puzzles`: the list by group, a line each (name, what the owner sees), as `HEARTBEAT.md#puzzles`.
- [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md): a Puzzle list (file, Fault, symptom on seeds 1–3, repair) and the release screenshots (ADD_PUZZLE Docs).

## 5. Release check

Puzzles only: the tests that reach the BW-1 each phase (`ctest --preset debug -R "bw1|manuals|puzzle"`); the full suite once at the end (a roadmap step's end). `bw_ui puzzles/<file>.bwp --screenshot … --after 1s` for each, viewed against its brief; Puzzles shown through the self-test also with `--repair wire:U24.4:U2.2`.
