# P5s — Z80 Part — Design

Spec: [Z80.md](Z80.md). Plan: [Z80_PLAN.md](Z80_PLAN.md). Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (the CPU kind: core apart from the Pins, conformance data; sampled inputs; the pulse rule), and P5c's [MOS6502_DESIGN.md](../p5c-6502/MOS6502_DESIGN.md) for the shape of the core, the data folder and the runner, followed here unless said otherwise.

Sources: Zilog Z80 CPU User Manual UM0080 (rev. 11, 2016): pinout and signal descriptions, the timing figures (Instruction Opcode Fetch, Memory Read or Write, Input or Output, Interrupt Request/Acknowledge, Non-Maskable Interrupt, Bus Request/Acknowledge, Halt Acknowledge, Reset), the T-states of each instruction (Instruction Set chapter), and the Z80A AC characteristics (the stub's `kZ80…` source). Undocumented behaviour: Sean Young, "The Undocumented Z80 Documented" v0.91 (2005): flags X/Y, `DD`/`FD`/`ED` behaviour, R, interrupts, IM 0, reset values, the `LD A,I` P/V bug; MEMPTR: "MEMPTR, esoteric register of the Zilog Z80 CPU" (boo_boo, Vladimir Kladov, 2006); Q and `SCF`/`CCF`: Patrik Rak's 2012 findings (as Harte's `q`); block-instruction flags on repeat: David Banks and Peter Helcmanovsky's 2018 findings (as in ares). Opcode decoding by fields x/y/z/p/q: Cristian Dinu, "Decoding Z80 Opcodes". Where these disagree with Tom Harte's data, the data wins and the source is noted in the code.

## Files

| File | Change |
|---|---|
| `src/parts/z80_core.{hpp,cpp}` | `Z80Core`: the instruction core, no simulator includes |
| `src/parts/z80.{hpp,cpp}` | `class Z80 final : public Component`: the core behind the Pins |
| `src/parts/delays.hpp` | the stub's `kZ80…` comment widened to both Parts; `kZ80Busak`, `kZ80Float` added (Pins) |
| `src/parts/CMakeLists.txt` | `z80.cpp`, `z80_core.cpp` (sorted, before `z80_stub.cpp`) |
| `src/parts/catalogue.cpp` | `PartInfo{"Z80", "8-bit microprocessor (NMOS)", "DIP-40", Chip, &CreateChip<Z80, "Z80">}` just before `Z80STUB` |
| `datasheets/Z80.md` | Function (plain words: M-cycles and T-states, M1 and refresh, wait, the interrupts and their three modes, NMI, HALT, bus request, reset, the random power-on state), Pins (`A0`–`A9`, `A10`–`A15`, `D0`–`D7`, every other name) |
| `tests/parts/catalogue_test.cpp` | `Z80` in `kMosSignals` and `kOffCornerPower` (`VCC` pin 11, `GND` pin 29, as `Z80STUB`) |
| `tests/parts/z80_core_test.cpp` | hand-written core cases (Tests) |
| `tests/parts/z80_cases.{hpp,cpp}` | reads the compact case files, expands M-cycles to Harte's T-states, runs one case on a core, reports the first difference |
| `tests/parts/z80_conformance_test.cpp` | Harte subset, full Harte, ZEXDOC, ZEXALL |
| `tests/parts/z80_test.cpp` | the Part at its Pins |
| `tests/CMakeLists.txt` | the five test sources (sorted, before `parts/z80_stub_test.cpp`); `BW_Z80_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data/z80"`, `BW_Z80_EXTERNAL_DIR="${PROJECT_SOURCE_DIR}/external/z80"` |
| `tests/data/z80/harte/*.txt` | the committed subset: the first 25 cases of each of the 1,604 files of `SingleStepTests/z80`, folder `v1`, a file's spaces written as `_` (`dd cb __ 06.json` → `dd_cb____06.txt`); 20 MB |
| `tests/data/z80/README.md`, `LICENSE-harte` | source repository and commit, the format, how to refresh; Harte's MIT licence |
| `tests/data/z80/convert_harte.py` | JSON → compact format (`--cases N`), standard library only (`python3`; the worktree has no `venv`) |
| `tests/data/z80/fetch.sh` | full Harte set (1,604 files, 1.37 GB, each fetched, converted into `external/z80/harte/`, deleted; `--subset` also rewrites the committed one) and `zexdoc.com`, `zexall.com` from `agn453/ZEXALL` into `external/z80/zex/`; commit hashes into `external/z80/SOURCES` |
| `netlists/bus_load_z80.bwn` | Bus load |
| `roms/make_bus_load_z80_rom.py`, `roms/bus_load_z80.rom` | Bus load |
| `tests/machines/bus_load_test.cpp` | both netlists (Bus load) |
| `docs/recipes/ADD_PART.md` | reference commits; the M-cycle core variant of the CPU kind |

`.gitignore` already has `external/`.

## Instruction core

### Interface

```cpp
struct Z80Registers {
    std::uint16_t pc, sp, ix, iy, wz;                  // wz: MEMPTR
    std::uint8_t a, f, b, c, d, e, h, l, i, r;
    std::uint16_t af2, bc2, de2, hl2;                  // the alternate set
    std::uint8_t im, q;    // q: F if the last instruction set the flags, else 0
    bool iff1, iff2;
    bool ei;               // the last instruction was EI (no interrupt after it)
    bool p;                // the last instruction was LD A,I or LD A,R (the NMOS P/V loss)
};

enum class Z80CycleKind : std::uint8_t {
    Fetch,        // M1: opcode read, then refresh at I:R (4 T-states)
    Read, Write,  // memory (3)
    In, Out,      // I/O, one automatic wait included (4)
    Acknowledge,  // INT acknowledge: M1 with IORQ, two automatic waits included (6)
    Internal      // `length` T-states with nothing on the bus (the address held)
};
constexpr int Z80CycleLength(Z80CycleKind kind, int internalLength);  // waits excluded

struct Z80Bus {
    Z80CycleKind kind;
    std::uint16_t address;  // Internal: unchanged from the cycle before
    std::uint8_t data;      // Write, Out
    std::uint16_t refresh;  // Fetch, Acknowledge: I:R as driven in T3 and T4 (R before its count)
    std::uint8_t length;    // Internal
    bool halted;            // Fetch: a halted CPU's NOP (data ignored, PC kept)
};

class Z80Core {
public:
    void start(const Z80Registers& registers);  // next cycle: a fetch at registers.pc
    void reset();                     // the reset values; next cycle: a fetch at $0000
    const Z80Bus& bus() const;        // the M-cycle in progress
    void step(std::uint8_t data);     // ends it; `data` read (meaningful on Fetch, Read, In,
                                      // Acknowledge)
    Z80Registers registers() const;   // the live values; between instructions, Harte's `final`
    bool halted() const;
    bool instructionEnded() const;    // the last step ended an instruction or interrupt sequence

    void setInt(bool asserted);       // level, looked at only when an instruction ends
    void setNmi(bool asserted);       // latches on the deasserted → asserted change
};
```

The decode (an `Instruction` table for P5t's assembler, as `Mos6502Decode`) is not part of this step. Bus request and reset are the Part's alone (Pins): the core neither sees nor needs them; `reset()` is called on release.

### M-cycles and T-states

The core works one M-cycle at a time, which is what the instruction tables count ("4, 3, 3"). The pin waveform within an M-cycle is fixed by its kind; the Part sequences it per T-state and adds the wait states. Internal T-states (the fifth and sixth of a long M1, `JR`'s five, the five after a repeating `LDIR`, the indexed displacement's five) are their own `Internal` cycles with the address held, so a data-dependent one (a repeating `CPIR`) is decided after the read it depends on. Harte's data shows the held address: the refresh address after a long M1, the written address after `LDIR`'s write, the operand address after `DD CB d op`'s opcode read. Example M-cycle lists (UM0080, Instruction Set; T-states per cycle):

| Instruction | M-cycles |
|---|---|
| `NOP` | Fetch |
| `LD r,n` | Fetch; Read PC |
| `INC rr` | Fetch; Internal 2 |
| `PUSH rr` | Fetch; Internal 1; Write SP−1; Write SP−2 |
| `JR e` (taken) | Fetch; Read PC; Internal 5 |
| `EX (SP),HL` | Fetch; Read SP; Read SP+1; Internal 1; Write SP+1; Write SP; Internal 2 |
| `LDIR` (repeating) | Fetch; Fetch; Read HL; Write DE; Internal 2; Internal 5 (PC − 2) |
| `LD r,(IX+d)` | Fetch; Fetch; Read PC; Internal 5; Read IX+d |
| `DD CB d op` | Fetch; Fetch; Read PC (d); Read PC (op: a read, not an M1, R not counted); Internal 2; Read IX+d; Internal 1; Write IX+d |
| `IN A,(n)` | Fetch; Read PC; In A:n |
| `HALT` | Fetch; then halted Fetches at PC until an interrupt |

The core is a resumable function (`run()`): each instruction is straight-line code, and each M-cycle sets `bus_`, records where to resume and returns; `step()` resumes there with the byte. The resume points are case labels of one switch numbered by `__COUNTER__` (a macro, `Z80_CYCLE(setup)`); decoding by the x/y/z/p/q fields jumps with `goto`, and no resume point sits inside a nested switch. No allocation, no virtual calls. Prefixes `CB`, `ED`, `DD`, `FD` are their own Fetches; a `DD`/`FD` before an opcode that has no index form is a 4-T-state prefix, then the opcode runs as unprefixed (Harte's `dd 00`); `DD DD` and `DD FD` keep the last; `DD ED` ignores the `DD`. A `DD`/`FD` sets Q to 0 (Harte's `dd 37`). The `ED` holes are two Fetches and nothing else (8 T-states).

### Flags and undocumented state

- X (bit 3) and Y (bit 5) as Harte's data has them: from the result for ALU operations, from the operand for `CP`; for `LDI`/`LDD` X is bit 3 and Y bit 1 of A + value, for `CPI`/`CPD` of A − value − H; from MEMPTR's high byte for `BIT n,(HL)`, from the address's for `BIT n,(IX+d)`; `SCF`/`CCF` from `((Q ^ F) | A)`. Q = F after an instruction that set flags, else 0.
- WZ (MEMPTR) at each instruction that sets it (Kladov's table): `LD A,(nn)`, `LD (nn),A`, `LD A,(rr)`, `LD (rr),A`, `JP`, `CALL`, `RET`, `RST`, `JR`, `DJNZ`, `IN`/`OUT`, `ADD`/`ADC`/`SBC HL,rr`, `(IX+d)`, block instructions, `RLD`/`RRD`, `EX (SP),HL`, `LD rr,(nn)`/`LD (nn),rr`, and the interrupts.
- Repeated block instructions (the 2018 rules): on a repeat PC goes back by 2, WZ = PC + 1, X and Y come from PC's bits 11 and 13; the I/O ones' H and P/V also take the extra adjust. Harte's data checks each.
- R: bits 0–6 + 1 per Fetch (prefixes included, the `DD CB` opcode read excluded, halted fetches and the acknowledge included); bit 7 only by `LD R,A`. The refresh address shows R before the count (Harte's `00`: R `10`, refresh `a610`, then R `11`).
- `p` set by `LD A,I`/`LD A,R`; if an interrupt (INT or NMI) is accepted at the end of that instruction, its P/V becomes 0 (NMOS).

### Interrupts and HALT

- When an instruction ends (`step()` of its last cycle), the core takes, in order: NMI if latched; INT if `setInt(true)` was given before this step, IFF1 set and the instruction was not `EI`. Otherwise the next instruction's Fetch. A prefix's Fetch never ends an instruction.
- NMI: a Fetch at PC (data ignored), Internal 1, Write SP−1 (PCH), Write SP−2 (PCL), PC = `$0066` (11 T-states); IFF1 = 0, IFF2 kept. `RETN` (and its `ED` mirrors, `RETI` too) IFF1 = IFF2.
- INT: IFF1 = IFF2 = 0, then the Acknowledge (6 T-states, refresh, R + 1). IM 1: Internal 1, two Writes, PC = `$0038` (13). IM 2: Internal 1, two Writes, Read `I:byte` (the byte read during the Acknowledge, bit 0 kept), Read `I:byte+1`, PC = the word (19). IM 0: the byte read is an opcode, decoded as if fetched, with no PC count for it or its operand Reads at PC (Young, §6.2: the device supplies them; on a floating bus they are whatever it reads); `RST p` gives 13 T-states (Acknowledge, Internal 1, two Writes), `CALL nn` 19.
- `HALT`: PC is past it (Harte); then each cycle is a Fetch at PC marked `halted`, the opcode ignored, R counted, PC not counted, and each ends an "instruction" (a NOP), so interrupts are looked at after each. An accepted interrupt pushes that PC.

## Pins

Index = pin − 1, as `Z80Stub` (the arrays `kAddress`, `kData` and the control indices copied). Kinds: `A0`–`A15`, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N` outputs, tri-state (they drive `Z` while the bus is granted, and A also during reset; UM0080 marks only these and D as tri-state); `M1_N`, `RFSH_N`, `HALT_N`, `BUSAK_N` outputs; `D0`–`D7` bidirectional, sampled; `INT_N`, `NMI_N`, `WAIT_N`, `BUSRQ_N`, `RESET_N` inputs, sampled; `CLK` input on both edges, not sampled; `VCC`, `GND` power. Signal pins `Family::Mos`, power pins `Family::Ttl` (as the 6502; `catalogue_test` requires it).

### The T-state sequencer

The Part keeps the current `Z80Bus`, the T-state within it (T1, T2, the waits, T3, T4; 1…n for an Internal), the data latched, and the inputs sampled. All edges below are `CLK` edges; every input is read with `readBitBefore` (its level before the edge). Sources: UM0080 timing figures; the strobe edges are those of `z80_stub.cpp`.

| Cycle | Rising edges | Falling edges |
|---|---|---|
| Fetch | T1: address, `M1_N` low, `RFSH_N` high; T3: data latched, `MREQ_N`, `RD_N`, `M1_N` high, refresh address, `RFSH_N` low | T1: `MREQ_N`, `RD_N` low; T2, Tw: `WAIT_N` read; T3: `MREQ_N` low; T4: `MREQ_N` high (`HALT_N` low after a HALT's) |
| Read | T1: address | T1: `MREQ_N`, `RD_N` low; T2, Tw: `WAIT_N`; T3: data latched, `MREQ_N`, `RD_N` high |
| Write | T1: address | T1: `MREQ_N` low, data driven; T2: `WR_N` low, `WAIT_N`; Tw: `WAIT_N`; T3: `MREQ_N`, `WR_N` high |
| In | T1: address; T2: `IORQ_N`, `RD_N` low | Tw (automatic, then more while `WAIT_N`): `WAIT_N`; T3: data latched, `IORQ_N`, `RD_N` high |
| Out | T1: address; T2: `IORQ_N`, `WR_N` low | T1: data driven; Tw: `WAIT_N`; T3: `IORQ_N`, `WR_N` high |
| Acknowledge | T1: address (PC), `M1_N` low, `RFSH_N` high; T3: data latched, `M1_N`, `IORQ_N` high, refresh, `RFSH_N` low | Tw1: `IORQ_N` low; Tw2 (and more while `WAIT_N`): `WAIT_N`; T3: `MREQ_N` low; T4: `MREQ_N` high |
| Internal | T1 after a Fetch or Acknowledge: `RFSH_N` high | — |

- **Inputs and `step()`.** At the rising edge of a cycle's last T-state (T4 of a Fetch or Acknowledge, T3 of a Read, Write, In or Out, the last of an Internal) the Part reads `INT_N` and `BUSRQ_N` and calls `setInt`. It calls `step(latched data)` at that same edge for a Fetch, Acknowledge and Internal, and at T3's falling edge for Read, Write, In and Out (their data edge comes after the last rising edge). So the INT the core uses is always the level at the last T-state's rising edge (UM0080: "the CPU samples the interrupt signal on the rising edge of the last clock of each instruction"); on the bus load, at a Fetch's T4 the address is the refresh address, so `INT_N` on `A6` reads R bit 6. The next cycle's pins start at the following rising edge (its T1).
- **NMI.** `NMI_N` is read at every rising edge and `setNmi` called each time, so a pulse that falls and rises within an instruction is latched (an edge seen at CLK resolution).
- **Data** driven by a Write or Out is released at the next rising edge (`kZ80Data`), as the stub.
- **WAIT.** At T2's falling edge (Tw's for I/O and the Acknowledge, after the automatic ones) `WAIT_N` low adds a Tw, read again at that Tw's falling edge.
- **Bus grant.** `BUSRQ_N` low at a cycle's last rising edge: from the next rising edge `BUSAK_N` low (`kZ80Busak`), A, D, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N` `Z` (`kZ80Float`); `M1_N`, `RFSH_N`, `HALT_N` stay driven (high); `BUSRQ_N` read at each rising edge; seen high: `BUSAK_N` high at that edge, and the core's next cycle starts at the following one. No refresh while granted (UM0080).
- **Reset.** `RESET_N` is read at every rising edge. Low: the cycle is abandoned, A and D `Z`, the control outputs high (`MREQ_N`, `IORQ_N`, `RD_N`, `WR_N`, `M1_N`, `RFSH_N`, `HALT_N`, `BUSAK_N`; UM0080: "the address and data bus go to a high-impedance state, and all control output signals go to the inactive state"); a bus grant ends. The first rising edge that sees it high calls `core.reset()`, and the next rising edge is T1 of the fetch at `$0000` (the count between is checked against UM0080's Reset figure in phase 4 and written in the plan's notes).
- **X** on a data pin reads as a random bit (`readBitBefore`, MOS).
- **Power-on** (`powerOn`): random registers (`pc` and every byte drawn, `iff1`/`iff2`, `im` 0–2), the core started at a Fetch at PC; the outputs as before a T1: address = PC, the strobes high, data released.
- **`HALT_N`** low from the HALT M1's T4 falling edge (UM0080, Halt Acknowledge) until the cycle after the last halted Fetch starts (the T1 rising edge of the interrupt's first cycle, or of reset's end).

Delays (`delays.hpp`, Z80A maxima, the existing figures reused): address `kZ80Address` 110 ns; `MREQ_N` `kZ80Mreq` 85; `RD_N` `kZ80Rd` 85; `WR_N` `kZ80Wr` 80; `IORQ_N` `kZ80Iorq` 75; `M1_N` `kZ80M1` 100; `RFSH_N` `kZ80Rfsh` 130; `HALT_N` `kZ80Halt` 300; data `kZ80Data` 150; new `kZ80Busak` (tDL(BA)/tDH(BA)) and `kZ80Float` (tF), read from the same AC table in phase 4 and commented.

The pulse rule (ADD_PART: drives are inertial, a second drive on a pin scheduled before the first applies replaces it) is checked per pin at 3.25 MHz (a T-state 307.7 ns, half of it 153.8 ns): `MREQ_N` up at T3↑ and down at T3↓, 154 ns apart against 85 ns; data released at T1↑ and driven again at T1↓, 154 ns apart against 150 ns (a 4 ns margin; at 4 MHz, 125 ns against 150, so a write right after a write would lose its release, which only changes nothing: the same pins are driven again); `RFSH_N`, `M1_N` a cycle or more apart. `HALT_N` low at a HALT's T4↓ (+300 ns) and high at the next T1↑ (154 ns later) when an interrupt is taken at the end of the HALT itself: the two cancel and `HALT_N` stays high, which is right (no halted fetch happened).

## Test data

### Format (`tests/data/z80/README.md`)

As `tests/data/6502` (comment lines, then one case in four or five lines), every number hex:

```
# <case name>
I <pc> <sp> <a> <f> <b> <c> <d> <e> <h> <l> <i> <r> <ix> <iy> <wz> <af'> <bc'> <de'> <hl'> <im> <iff1> <iff2> <ei> <p> <q> <address>:<value> …
F (the same fields)
C <address>:<data>:<flags> …    one per T-state; `-` for a null address or data; flags rwmi or `-`
P <port>:<value>:<r|w> …        only when the case has ports
```

### Runner (`z80_cases`)

`ReadZ80Cases(path)` → `std::expected<std::vector<Z80Case>, std::string>` (file and line on a bad line). `RunZ80Case(c)` → `std::optional<std::string>` (the first difference): a 64 KiB array with the initial RAM (reused, only touched cells cleared), the ports in order (`r` entries give the byte an `In` reads, `w` entries the byte an `Out` must write), `start(initial)`, then M-cycle by M-cycle until the listed T-states are used: each M-cycle expanded into Harte's sampling and compared with the next listed T-states, then `instructionEnded()`, `registers()` (every field, WZ, Q, P, IFF1/2, IM and `ei` included) and the final RAM. Harte's sampling, confirmed on the converted files (`00`, `d3`, `db`, `ed_b0`):

| Cycle | T-states |
|---|---|
| Fetch | `pc - ----`, `pc - r-m-`, `refresh opcode ----`, `refresh - ----` |
| Read | `a - ----`, `a - r-m-`, `a value ----` |
| Write | `a - ----`, `a value -wm-`, `a - ----` |
| In | `a - ----`, `a - ----`, `a - r--i`, `a value ----` |
| Out | `a - ----`, `a - ----`, `a value -w-i`, `a - ----` |
| Internal | `held - ----` each (the refresh address after a Fetch, else the last address) |

The expansion lives in the runner only; the Part has its own sequencer, checked by its own tests. `ei`, `p` and `q` are random in `initial` and checked in `final`.

### ZEXDOC and ZEXALL

`external/z80/zex/zexdoc.com` (and `zexall.com`) loaded at `$0100`; `$0005` holds a `RET` (`$C9`) and `$0006`–`$0007` `$F000` (the top of memory the program reads for its stack). The core starts at `$0100`; at each Fetch at `$0005` the runner does the BDOS call (C = 2: print E; C = 9: print from DE to `$`) into a string, then the `RET` runs on the core; a Fetch at `$0000` (the warm boot) ends the run. Pass: the output has no `ERROR` and has `Tests complete`. The output is printed on failure. Cap: 100 G T-states.

## Bus load

### Netlist (`netlists/bus_load_z80.bwn`)

A copy of `bus_load.bwn` with these changes, the header comment rewritten to say what differs and why:

- `part U2  Z80`.
- `INT_N` on the `A6` Net (`net A6 … U2.INT_N`), as the ZX80: the interrupt asks while A6 is low, and the CPU samples it at the last T-state of each instruction, a Fetch's T4 for every instruction the display runs, when the refresh address `I:R` is on the bus; so R bit 6 chooses the line's end.
- `part U27 POR time=1ms`, `part R1 R value=3k3` from `RESET_N` to `VCC`: `net RESET_N U2.RESET_N U27.RST_N R1.2`, `net VCC R1.1` (as `bw1.bwn`'s `RES_N`).
- `U3 2716 contents=../roms/bus_load_z80.rom`.
- `NMI_N`, `WAIT_N`, `BUSRQ_N` stay on `VCC`.

### ROM (`roms/make_bus_load_z80_rom.py`)

Writes `roms/bus_load_z80.rom` (the `.rom` text format of P4l), each instruction on its own line with its mnemonic and T-states. Layout: code at `$0000`–`$002E` and the IM 1 handler at `$0038`–`$0048`; the RAM image at `$0100`–`$0425` (6 `HALT`s, `Z80Stub::Page()`'s 792 bytes rebuilt in Python from the same title and rule, 8 `HALT`s); the P5a glyphs at `$0600`–`$07FF` (imported from `make_bus_load_rom.py`). The script checks the budget below every time it runs (both handler paths, the frame total, the R window, the line counter); `--check` also compares the file on disk.

The program (ZX80-style, the same circuit as the stub; as implemented in phase 5):

1. Reset: `DI`, SP = `$4400` (RAM 1K at `$4000`), `IM 1`, I = `$0E` (the refresh address's A9, A10 high: the glyphs at `$0600`, as the stub's `kRefreshPage`), `LDIR` the image to `$4000` (806 bytes, about 5 ms).
2. Frame: `IN A,($FE)` (VSYNC on), a delay (`DJNZ` × 120 and 18 T-states of padding), `OUT ($FE),A` (VSYNC off; about 7.7 lines; no interrupt, so no HSYNC), HL = `$C000`, B = 38 rows, C = 8 lines, A = `$5D`, `CALL tail`; the handler's `RET Z` on the last row comes back here, `JR frame`.
3. 38 rows of 8 lines, each row entered by `JP (HL)` at its first byte in the image's mirror (`$C000`, A15 high): 6 top-border rows (a lone `HALT` each; 48 lines, so the line counter is 0 on row 0's first line), the 24 display rows (32 characters fetched with D6 low, so the hardware forces NOPs and shows them; the NEWLINE `$76` runs as `HALT`), 8 bottom-border rows. A `HALT` leaves PC at the byte after it, the next row's start, which the interrupt pushes.
4. **The tail** `LD R,A`, `EI`, `JP (HL)`, with A = `$5D`: the k-th Fetch after `EI` shows R's 7 bits as `$5D + k`, A6 high for k = 1…34 and low (`$00`) at k = 35: on a display line `JP (HL)`, 32 characters, the `HALT`, one halted Fetch; on a border line `JP (HL)`, the `HALT`, 33 halted Fetches. Both are 35 Fetches (140 T-states).
5. **The handler** (`$0038`), on the ZX80's pattern: `DEC C`, `JP NZ,same`; the next row: `POP HL` (the pushed PC), `DEC B`, `RET Z` (the frame done, 5 T-states when not taken), `SET 3,C`; `same`: `POP DE` (dropped), `RET Z` (never taken: 5 T-states of padding), `JR tail`. Both paths take 54 T-states to `JP (HL)`, so a line is 13 (acknowledge) + 54 + 140 = 207, the stub's line, and the first character always the same time after HSYNC (71 T-states, the stub's 59: the page sits 3.7 µs further right).
6. The frame: 303 lines of 207 and the line across VSYNC of 1,863 (9 × 207): 312 × 207 T-states, the stub's frame.

The RAM holds the image (`$4000`–`$4325`) and the stack at `$43FE`–`$43FF`.

### Test (`tests/machines/bus_load_test.cpp`)

`Load()` takes a `BusLoad` (netlist, ROM, time cap); each `TEST_CASE` `GENERATE`s over `bus_load.bwn` and `bus_load_z80.bwn`. `ExpectedPage()` takes the netlist's ROM, so the test checks the glyphs the circuit uses. The page check and frame stats stay as they are (lines 310–312, 90 % synced). The cap to 3 locked frames is 100 ms for the stub and 250 ms for the Z80: its first VSYNC comes after the POR and the copy (6 ms), and the TV, free-running at 21 ms, accepts a vertical sync only 16–21 ms into its own frame, so it falls in step after about 6 frames. One more case for the Z80 netlist: after 10 ms the RAM from `$4006` holds `Z80Stub::Page()`, read with `Ram2114::word(address)` of U4 (the low nibble) and U5 (the high nibble).

These cases are `[slow]`, which does not keep them out of the debug suite (only `[.]` does): the TV case takes 80 s in debug for both netlists and both seeds.

### Measurements (Results)

Release, headless, `bw run --time 2s`, best of 3, the two netlists alternated: ratio, applied events, evaluations, per simulated µs; `bw_ui netlists/bus_load_z80.bwn --screenshot … --after 2s` with the TV open: ratio, UI ms, the page viewed. Recorded in the plan's notes and in ARCHITECTURE (Performance, P5s row).

## Tests

Test names: `"Z80 core: …"`, `"Z80: …"`.

`tests/parts/z80_core_test.cpp` (`[parts]`), each checked to fail on a mutation of its rule (the mutations listed in the plan's notes):

- NOP: one Fetch, refresh `I:R`, R + 1, bit 7 kept; `DD`, `ED`, `CB` prefixes count R; `DD CB d op` counts two.
- Prefix chains Harte lacks: `DD DD 21 nn nn` and `DD FD 21 nn nn` load the last prefix's register (R + 3); `DD ED 4B nn nn` (`LD BC,(nn)`, the `DD` ignored, 24 T-states).
- `ED` holes Harte lacks: one from each of `00`–`3F`, `80`–`9F` (not block), `A4`–`A7`, `C0`–`FF`: two Fetches, 8 T-states, nothing changes but R and PC.
- Reset: from mid-instruction; then a Fetch at `$0000`, the reset values.
- HALT: Fetches at the next PC, `halted`, R counted, PC kept; INT leaves it with that PC pushed.
- EI: INT asserted during `EI` is taken after the next instruction, not after `EI`; `DI` masks it.
- IM 1, IM 2 (vector `I:byte`, both Reads, the pushed PC), IM 0 with `RST 28h` on the bus, and with `CALL $1234` (Acknowledge byte `$CD`, then two Reads at PC giving `$34`, `$12`; PC not counted; 19 T-states).
- NMI: once per edge, held low once; a short pulse (asserted then released between two steps) still taken; over a pending INT; IFF1 cleared, IFF2 kept; `RETN` restores; taken in `EI`'s shadow.
- No interrupt between a prefix and its opcode (INT held during `DD 21 nn nn`: taken after it).
- `LDIR` with INT: taken between repeats, the pushed PC the `LDIR`'s.
- `LD A,I` then an accepted INT: P/V 0.

`tests/parts/z80_conformance_test.cpp`:

- `Z80 core: Harte subset` (`[parts]`): every file of `BW_Z80_DATA_DIR/harte`, `CHECK(files == 1604)`; the first failure of each file printed. One ctest test.
- `Z80 core: Harte full`, `Z80 core: ZEXDOC`, `Z80 core: ZEXALL` (`[.][slow]`, not registered by `catch_discover_tests`, run by name in release): `SKIP` when `BW_Z80_EXTERNAL_DIR` lacks the files.

`tests/parts/z80_test.cpp` (the 6502 bench shape: every Pin on its own Net, a `ClockSource` of 307.7 ns (3.25 MHz) on `CLK`, a `Stimulus` per input; a test-only `BenchMemory`, 64 KiB, woken by the address, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N`: drives `D` 150 ns after `RD_N` falls with `MREQ_N` low, releases it when `RD_N` rises, stores at `WR_N`'s rise; an `IORQ_N` read gives a fixed port byte (or `$FF` with `M1_N` low: the acknowledge), a write records it):

- `Z80: pins`: count, names, kinds, families, `sampled` on every input and data pin, `CLK` not.
- `Z80: M1 timing`: each edge of the Fetch table at its delay; the refresh address `I:R`.
- `Z80: read, write, in, out timing`: each table row at its delays; written data released at the next T1.
- `Z80: reads before the edge`: a data Net changed at the sampling edge itself is not seen; one changed before is.
- `Z80: runs a program`: reset, a short program (`LD`, `PUSH`/`POP`, `CALL`/`RET`, `LDIR`, `OUT`, a loop) leaves the expected bytes in `BenchMemory` and port writes.
- `Z80: WAIT`: low across T2 adds one Tw per falling edge seen low on a Read; on an `In` after the automatic one.
- `Z80: BUSRQ`: the bus floats and `BUSAK_N` falls after the M-cycle, `M1_N` and `RFSH_N` stay driven; released, the program goes on.
- `Z80: RESET_N`: low 3 clocks: A and D float, the control outputs high; the Fetch at `$0000` after release.
- `Z80: INT and NMI through the pins`: the IM 1 acknowledge on the pins at its delays (`M1_N`, then `IORQ_N` at Tw1↓, two Tw), the push, the fetch at `$0038`; NMI on its falling edge: the fetch at `$0066`.
- `Z80: HALT_N`: low from the HALT's T4 fall to the interrupt.
- `Z80: X on the data bus`: an opcode read as X runs (no X on any output).
- `Z80: wakes only on CLK`: the 6502's shape (`mos6502_test.cpp`, "wakes only on PHI0"): no memory, the data Nets tied to a NOP, only the non-data inputs toggled by `Stimulus`; the CPU's evaluations (total minus the clock's and the Stimuli's) equal the `CLK` edges.

Debug cost: the Harte subset is 40,100 cases, 20 MB (the 6502's 25,600 cases, 4.8 MB, take 17 s): about 60–90 s, one ctest test; measured at phase 2 and recorded. The full set takes 4.7 s in release (measured while the docs were reviewed).
