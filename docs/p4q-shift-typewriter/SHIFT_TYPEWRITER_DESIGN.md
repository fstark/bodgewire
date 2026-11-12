# P4q Shift-register TV typewriter — Detailed Design

Implements [SHIFT_TYPEWRITER.md](SHIFT_TYPEWRITER.md): `EvalContext::randomBit`, the DIP-8 package, the 2524 and 2518 Parts, and the 090 Machine redone on them ([TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md), built as in [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md)).

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (both Parts, the package), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (every step but 4, Puzzles: P4r). This design gives only what they leave open. Code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

## 1. Files

```
src/sim/component.hpp, simulator.cpp            EvalContext::randomBit (§2)
boards/footprints/DIP-8_W7.62mm.kicad_mod       copied from KiCad unchanged (§3)
src/board/kicad_netlist.cpp, placer.cpp         DIP-8 row, grid package
src/parts/shift_register_2524.hpp, .cpp         ShiftRegister2524 (§4)
src/parts/shift_register_2518.hpp, .cpp         ShiftRegister2518 (§5)
src/parts/delays.hpp, CMakeLists.txt, catalogue.cpp
datasheets/2524.md, 2518.md
tests/sim/…                                      randomBit case (the file holding the PowerOnContext cases)
tests/board/footprint_test.cpp, kicad_netlist_test.cpp, placer_test.cpp
tests/parts/shift_register_2524_test.cpp, shift_register_2518_test.cpp, catalogue_test.cpp
tests/CMakeLists.txt
netlists/typewriter.bwn                          replaced, two stages (§6)
tests/machines/typewriter_test.cpp               §7
manuals/typewriter/user.md, service.md          TYPEWRITER_DESIGN.md § Manuals
boards/typewriter/typewriter.place, .kicad_pro, .kicad_pcb
tests/board/boards_test.cpp, tests/machine/electrical_test.cpp,
    tests/view/board_scene_test.cpp              "typewriter" in the per-Machine loops
tests/session/session_test.cpp                  §8
docs/recipes/ADD_PART.md, ADD_MACHINE.md        §10
```

## 2. `EvalContext::randomBit`

[component.hpp](../../src/sim/component.hpp), beside `readBit`:

```cpp
bool randomBit();  // seeded random bit, as PowerOnContext::randomBit
```

Defined in [simulator.cpp](../../src/sim/simulator.cpp) as `return sim_.rng_.nextBit();`. A test in the file that already tests `PowerOnContext::randomBit` / `readBit` (find it with `grep -rln readBit tests/sim`): a small Component that, on a clock edge, records 16 `randomBit()`s; the same seed gives the same bits, another seed other bits.

## 3. DIP-8

ADD_PART "A new package": `DIP-8_W7.62mm.kicad_mod` copied from `/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/Package_DIP.pretty/`; `{"DIP-8", "Package_DIP:DIP-8_W7.62mm"}` first in `kFootprints` (array size 14); `"DIP-8"` in `IsGridPackage` (a DIP-8 is shorter than a DIP-16, whose slot it takes). Tests as `c547493`: `footprint: DIP-8 pads and courtyard` (8 pads, pad 5 at (7.62, 7.62) mm; the courtyard read from the file), `KicadFootprint("DIP-8")`. The placer case needs a catalogue Part in DIP-8, so it comes with the 2524 (§4): `placer: DIP-8 sits in the chip grid`, as `placer: DIP-18 and DIP-20 sit in the chip grid` with a `part U1 2524`.

## 4. 2524

```cpp
// src/parts/shift_register_2524.hpp
// 512-bit dynamic recirculating shift register (Signetics 2524), P-MOS. Pins per datasheet:
// 1 PHI2, 2 OUT, 3 READ, 4 VDD, 5 WRITE, 6 IN, 7 PHI1, 8 VCC. Two clocks pulsed low in turn: on
// the rise of PHI2 the bit at the output is rewritten from IN if WRITE is high, else kept
// (recirculated); on the rise of PHI1 the next bit comes to OUT, or 0 while READ is low.
// Dynamic: if either clock has not risen for k2524Hold, the bits go random. VDD (-5 V)
// is not connected on these Boards; no GND pin. Inputs read X as a random bit.
class ShiftRegister2524 final : public Component {
public:
    explicit ShiftRegister2524(std::string reference);
    static constexpr PinIndex kPhi2 = 0, kOut = 1, kRead = 2, kWrite = 4, kIn = 5, kPhi1 = 6;
    static constexpr std::size_t kBits = 512;

    void powerOn(PowerOnContext& ctx) override;  // random bits
    void evaluate(EvalContext& ctx) override;
    bool bit(std::size_t offset) const { return bits_[(head_ + offset) % kBits]; }  // 0: at OUT

private:
    std::bitset<kBits> bits_;
    std::size_t head_ = 0;  // the bit at the output
    bool out_ = false;      // the output stage: bits_[head_] as of the last PHI1 rise
    Time lastPhi1_ = 0, lastPhi2_ = 0, lastDecay_ = 0;
};
```

