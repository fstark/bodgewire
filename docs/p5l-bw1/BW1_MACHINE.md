# P5l — BW-1

Roadmap step P5l ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the third of four building the BW-1 on the KIM-1's own design (P5j chips, P5k monitor, P5l Machine, P5m Puzzles). This step builds the working Machine, 120 BW-1 ([BW1.md](../machines/120-bw1/BW1.md)): the P5j chips wired as the KIM, the P5k programs in its two 6530s, the keypad and six digits on the Board, laid out like the KIM's.

Two things the catalogue lacks come with it:

- a **common-cathode digit**, `DIGITCC`: the KIM's 74145 sinks a digit's common pin while a `PA` line high lights a segment, the opposite of our common-anode `DIGIT`;
- a **slide switch**, `SLIDE`: the KIM's SST is a slide switch, on or off until moved, not a key.

## Decided

With the roadmap rows (2026-11-14): the KIM-1's design (two 6530s, eight 6102s, a 74145 decoding 1 KB blocks with `A13`–`A15` undecoded, a second 74145 scanning the six digits and the keypad's rows from the 6530-002, ST and SST on NMI); cassette, TTY and the other analog edges left out; LED drivers as logic-only equivalents; our own monitor (P5k); the keypad and digits as P5i Components, laid out like the KIM's; the 003's `PA` pins unconnected (P5k); the ratio measured with and without a key held.

Asked at the start of the step (2026-11-14):

- **Digits: a common-cathode digit.** A new catalogue row `DIGITCC` on the same footprint as `DIGIT` (KiCad's LTS-6760/LTS-6780 footprint covers both), pads 3 and 8 the common cathode. Its cathode is straight on U24's output (open collector: low selects, floating is dark); the segments come from `PA0`–`PA6` through one 74LS244, the logic-only segment driver: one chip, a Puzzle link.
- **ST and RS keys; SST a new slide switch.** ST a key from `NMI_N` to `GND`, RS a key from `RES_N` to `GND` beside the power-on reset (no 556: no contact bounce is modelled, P5i). SST a new `SLIDE` catalogue row (its own footprint, drawn as a slide switch, a click moves it) between a NAND of `SYNC` and `K7` and `NMI_N`.
- **The strap: nothing on the Board.** U24's output 3 is on a one-pin Net `STRAP` (pad 4, no track); the player bodges it to `PA0` to reach the self-test (P5k: "a bodge wire reaches it on a machine whose monitor is dead"). The service manual says so.
- **PC keys** (Typing, F2): `0`–`9`, `A`–`F` the hex keys; Enter GO, `+` `+`, `/` AD, `*` DA, `P` PC, Escape ST, Backspace RS. SST has none.

## New

1. **Lamps with two cathodes.** A catalogue `Lamp` names up to two cathode Pins, as it names up to two anodes; a lamp is lit while one of its anodes is high and one of its cathodes low. The common-anode `DIGIT` and the `LED` are unchanged.
2. **`DIGITCC`**: red 0.56-inch 7-segment digit, common cathode: segment anodes `A`–`G`, `DP`; `CC1`, `CC2` (pads 3, 8, joined inside). A datasheet. Drawn as `DIGIT` (same package).
3. **`SLIDE`**: a slide switch, two legs joined while on; `legend=` names it on the Board. The same `Switch` as a `BUTTON` or `KEY` (a Fault, a swap, `--press`), on a 1-position DIP slide footprint, drawn as a slide switch whose knob shows on or off; a left or right click on it moves it, and it stays where it was put across power-off and on. A datasheet.
4. **Machine 120 BW-1**: `netlists/bw1.bwn` (19 chips and the oscillator, 6 digits, 23 keys, the slide switch, 7 resistors), manuals, `boards/bw1/` (placed by `bw place`, routed by the user), tests.

## Tests

- Lamps: a two-cathode lamp lit by either cathode; the `DIGIT` and `LED` lamps unchanged; a `DIGITCC` lit while its segment pin is high and its common pin low, dark with the common pin floating.
- `SLIDE`: catalogue row, legend, the Switch's Faults; parsed with `legend=`, refused with `pc=`.
- BW-1, fault-free, on the real Parts (seeds 1–3 where the RAMs' power-on matters): reset shows `0200` and the byte there; `AD 0300 DA A9 +` shows `0301`; a program typed in and run with GO (`LDA #$42`, `STA $10`, `BRK`) stops on `0206`, and `AD 0010` shows `42`; ST stops a `JMP $0200` loop on `02004C`; SST on: GO steps one instruction at a time (`0202A9`, then `0204`); RS returns to `0200`; with the strap (a net added in the test, and a Bodge Wire on the Board through the Session) the self-test reaches `PA55`, and a held GO shows `PA5513`.
- Board: `check-board` ok, the per-Machine Board loops, a `[session]` case with every instrument.
- Release: headless ratio idle and with a key held; `bw_ui` with every instrument at 1.00×, screenshots of the keypad, the digits and SST viewed.

Design: [BW1_MACHINE_DESIGN.md](BW1_MACHINE_DESIGN.md). Plan: [BW1_MACHINE_PLAN.md](BW1_MACHINE_PLAN.md). Machine: [BW1.md](../machines/120-bw1/BW1.md), [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md).
