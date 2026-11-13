# P5d — ROM programs + first computer

Roadmap step P5d ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): ROMs written in assembly and built into `.rom` files by our own assembler; three new Parts for the first computer (address decoder, output latch, power-on reset); and the Machine they make, 110 LED computer ([LED_COMPUTER.md](../machines/110-led-computer/LED_COMPUTER.md)). Design: [ROM_PROGRAMS_DESIGN.md](ROM_PROGRAMS_DESIGN.md). Plan: [ROM_PROGRAMS_PLAN.md](ROM_PROGRAMS_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P5c gave a 6502 that runs on a bus of NOPs. A computer needs a program in ROM, RAM for its variables and stack, decoding that gives each chip its share of the address space, and a port to the outside. Every later computer (BW-1, BW-2001) has these, and their ROMs (a monitor, a screen editor) are too long to write in hex. The 110 is the smallest Machine where all of it has to work at once: if the ROM, the RAM, the decoder or the latch fails, the light stops or goes wrong.

## User decisions (2026-11-13)

| Question | Decision |
|---|---|
| Assembler | our own: `bw asm`, a small 6502 assembler in C++ sharing the core's opcode table. The `.rom` it writes is committed; a test checks each `.rom` matches its `.asm`. No outside tool |
| Decoder and latch | 74LS138 (3-to-8 decoder on `A15`–`A13`, 8K blocks, partial decoding with mirrors) and 74LS273 (octal D flip-flop, its clear on `RES_N`, so Reset turns the LEDs off) |
| Program | a bouncing light on 8 LEDs, about 8 steps a second; its position and direction in zero-page RAM, the delay a `JSR` subroutine, so the ROM, the RAM and the stack all have to work |
| Inputs | Reset only; keys come with P5g and P5h |
| Power-on | a power-on reset Part: a logic-only stand-in for the RC network and Schmitt trigger (GAME_DESIGN, analog sub-circuits), holding `RES_N` low for a set time after power-on, open collector so the Reset button still works. The BW-1, BW-2001 and BW-80 reuse it |

Decided while designing (2026-11-13), for the user to overrule:

- The power-on reset is `part U8 POR time=20ms`, in a DIP-8 package: `RST_N` pin 1 (open collector), `GND` pin 4, `VCC` pin 8 (the corners, as on a 74-series chip), the others `NC2`, `NC3`, `NC5`–`NC7`. 20 ms is short for a real RC (often 100 ms or more) but is invisible to a player and keeps tests short.
- Memory map of the 110: RAM (two 2114s, 1K) at `$0000`, mirrored in `$0000`–`$1FFF`; the LED latch at `$8000`, anywhere in `$8000`–`$9FFF`, write only; the ROM (2716, 2K) at `$F800`, mirrored in `$E000`–`$FFFF`. The other blocks are empty: a read there floats the bus.
- The write strobe from a 74LS02: `WR = NOR(PHI1, RW)` (high in the second half of a write cycle), `WR_N` for the RAMs' `WE_N`, and the latch clock rising at the end of a write to the latch block. Its four gates are all used.
- Assembler syntax as in [What is new](#what-is-new) 1, close to the common 6502 assemblers (ca65, 64tass) in what it shares with them; only the documented opcodes.

## What is new

1. **`bw asm`** — `bw asm <file.asm> -o <file.rom>`. A two-pass assembler for the 151 documented NMOS 6502 opcodes:
   - one statement a line; `;` starts a comment; a `label:` at the start of a line names the address; `@name:` is a local label, known only between two ordinary labels; `NAME = expr` defines a constant;
   - mnemonics in either case; operands: none, `A`, `#expr`, `expr`, `expr,X`, `expr,Y`, `(expr)`, `(expr,X)`, `(expr),Y`; an operand starting with `(` is indirect. The zero-page form is used when the value is known in the first pass and below `$100` and the instruction has one, otherwise the absolute form; branches take their target and check its range;
   - expressions: `$hex`, `%binary`, decimal, `'c'`, symbols, `*` (the statement's address); unary `-`, `~`, `<` (low byte), `>` (high byte); binary `*`, `/`, `+`, `-`, `<<`, `>>`, `&`, `^`, `|` with C's precedence; parentheses inside an expression;
   - directives: `.rom BASE, SIZE` (once, first: the CPU addresses the ROM chip answers at and its size in bytes), `.org expr`, `.byte expr|"text", …`, `.word expr, …` (low byte first);
   - errors name the file and line and stop: an unknown mnemonic, an operand the instruction has no form for, an undefined or twice-defined symbol, a value out of range, a branch out of range, a byte outside the `.rom` window or written twice;
   - the `.rom` file (ROM_PARTS format) starts with a comment naming its source and saying it is generated; each statement that emits bytes is one line, `@OOO HH HH ; FFFF  source`, the offset in the ROM, the bytes, the CPU address and the statement. Unwritten words read `$FF` (erased).
2. **`Mos6502Decode`** — the core's opcode table seen from outside: for each opcode its mnemonic, its operand form and whether it is documented. The assembler builds its encoding from it, so the assembler and the CPU cannot disagree.
3. **74LS138** — 3-to-8 line decoder, DIP-16: `A`, `B`, `C` select one of `Y0_N`–`Y7_N` (low) while `G1` is high and `G2A_N`, `G2B_N` are low; otherwise all high. Datasheet `74LS138.md`.
4. **74LS273** — octal D flip-flop with clear, DIP-20: `1D`–`8D` taken on the rising edge of `CLK` (sampled), `CLR_N` low clears every `Q` at once (asynchronous). Random at power-on. Datasheet `74LS273.md`.
5. **`POR`** — power-on reset, DIP-8: `RST_N` (open collector) pulls low from power-on for `time=` (required, at least 1 µs), then lets go for good. It has no inputs. Datasheet `POR.md`.
6. **Recipes** — ADD_MACHINE: ROM programs (`.asm` in `roms/`, `bw asm`, the `.rom` committed and checked). ADD_PART: the decoder, the octal register and a Part that acts by itself after power-on.

Nothing else is new: the 6502, ROM and RAM Parts, the Board pipeline, the Session and the instruments are unchanged. The watch budget is unchanged.

## Machine

110 LED computer, built as in [LED_COMPUTER_DESIGN.md](../machines/110-led-computer/LED_COMPUTER_DESIGN.md): a 6502 at 1 MHz, a 2716 with the program, two 2114s, the 74LS138 and 74LS02 decoding, the 74LS273 driving 8 LEDs, the power-on reset and a Reset button; netlist, program, manuals, Board placed by `bw place` and routed by the user. No Puzzles.

## Tests

- **Decode:** 151 opcodes documented, with their mnemonics and forms as in the 6502 data sheet (a few spot checks: `$A9` `LDA #`, `$6C` `JMP ()`, `$96` `STX zp,Y`, `$EA` `NOP`; `$EB` and `$1A` undocumented).
- **Assembler:** every documented (mnemonic, form) pair assembles to its opcode and length; the zero-page form chosen for a known small value, the absolute one for a forward reference; branches forwards and backwards, out of range refused; local labels scoped; constants; each expression operator and its precedence; `<` and `>`; `.byte` with text, `.word` little-endian; `.org`; every error with its line; the `.rom` text (header naming the source's filename, lines, offsets) for a small program, read back by `ParseRomContents` to the same bytes.
- **Assembled ROMs:** every `roms/*.asm` assembles to exactly its committed `.rom`.
- **`bw asm`:** a ctest runs the command on `roms/led_computer.asm` and compares its output with the committed `.rom`.
- **74LS138:** all 8 selects with the enables on; each enable off forces all outputs high; X on a select makes X only on the outputs it could change; delays against `delays::`.
- **74LS273:** a byte taken on the rising edge from before the edge; nothing on the falling edge; clear wins over the clock; X on `CLR_N`; `D`s sampled; delays.
- **POR:** `RST_N` low from power-on until exactly `time`, then released (pulled up); a button on the same Net still pulls it low later; `time=` parsing and its errors.
- **110 LED computer:** see its design (power-on reset, the light's sequence and timing, Reset, the program's variables in the 2114s); Board checks, KiCad DRC, manuals; a Session test with every instrument attached.
- **Release:** `bw_ui` on the 110 with every instrument attached at 1.00×, screenshot viewed.

## Later

- Macros, conditional assembly, `.include`, undocumented opcodes in the assembler: when a ROM needs them (the BW-1 monitor may).
- A listing or symbol file for the debugger-less player: the `.rom` already carries the source as comments.
- The latch's data hold at the end of a write (the clock rises about 80 ns after `PHI0` falls, the CPU lets go of the data at 90 ns) is a real 6502-board margin; timing faults are Later (ARCHITECTURE, roadmap).