Pins: `PHI1`, `PHI2` `{Input, Mos, risingEdgeOnly = true}`; `READ`, `WRITE`, `IN` `{Input, Mos}`; `OUT` `{Output, Mos}`; `VDD` `{NoConnect, Mos}` (MOS: `catalogue_test.cpp` wants every non-Power pin of a `kMosSignals` Part MOS); `VCC` `{Power, Ttl}`.

`delays.hpp`: `inline constexpr Time k2524ClockToOutput = 100'000;  // tA+, PHI1 to OUT (maximum, no typical given)` and `inline constexpr Time k2524Hold = 10'000'000'000;  // 10 ms: bits kept with a clock stopped (P4q spec)`.

```
evaluate:
    phi1 = rose(PHI1); phi2 = rose(PHI2)
    if phi1 or phi2:
        since = max(min(lastPhi1_, lastPhi2_), lastDecay_)
        if now - since > k2524Hold:  for each bit: bits_[i] = randomBit();  lastDecay_ = now
        if phi1: lastPhi1_ = now;  if phi2: lastPhi2_ = now
    if phi2 and readBitBefore(WRITE):  bits_[head_] = readBitBefore(IN)
    if phi1:  head_ = (head_ + 1) % kBits;  out_ = bits_[head_]
    drive OUT = readBit(READ) && out_ ? One : Zero, k2524ClockToOutput
```

- Both clocks rising at the same timestamp (a fault tying them): write, then shift, in that order.
- `drive` does nothing when the value is unchanged, so an evaluation on `READ`, `WRITE` or `IN` alone re-drives the same value. `READ` acting at once (the datasheet ANDs it with `PHI1`) is a simplification noted in the datasheet file.
- A write never changes `OUT` (`out_` moves only on `PHI1`): the written bit comes out 512 shifts later, as on the real chip. The decay leaves `out_` alone.
- A clock that never rises again never triggers the decay itself: the bits are only seen through `OUT`, which moves only on `PHI1`, so the lazy check at the next rise of either clock is enough. While one clock is stopped, the other's rises re-randomise every 10 ms (`lastDecay_`).
- `powerOn`: 512 random bits, `head_` 0, `out_ = bits_[0]`, `lastPhi1_ = lastPhi2_ = lastDecay_ = 0`, `setOutput(OUT, out_)` (`READ` is not known yet; the first evaluation fixes it). The first frame's pause (power-on to the first top line) is under 10 ms.
- No allocation (`std::bitset`).
- Catalogue: `PartInfo{"2524", "512-bit dynamic recirculating shift register", "DIP-8", Chip, &CreateChip<ShiftRegister2524, "2524">}` sorted by name (`"2513" < "2518" < "2524" < "2716"`).

Datasheet `datasheets/2524.md` (ADD_PART "Files"): Function in plain words (512 cells in a ring, round once per 512 clock pairs; write vs recirculate; `READ`; dynamic: keep the clocks running, at least one pulse every 10 ms here, the real chip's guaranteed rate about 1 kHz at 25 °C; the real clocks are +5/−12 V pulses from a driver, here TTL; the real output only pulls high, the board's resistor pulls it low: here an ordinary output; `VDD` −5 V not connected), Pins (every name).

## 5. 2518

```cpp
// src/parts/shift_register_2518.hpp
// Hex 32-bit static shift register (Signetics 2518), P-MOS. Pins per datasheet: 1 IN4, 2 IN5,
// 3 IN6, 4 REC, 5 VGG, 6 CLK, 7 OUT6, 8 OUT5, 9 OUT4, 10 OUT3, 11 OUT2, 12 OUT1, 13 IN1, 14 IN2,
// 15 IN3, 16 VCC. On the rise of CLK each register puts the bit at its output back in (REC
// high, "Recirculate") or takes its IN (REC low), and the next bit comes out. VGG (-12 V) is not connected on
// these Boards; no GND pin. Inputs read X as a random bit.
class ShiftRegister2518 final : public Component {
public:
    explicit ShiftRegister2518(std::string reference);
    static constexpr PinIndex kRec = 3, kClk = 5;
    static constexpr std::array<PinIndex, 6> kIn{12, 13, 14, 0, 1, 2};    // IN1..IN6
    static constexpr std::array<PinIndex, 6> kOut{11, 10, 9, 8, 7, 6};    // OUT1..OUT6
    static constexpr std::size_t kWords = 32;

    void powerOn(PowerOnContext& ctx) override;  // random words
    void evaluate(EvalContext& ctx) override;
    std::uint8_t word(std::size_t offset) const { return words_[(head_ + offset) % kWords]; }  // bit i: register i + 1

private:
    std::array<std::uint8_t, kWords> words_{};  // bit i of a word: register i + 1 (OUT1 = bit 0)
    std::size_t head_ = 0;
};
```

