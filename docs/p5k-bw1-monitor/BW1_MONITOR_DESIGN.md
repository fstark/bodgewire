# P5k BW-1 monitor — Detailed Design

Implements [BW1_MONITOR.md](BW1_MONITOR.md): the `.checksum` directive, the monitor `roms/bw1_002.asm`, the self-test `roms/bw1_003.asm`, and a model of the KIM around the 6502 core to test them. No Part, netlist, Board or UI change.

Recipes: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 (a program: `roms/<m>.asm`, header comment, `bw asm`, the `asm_<m>` ctest; vectors read from the `.rom` in tests). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md). Sources: the KIM-1 User Manual (`6502.org/documents/books/kim1_user_manual.pdf`, scanned; cited "KIM Fig. N", "KIM listing" for Appendix I's listing and symbol table).

---

## 1. `.checksum` — `src/asm/asm6502.{hpp,cpp}`

- Header comment: `.checksum` added to the directive list: "one byte making the sum of the whole ROM 0 (mod 256), once".
- `directive`: `.CHECKSUM` takes no argument (any → `".checksum takes no value"`); a second one → `".checksum given twice"` (checked in pass 1, as `.rom given twice`). It records `checksumAt_` (the offset in `words_`, pass 1 and 2) and emits `checksum_` (0 until known).
- `run()`: after pass 2, if a `.checksum` was seen: `checksum_ = (256 − Σ words_ without that byte) & 0xFF`, and pass 2 runs once more with it (the loop body made a lambda `pass(int)`; `words_` refilled with `$FF`, `written_` cleared, `text_` cleared before each pass 2), so the `.rom` text carries the real byte on its line. Pass 2 is deterministic, so the second run differs only in that byte.
- `emit`'s "written twice" check stays: the fixed entry points (§2) rely on it to catch code running into the next one.

Tests (`tests/asm/asm6502_test.cpp`):
- `asm: .checksum makes the ROM sum to 0`: a `.rom $F000, $400` program with code, a `.checksum` in the middle and code after it; the words sum to 0 mod 256; the byte at its address is the one in the `.rom` text (`ParseRomContents` of the text gives the same words); a program without one is unchanged.
- errors: `.checksum 5`, two of them, `.checksum` before `.rom` (`no .rom BASE, SIZE before the first byte`).

## 2. The monitor — `roms/bw1_002.asm`, `roms/bw1_002.rom`

`.rom $1C00, $400`. Header comment: BW-1 (spec path), the memory map (BW1_MONITOR.md, "The KIM it runs on"), the entry-point table, the zero page. Every name below is the KIM's (KIM listing), our code under it.

### 2.1 Constants

```
PCL = $EF  PCH = $F0  PREG = $F1  SPUSER = $F2  ACC = $F3  YREG = $F4  XREG = $F5
INL = $F8  INH = $F9  POINTL = $FA  POINTH = $FB  TEMP = $FC  TMPX = $FD  MODE = $FF
NMIV = $17FA  IRQV = $17FE
SAD = $1740  PADD = $1741  SBD = $1742  PBDD = $1743
SELFTEST = $1800        ; bw1_003.asm's entry
DIGIT1 = $08            ; SBD for digit 1; +2 a digit (U24 outputs 4-9)
STRAP = $06             ; SBD for U24 output 3, the strap's line
DELAY = 100             ; dey/bne loops a digit: about 0.5 ms
```

`MODE` 1 is address mode (the KIM's). `TEMP` holds `Y` in `CONVD`; `TMPX` the rows' AND in `AK`.

### 2.2 Fixed addresses

Each placed by `.org` at its KIM address; between them, unwritten `$FF`.

| `.org` | Code | Bytes |
|---|---|---|
| `$1C00` `SAVE` | `sta ACC` · `pla` · `sta PREG` · `SAVE1:` `pla` · `sta PCL` · `sta POINTL` · `pla` · `sta PCH` · `sta POINTH` · `SAVE2:` `sty YREG` · `stx XREG` · `tsx` · `stx SPUSER` · `jsr INITS` · `jmp START` | 28, to `$1C1B`; `SAVE1` lands on `$1C05`, `SAVE2` on `$1C0F` |
| `$1C1C` `NMIT` | `jmp (NMIV)` | 3 |
| `$1C1F` `IRQT` | `jmp (IRQV)` | 3 |
| `$1C22` `RST` | §2.3 | about 30, before `$1C4F` |
| `$1C4F` `START` | §2.4 | the main loop and the routines' bodies follow it |
| `$1EFE` `AK` | `jmp AKX` | 3 |
| `$1F19` `SCAND` | `ldy #0` · `lda (POINTL),y` · `sta INH`, falling into `SCANDS` | 6 |
| `$1F1F` `SCANDS` | `jmp SCANX` | 3 |
| `$1F40` `KEYIN` | `lda #0` · `sta PADD` · `jmp AKX` | 8 |
| `$1F48` `CONVD` | `jmp CONVX` | 3 |
| `$1F63` `INCPT` | `inc POINTL` · `bne @d` · `inc POINTH` · `@d: rts` | 7, to `$1F69` |
| `$1F6A` `GETKEY` | `jmp GETKX` | 3 |
| `$1FE7` `TABLE` | `.byte $BF, $86, $DB, $CF, $E6, $ED, $FD, $87, $FF, $EF, $F7, $FC, $B9, $DE, $F9, $F1` | 16 (KIM listing's first 8; the other 8 are `8`–`F` drawn the same way, bit 7 set as the KIM's) |
| `$1FF7` | `.checksum` | 1 |
| `$1FFA` | `.word NMIT, RST, IRQT` | 6 |

The test (§5.1) reads every fixed address from the `.rom` and checks the opcode there (`$85` at `SAVE`, `$6C` at `NMIT` and `IRQT`, `$A9` at `RST` and `KEYIN`, `$A0` at `SCAND`, `$E6` at `INCPT`, `$4C` at the stubs), so a moved routine fails a test, not only a KIM program.

### 2.3 Reset

```
RST:    lda #0
        sta PADD        ; PA inputs (RES did it; RS reaches here the same way)
        lda #$1E
        sta PBDD        ; PB1-PB4 outputs: U24's inputs
        lda #STRAP
        sta SBD         ; U24 output 3 low: the strap pulls PA0 low if closed
        lda #$01
        bit SAD         ; read at least 5 us after the write: the port's 1 us, then U24
        bne @mon
        jmp SELFTEST    ; strap closed: no RAM touched so far
@mon:   ldx #$FF
        txs
        jmp COLD
```

`COLD` (after `START`'s code): `stx SPUSER` (`$FF`); `lda #0`, `sta ACC`, `XREG`, `YREG`, `PREG`; `lda #<SAVE`, `sta NMIV`, `sta IRQV`; `lda #>SAVE`, `sta NMIV+1`, `sta IRQV+1`; `POINTL` = `$00`, `POINTH` = `$02`; `jsr INITS`; `jmp START`. The first `jsr` uses the stack at `$01FE`–`$01FF`, the zero page and the RIOT RAM vectors are written: bad RAM there stops the monitor here.

`INITS`: `MODE` = 1, then `INIT1`: `PBDD` = `$1E`, `PADD` = 0; `rts`.

### 2.4 The loop

```
START:  jsr INIT1
@up:    jsr SCAND       ; shows the address and its byte; A = 0 when no key is held
        bne @up         ; wait for every key to be released
@down:  jsr SCAND
        beq @down
        jsr SCAND       ; still held a scan later (about 3 ms)
        beq @down
        jsr GETKEY
        cmp #$15
        bcs START       ; none (released meanwhile)
        cmp #$10
        bcc HEX
        ...             ; $10 AD: MODE = 1; $11 DA: MODE = 0; $12 +: jsr INCPT;
                        ; $13 GO: GOEXEC; $14 PC: POINTL/H = PCL/H. Then jmp START.
HEX:    sta TEMP
        lda MODE
        beq @data
        ldx #4
@shift: asl POINTL
        rol POINTH
        dex
        bne @shift
        lda POINTL
        ora TEMP
        sta POINTL
        jmp START
@data:  ldy #0
        lda (POINTL),y
        asl ×4
        ora TEMP
        sta (POINTL),y
        jmp START
GOEXEC: ldx SPUSER
        txs
        lda POINTH
        pha
        lda POINTL
        pha
        lda PREG
        pha
        ldx XREG
        ldy YREG
        lda ACC
        rti
```

No `adc` or `sbc` anywhere in the monitor: it runs with the program's `D` flag after a stop.

### 2.5 Display and keys

- `SCANX`: `lda #$7F`, `sta PADD` (segments out; no digit is selected yet: `SBD` is on a row from the last `AK`); `ldx #DIGIT1`; `ldy #3`; per byte (`INL,y`: `y` = 3 `POINTH`, 2 `POINTL`, 1 `INH`; `lda abs,y` as there is no `lda zp,y`): high nibble (`lsr` ×4), `jsr CONVX`, then `lda INL,y` again (`CONVX` keeps `Y`, not `A`) and the low one, `jsr CONVX`; `dey`, `bne`; then `jmp AKX`.
- `CONVX` (`CONVD`): `sty TEMP`; `and #$0F`; `tay`; `lda TABLE,y`; `ldy #0`; `sty SAD` (blank before the digit changes: no ghost of the last digit on the next); `stx SBD`; `sta SAD`; `ldy #DELAY`; `@d: dey`, `bne @d`; `inx`, `inx`; `ldy TEMP`; `rts`. About 560 cycles a digit, about 3.5 ms a scan with `AK` (measured in phase 3), about 290 scans a second.
- `AKX` (`AK`): `lda #0`; `sta SBD` (row 0 first: the last digit goes dark before port A floats high); `sta PADD`; `lda SAD`, `ora #$80`, `sta TMPX`; row 1 (`$02`) and row 2 (`$04`) the same, each `and TMPX`, `sta TMPX`; `eor #$FF`: 0 when no column is low; `rts`. Each `lda SAD` reads at least 3 µs after its `sta SBD` (the port's 1 µs and U24).
- `GETKX` (`GETKEY`): `lda #0`, `sta PADD`; `ldx #0` (`SBD` of row 0), `ldy #$FE` (row base − 2); per row: `stx SBD`; `lda SAD`, `ora #$80`, `eor #$FF`; `bne @found`; `iny` ×7, `inx` ×2, `cpx #6`, `bne`; `lda #$15`, `rts`. `@found`: `iny`, `asl`, `bcc @found`; `tya`; `rts`: `PA6` (the first bit shifted out after bit 7) gives base + 0, `PA0` base + 6; with two keys, the lowest code.

## 3. The self-test — `roms/bw1_003.asm`, `roms/bw1_003.rom`

`.rom $1800, $400`, entry `SELFTEST` at `$1800` (the 002's constant). The 002's port and segment constants repeated (no include in `bw asm`), with `TABLE3`, the hex table again, and `SEG_P` = `$F3`, `SEG_A` = `$F7`, `SEG_S` = `$ED` (bit 7 set as the table's). No `jsr`, no stack, no zero page; `S` holds a byte (`txs`/`tsx`).

```
SELFTEST: sei
        cld
        lda #$7F
        sta PADD        ; segments out
        lda #$1E
        sta PBDD
        lda #DIGIT1
        sta SBD         ; digit 1 alone, held while a test runs
        lda TABLE3+1
        sta SAD         ; "1"
        ...
```

- **Test 1**, main RAM. Pass A: for `X` = 0..255: `$0000,x` ← `X`, `$0100,x` ← `X ^ $FF`, `$0200,x` ← `X ^ $55`, `$0300,x` ← `X ^ $AA`; then for each, `txa`, `eor #pattern`, `eor $0n00,x`: non-zero is the bad bits, to `FAILn` (`beq` past a `jmp FAILn`, as the entries are out of branch range). Pass B: the same with the complement of each pattern (pages 0 and 1 swap theirs, 2 and 3). About 22 ms a pass.
- `FAIL` entry per failing page or chip: `tay` (bits), then the address's low byte to `S` (`txs` for main RAM; `txa`, `ora #$C0` or `#$80`, `tax`, `txs` for the RIOT RAMs), then `lda #$1n` (main RAM page `n`), `#$27`, `#$37`, `#$4C`, `#$58`; `sta PADD3` (`$1701`: the 003's port A direction register, read back as written); `jmp FAIL`. Its pins drive whatever the 003's port A data register holds (random after `1800` GO without a reset): P5l must leave the 003's `PA` pins unconnected.
- **Tests 2, 3**, `$17C0,x` and `$1780,x`, `X` = 0..63: `X ^ $A5`, then `X ^ $5A`. Digit 1 `2`, `3`.
- **Tests 4, 5**: `lda #0`; per page of the ROM: `ldx #0`, `@s: clc`, `adc $1C00,x`, `inx`, `bne @s` (four pages); `tay` (sets `Z` from the sum, which `inx` had left set, and puts it where `FAIL` shows the bits), `beq` past a `jmp FAIL4` (`S` ← 0). Then `$1800`. Digit 1 `4`, `5`.
- **Test 6**, display: `SAD` = `$7F`; `Y` = 0 (256 frames): `A` = `DIGIT1`, per digit `sta SBD`, `ldx #DELAY`, `dex`/`bne`, `clc`, `adc #2`, `cmp #DIGIT1+12`, `bne`; `dey`, `bne`: about 0.8 s. Then each digit alone: `A` per digit, `X` and `Y` nested 256 × 256 (`dex`/`bne` inside `dey`/`bne`): about 0.33 s each.
- **Keypad echo**, forever: the keys into `Y` as `GETKX` (inline: `PADD` = 0, rows, `$15` if none); `PADD` = `$7F`; digits 1–4 `SEG_P`, `SEG_A`, `SEG_S`, `SEG_S` (not `A` or `S`: a constant named like a register reads as one); digits 5–6 `Y`'s hex digits, or blank for `$15`; `jmp` back.
- **`FAIL`**, forever: digit 1 the high nibble of `PADD3`, digit 2 its low nibble, digits 3–4 `S` (`tsx`, `txa`), digits 5–6 `Y` (`tya`).

A digit, inline (`X` free, `A` the segments): `ldx #0`, `stx SAD`, `ldx #sel`, `stx SBD`, `sta SAD`, `ldx #DELAY`, `@dN: dex`, `bne @dN` (`@d1` … `@d6`: local labels must differ within a scope). Six of them in `FAIL` and six in the echo; the nibble lookups use `X` as the index before the digit code loads it.

`.checksum` at the end of the code.

## 4. The model — `tests/machines/bw1_model.{hpp,cpp}` (new, test code)

```cpp
// The KIM-1 as the BW-1 monitor sees it (docs/p5k-bw1-monitor/BW1_MONITOR.md, "The KIM it runs
// on"), around Mos6502Core: one bus cycle a microsecond; a Bw1Model is not the netlist (P5l).
class Bw1Model {
public:
    explicit Bw1Model(std::uint64_t seed);  // RAMs random from `seed`; powered on and reset
    void run(std::uint64_t us);
    void reset();                           // RS: RES on the 6502 and both 6530s for 8 cycles

    // Keys by GETKEY code (0x00-0x14); one at a time. press(): held `hold`, then released `gap`.
    void hold(int key);
    void release();
    void press(int key, std::uint64_t holdUs = 40'000, std::uint64_t gapUs = 40'000);
    void type(std::string_view keys);       // "AD 0200 DA A9 + 42 GO": names and hex digits
    void stop(std::uint64_t holdUs = 20'000);  // ST: NMI low while held
    void setSst(bool on);
    void setStrap(bool closed);

    // The six digits over the next `us`: a digit selected under 1/20 of it is ' '; a segment
    // lit for at least half of its digit's time is on. Characters: 0-9 A-F, 'P', '-', ' ', '?'
    // (S and 5 are the same segments: "PASS" reads "PA55").
    std::string display(std::uint64_t us = 20'000);

    std::uint8_t peek(std::uint16_t address) const;  // as the CPU would read it, no side effect
    void poke(std::uint16_t address, std::uint8_t value);  // RAM only
    void stick(std::uint16_t address, unsigned bit, bool one);  // main or RIOT RAM
    void patchRom(std::uint16_t address, std::span<const std::uint8_t> bytes);  // ROM, in place
    const Mos6502Core& core() const { return core_; }
    bool timerTouched() const { return timerTouched_; }
};
```

- **ROMs**: `ReadRomContents(BW_ROM_DIR "/bw1_002.rom", 1024, 8)` and `bw1_003.rom`; `REQUIRE`d.
- **Decode** (§ "The KIM it runs on"): `block = (address >> 10) & 7`; 0 RAM (`address & $3FF`); 5: `a96 = (address >> 6) & $F`: `1111` 002 RAM, `1110` 003 RAM (`address & $3F`), `1101` 002 I/O, `1100` 003 I/O; 6 the 003 ROM, 7 the 002 ROM; anything else reads the last byte on the bus (bus charge) and ignores writes.
- **6530 I/O**: `A2` low: the four registers by `A1 A0`, a read of a data register giving the register's bit for outputs and the pin for inputs; `A2` high: the timer, `timerTouched_ = true`, reads 0. `RES` clears the four registers of each chip.
- **Pins** of the 002: `PB` pin = direction ? data : 1; U24's input `n = (PB >> 1) & $F`. `PA` pin `i` = direction ? data : 1, then 0 when a held key's row `r` (key / 7) is `n` and its column (6 − key % 7) is `i`, or when the strap is closed, `n` = 3 and `i` = 0. A key pulls a pin driven 1 low too (the 6530's 1 is its pull-up).
- **Display**: digit `n − 4` while `n` is 4–9, segments the `PA` pins 0–6 high. Kept as the current state and the cycle it began; each change (a port write, a key, the strap) adds the elapsed cycles to `digitOn[6]` and `segmentOn[6][7]`. `display()` clears them, runs, flushes, decodes: the segment patterns of `TABLE` (`& $7F`), `P` `$73`, `-` `$40`, blank `$00`; else `?`.
- **NMI** before each cycle: `stHeld_ || (sst_ && bus.sync && block(bus.address) != 7)` (KIM Fig. 3.3: SST joins NMI to NAND(SYNC, K7)). `core_.setNmi()`, `core_.setIrq(false)` (nothing drives `IRQ` on the BW-1's monitor).
- **Stuck bits**: per RAM byte a mask and a value; a write stores `(data & ~mask) | value`, and `stick` forces the byte at once, as `RamBlock`.
- **Power-on**: RAM bytes from `std::mt19937_64(seed)`, `core_.start` with random registers, then `reset()` (the KIM's POR holds `RES`).
- **Keys by name** in `type`: `AD`, `DA`, `+`, `GO`, `PC`, hex digits one a key; blanks separate.

`run` is one loop: `bus = core_.bus()`, NMI, read or write, `core_.step`, `++cycle_`. No allocation in it.

`tests/machines/bw1_model_test.cpp`: the model alone, on a small program assembled in the test with `Assemble6502` and loaded by `patchRom` over the 002's ROM (its words, with the vectors): a digit lit by `SBD`/`SAD` reads back in `display()`; a key held reads its column low only while its row is selected; the strap only on row 3, `PA0`; SST's NMI only outside `$1C00`–`$1FFF`; a stuck bit; the bus charge; the timer flag.

## 5. Tests — `tests/machines/bw1_monitor_test.cpp`, `bw1_selftest_test.cpp` (new)

Tag `[machines]`. Case names begin `bw1 model: `, `bw1 monitor: `, `bw1 self-test: ` (ctest names come from them: the plan's `-R` filters). Program bytes typed through the keypad (`type`), as a player would.

### 5.1 Monitor (seeds 1–3 where the RAM's contents matter)

- reset: `display()` is `0200` plus the byte at `$0200` (`peek`); `MODE` = 1, `NMIV` and `IRQV` = `$1C00`, `SPUSER` = `$FF`.
- `type("AD 0300 DA A9")`: `0300A9`, `peek($0300)` = `$A9`; `+` → `0301`; `AD 1234` → `1234`; five hex digits keep the last four.
- a program, `AD 0200 DA A9 + 42 + 85 + 10 + 00 AD 0200 GO` (`LDA #$42`, `STA $10`, `BRK`): `peek($10)` = `$42`, the display `0206xx` (the `BRK`'s address + 2), `PCL`/`PCH` = `$0206`; `PC` after moving away shows `0206` again.
- saved registers: `$5A` typed at `$F3`, then `STA $10`, `BRK` at `$0200` and GO: `peek($10)` = `$5A`.
- ST: `JMP $0200` (`4C 00 02`), GO, `stop()`: `02004C`.
- SST: `LDA #$11`, `LDA #$22`, `LDA #$33` at `$0200`; SST on, GO: `0202A9`, `ACC` = `$11`; GO: `0204A9`, `$22`; GO: `0206` and the byte at `$0206` (`peek`), `$33`.
- KIM entry points: at `$0200` `20 1F 1F` (`JSR SCANDS`), `D0 FB` (`BNE $0200`: wait for no key, GO is still held), `20 1F 1F`, `F0 FB` (`BEQ $0205`: wait for a key), `20 6A 1F` (`JSR GETKEY`), `85 00` (`STA $00`), `00` (`BRK`); GO; `press(7)`: `peek($00)` = 7, display `0211xx`.
- a held key read once: `AD 0300 DA`, `hold(5)` for 300 ms: the byte at `$0300` is `$x5` with the old low nibble shifted once, not more.
- the fixed addresses' opcodes (§2.2).
- bad RAM (no timer check: the CPU runs random bytes): `stick($01FF, b, 0)` for every bit (the `JSR`'s return high byte reads `$00`): after reset `display()` is not `0200xx` in 200 ms; `stick($FB, 1, 0)` (`POINTH` bit 1): `0000xx`.
- `timerTouched()` false after each other case (the model flags a timer access; it does not refuse it).

### 5.2 Self-test

- strap closed, reset: `display(5ms)` after 2 ms is `1` and five blanks; at 300 ms `888888`; at 1.0 s `8` and five blanks; at 3.2 s `PA55` and two blanks; `hold(0x13)`: `PA5513`. Seeds 1–3.
- `AD 1800 GO` with the strap open: the same `PA55` by 3.5 s after GO.
- failures, strap closed: `stick($0234, 3, …)` (the bit opposite to the first pattern's: `$34 ^ $55 = $61`, bit 3 is 0, so stuck at 1) → `123408`; `stick($17D5, 0, …)` → `27D501`; `stick($1795, …)` on bits 7, 6, 5, 4 at 0, 1, 0, 0 (the inverse of the high nibble of pass A's `$15 ^ $A5 = $B0`) → `3795F0`; `patchRom($1D00, …)` changed by `+1` → `4C00` and `01`, by `$10` → `10`; `patchRom($1BF0, …)` (past the code: unwritten `$FF`) → `5800xx`.
- dead RAM: every main and RIOT RAM byte stuck at `$FF`: `1000FF` (page 0, offset 0, pattern 0).
- strap open, reset: the monitor (`0200xx`), not the self-test.

`tests/CMakeLists.txt`: `machines/bw1_model.cpp`, `machines/bw1_model_test.cpp`, `machines/bw1_monitor_test.cpp`, `machines/bw1_selftest_test.cpp`. `BW_ROM_DIR` is already defined for `bw_tests`.

## 6. Docs

- [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2, a program: a ROM with entry points at fixed addresses uses one `.org` each (the assembler's "written twice" catches code running into the next) and a test of the opcodes there; `.checksum` for a ROM a self-test sums; a program can be tested on `Mos6502Core` with the machine around it modelled (`tests/machines/bw1_model`) before its netlist exists.
- [GLOSSARY.md](../GLOSSARY.md): **Monitor** (line 82) checked against this use (a ROM program that reads keys and shows memory); **Strap**: a jumper or wire that a program reads to choose what it does (the BW-1's self-test).
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P5k row done, with links.

## 7. Release check

No Part, kernel or UI change. The release build passes the `[machines]` cases above and the asm tests; `bw_ui` of the 110 starts (`--screenshot`, viewed) as the shared-code check. The full debug suite at the end (the assembler is shared code).
