# P5g — Memory Faults

Roadmap step P5g ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): Faults inside a memory chip (ARCHITECTURE, Faults: "internal faults live in the component model"). Until now a RAM or a ROM could only fail at its pins. Now a bit inside it can be bad, while its pins, its Nets and every other chip stay good.

Decided with the user (2026-11-14):

- **Player view first.** A memory Fault pays off where the Machine shows the memory: a character ROM, a message ROM, later video RAM (BW-2001). In program memory almost every memory Fault ends as "it crashes", and the player swaps the chip. Puzzles should prefer memory the Machine draws.
- **Three "broken" Faults now**, steady at every power-on:
  - **stuck cell**: one bit of one word always reads 0 or 1, and writes do not change it (in an EPROM, a programmed 0 that leaked to 1);
  - **stuck column**: the same bit stuck in every word, inside the chip. A stuck output pin forces its bus line all the time. A stuck column is wrong only when this chip is read: the line is good while another chip drives it, and it floats while the chip is deselected;
  - **internal address line**: the chip ignores one address bit, so pairs of words share one cell. This is already the stuck input of P4h (`fault stuck U3.A0 0`): the chip reads 0 on `A0` while the `A0` Net is unchanged. P5g adds no code for it, only documents it and tests it on a memory.
- **"Flaky" Faults later**: the weak cell (it loses a write after a time), the flaky bit (random at each read), coupling (a write to one cell disturbs another) and read disturb go to a new roadmap row, **Flaky machines**, after the computers. Flakiness is its own class of Symptom with several causes besides RAM: intermittent contacts, thermal chips, timing margins, floating or contended lines. That row also takes the tools that make flakiness worth hunting: RAM test ROMs, tapping, heat. If RAM were the only cause, the player would just swap RAM.
- **Wrong IC fitted: dropped** for now. It is too close to "the chip doesn't work, swap it". Chip markings come later.
- **No Puzzle files** in this step: P5h (computer Puzzles) and later Machine rows write them. The Faults are tested on the 110 LED computer and the 080 message display, in their test files.
- GAME_DESIGN.md records flakiness as a class of Symptom, and the swap reflex as an open question: while a swap costs nothing, "swap every chip" solves every chip Fault (soldered or socketed chips, spares, time).

## New code

1. **Memory cells in the Parts.** The 2114 and the ROMs (2716, 2513) can have bits stuck: a stuck bit of a word reads its value from then on. In the 2114 it holds through power-on (random contents) and through every write; in a ROM it changes the contents. Reads, timings and X handling are unchanged.
2. **Puzzle lines.** Pins are named as in every Puzzle line (number or name); the data pin gives the bit.
   - `fault cell U3.IO2 $1FF 0`: bit `IO2` of word `$1FF` (the chip's own address: `A0`–`A9` of a 2114, `A0`–`A10` of a 2716, `A1`–`A9` of a 2513) stuck at 0. The word is written `$` + hex or in decimal.
   - `fault column U15.O1 0`: bit `O1` of every word stuck at 0.
   - Refused: a chip that is not a RAM or ROM, a pin that is not one of its data pins, a word beyond its size.
   - Both are repaired by swapping the chip, which removes every cell and column Fault of that chip (like its stuck pins).
3. **Builder.** `BuildCircuit` takes the stuck cells next to the stuck pins.

Nothing is drawn and the UI does not change. A swap is already the repair, and the instruments read the bus as before.

## Tests

- 2114: a stuck cell reads its value after any write and after power-on with any seed; the other bits of the word and the other words are unchanged; a column sticks every word.
- ROM: a stuck cell and a column change what the outputs drive; nothing else changes.
- Puzzle syntax: `fault cell` and `fault column` parsed (hex and decimal words); errors for malformed lines.
- Loading: refusals (unknown reference or pin, not a memory, not a data pin, word too large); a swap removes the chip's cell and column Faults and keeps the other chips'.
- Builder: stuck cells reach the Component.
- 110 LED computer:
  - a stuck cell in the stack (`U4.IO4` at `$1FF`, bit 7 of the high byte `JSR` pushes) sends the first `RTS` to `$78xx`, where nothing answers. The data bus reads `$FF`, so the 6502 runs `$FF` opcodes until it wraps round to the program's start, again and again. The light shows `D1` and never moves, the same for every seed because nothing random is read on the way;
  - `fault stuck U3.A0 0` (internal address line): `light` and `dir` share their low nibble in `U3`, and `JSR`'s two return bytes share one word. `sta dir` clears `light`, so every latch write is `0x00`, and the first `RTS` returns into erased ROM (`$FF`), which runs round to the program's start. The LEDs stay dark. Of the 2114's address lines only `A0` shows with this program: the others alias words it never uses (a note for P5h).
  - Both are fixed by swapping the chip.
- 080 message display:
  - `fault column U15.O1 0` draws the page with the thin letters of the `thin_letters` Puzzle, from inside the character ROM;
  - a stuck cell in `U15` takes one dot out of one glyph line everywhere that letter appears.
  - Both are fixed by swapping `U15`.
- Release: `bw_ui` screenshot of a message display with the stuck cell (a Puzzle file used for the check, not committed), viewed; the 110 and `bus_load` headless ratios unchanged.

Design: [MEMORY_FAULTS_DESIGN.md](MEMORY_FAULTS_DESIGN.md). Plan: [MEMORY_FAULTS_PLAN.md](MEMORY_FAULTS_PLAN.md).