`delays.hpp`: `inline constexpr Time k2518ClockToOutput = 300'000;  // tA, CLK to OUT (typical; maximum 350 ns)`.

```
evaluate:
    if rose(CLK):
        if !readBitBefore(REC):  words_[head_] = bits of readBitBefore(IN1..IN6)
        head_ = (head_ + 1) % kWords
    drive OUTk = bit k-1 of words_[head_], k2518ClockToOutput
```

Pins: `CLK` `{Input, Mos, risingEdgeOnly}`; `REC`, `IN1`–`IN6` `{Input, Mos}` (read only at the edge, as the 74LS164's `A`/`B`: their own changes re-drive the same outputs, which `drive` ignores); `OUT1`–`OUT6` `{Output, Mos}`, `VGG` `{NoConnect, Mos}`, `VCC` `{Power, Ttl}`. Catalogue row `"2518", "hex 32-bit static shift register", "DIP-16"`. Datasheet `datasheets/2518.md` as the 2524's (static: the clock may stop high; the real output; `VGG` not connected).

## 6. Catalogue test and netlist stages

`tests/parts/catalogue_test.cpp`: `"2524"` and `"2518"` in `kMosSignals`; a `kNoGround[] = {"2524", "2518"}`: for these, `names.contains("GND")` is not required and the corner check is only `pins[count - 1].name == "VCC"`.

`netlists/typewriter.bwn` replaced (ADD_MACHINE step 2, "A big Machine"), each stage a commit with its cases:

1. **Video from the page.** `X1`, `U1`–`U13`, `U14` gates 1–2, `U15`, `U16`–`U25`, `U29` gate 3, `J1`, `J2`: the final netlist's nets for these, with `WRITE` and `W0`–`W5` on `GND` (the page only recirculates), `U14` gates 3–4 and `U29` gates 1, 2 and 4 tied, `U12` gates 2–4, `U11` gates 3–4 and `U5` gates 4–5 tied, `U38` absent so `ACTIVE` has one load fewer, and `VIDEO_D` straight to `U25.2D` (no `U36`). The header comment says what is missing. Release speed recorded (`bw run netlists/typewriter.bwn --time 2s`).
2. **Keyboard, writes, cursor, Clear.** Everything else: the final netlist, TYPEWRITER_DESIGN.md § Netlist word for word.

## 7. Tests: `tests/machines/typewriter_test.cpp`

P4p's file, kept where it still applies: the picture helpers (`Lit`, `Font`, `ExpectedPage`, `Matches`, `FindPage`, `ShownAt`, `LitOutside`), `DesignCircuit`, the `Typewriter` struct's `clear()`, `cursor()`, `lockedFrames()`. Changed:

- `codes()` (not `const`): run until `V8` is 1 (steps of 100 µs; the vertical blanking lasts 3.5 ms), then cell `i` = bits 0–5 from `U16`–`U21` `bit(i)`: after the last top line of a frame the page's output holds cell 0 (TYPEWRITER_DESIGN.md, Timing notes). The picture case checks this mapping against the TV.
- `type(keys)`: hands the keys to `J3` (at most 32 at a time: `kKeyboardWaiting`), waking it, and runs 26 ms a key plus 20 ms: each key's `STROBE` is 25 ms and a little, and the last key's cell comes round within a frame of its `STROBE`. No `queued` flag: the keyboard always queues.
- `clear()` unchanged (25 ms held, more than a frame).
- `#include "parts/shift_register_2524.hpp"` instead of the 2114's.

Cases (stage that brings them in brackets; `[machines]`, `[slow]` where the TV is read or more than about 0.4 s is simulated):

- [1] `typewriter: netlist loads without warnings`.
- [1] `typewriter: the TV shows the page` (`[slow]`), seeds 1 and 2: the second and third locked frames are each `FindPage(ExpectedPage(codes()))` (no cursor: its Nets do not exist in stage 1, and `nets.at` would throw), found at `left` 30–38, `top` ≤ 8, nothing lit outside. In stage 2 each frame is `ShownAt(frame, codes(), *cursor())` (the cursor's cell inverted or plain: it blinks every 8 frames). `codes()` runs the simulator, so it is not `const`.
- [2] `typewriter: Clear blanks the page and homes the cursor`, seeds 1–3: every cell 0x20, cursor (0, 0).
- [2] `typewriter: keys land at the cursor`: after Clear, `"HELLO, WORLD"`: cells 0–11 its codes `& 0x3F`, cell 12 a space, cursor (12, 0).
- [2] `typewriter: Return starts a line, other control keys do nothing`: after Clear, `"AB\rC"`, then Backspace (8), Esc (27), CTRL-A (1): row 0 `AB`, row 1 `C`, every other cell a space, cursor (1, 1).
- [2] `typewriter: lines wrap` (`[slow]`): after Clear, 33 letters `A`… (cycling the alphabet): cells 0–32, cursor (1, 1).
- [2] `typewriter: the page wraps` (`[slow]`): after Clear, 16 Returns: cursor (0, 0); then `Z`: cell 0 is `Z`, cursor (1, 0).
- [2] `typewriter: the typed page and a blinking cursor` (`[slow]`), seed 1: Clear, `"HELLO\rWORLD"`, then 20 locked frames: each is the page with cursor (5, 1) inverted or plain, both seen, the rest of each frame identical to the first's.

P4p's `keys typed at once all land` is gone (every typing queues now; `lines wrap` hands over 32 then 1). Debug runs P4p's Machine at about 0.016× real time; a key costs 26 ms simulated, so `lines wrap` is about 0.9 s simulated, about a minute in debug.

## 8. Session test

`session: typewriter with every instrument` (`[session][slow]`), as P4p planned: 8 clips (`U23` `O1`–`O5`, `U16.OUT`, `U24.SH_LD_N`, `U24.QH`), hooks on `U1.RCO` and `U15.2Y` (`PHI2_N`), the probe on `U24.QH`, the meter on `U23.VCC`; `session.keyboards() == {"J3"}`; `powerOn(1, 500_ms)`, then `CHECK(session.type("J3", U'A'))` before `waitIdle`; the TV locked and more than 15 frames.

## 9. Board

TYPEWRITER_DESIGN.md § Board, ADD_MACHINE step 3. The untracked P4p files in `boards/typewriter/` are replaced (spec). They must go before the new netlist does (phase 4): `tests/CMakeLists.txt` globs pick up `drc_typewriter` and `place_drc_typewriter`, and `bw_ui netlists/typewriter.bwn` loads a Board it finds beside the netlist, so P4p's Board would be checked and loaded against the new netlist. Phase 4 deletes `typewriter.kicad_pcb`, `temp-freerouting.dsn` and `.history/`, keeps `.kicad_pro` and `.kicad_prl`, and writes the new `typewriter.place` (`U23` for the 2513); phase 7 places the Board. Phases 4–6 run from the netlist alone. Before the Board the tests use `DesignCircuit()`; once it is routed, every case but `loads without warnings` builds from the Board (`BoardCircuit()`, as in `message_test.cpp`).

## 10. Recipes

- ADD_PART: reference commits for the 2524 (dynamic shift register: two clocks, a random decay with `EvalContext::randomBit`, a test accessor relative to the output) and the 2518 (several registers on one clock), DIP-8; a rule: "A P-MOS chip without a `GND` pin (2524, 2518): its negative supply pin is `NoConnect`, and it goes in `kNoGround` in `catalogue_test.cpp`."; "Dynamic storage: keep the time of the last clock and lose the contents lazily at the next one; no wake needed, the contents only show through outputs that move on a clock."
- ADD_MACHINE: reference commits (090 shift-register stages, Board); gotcha "Clocks decoded from the dot counter: make each edge come from one counter bit (090 `PHI1_N`, `PHI2_N`, `ROW_CLK`), and check that the gating signals change only on edges where that bit already holds the gate off."; step 5's note on keyboards stays.

## 11. Release check

ADD_MACHINE step 5:

```
build/release/src/cli/bw run netlists/typewriter.bwn --time 2s
build/release/src/cli/bw run netlists/typewriter.bwn --press SW1:1ms:30ms --time 2s
build/release/src/ui/bw_ui netlists/typewriter.bwn --press SW1:300ms:400ms \
    --type 'J3:500ms:HELLO, WORLD\nTV TYPEWRITER' --after 4500ms --screenshot /tmp/typewriter.png \
    --probe U24.QH --clip … --hook … --meter …
```

(`--type` sends one key every 100 ms, slower than the 25 ms strobe; the last key at 3.0 s simulated.) View the PNG: the two typed lines top left, the block after `TV TYPEWRITER` (on or off), the keyboard window. Ratios and UI time recorded with and without instruments, compared with P4p's in the plan's notes.
