# P5n — BW-2001 chips

Roadmap step P5n ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the first of the BW-2001's (P5n chips, P5o Tiny BASIC spec, P5p firmware, P5q Machine, P5r Puzzles). It brings the chips of the Commodore PET 2001 that the catalogue lacks, tested on a bench before any Machine exists, and lets a ROM Part read a real PET ROM dump kept outside git.

## The PET 2001's chips

The chip list comes from the PET 2001 schematics (320026, 320082, 320130) and the Commodore "Main Logic Assembly Parts Cross Reference" (zimmers.net, `pub/cbm/schematics/computers/pet/2001/`), checked against MAME's `pet.cpp`. All four board revisions share one set of locations. They differ in their ROMs (6540 or 2316B) and RAMs (6550 or 2114):

| Function | Chips (location) | In the catalogue |
|---|---|---|
| CPU, buses | 6502 (UF3); 7417 address buffers (UE3, UE4); 74LS04 (UE5); 74LS244 data buffers (UG5, UG6, UH8, UH9) | 7417 new |
| Decoding | 74LS154 4 KB blocks `SEL0`–`SELF` (UG2); 74LS00 (UG3); 74LS21 (UG4); 74LS08 (UE2) | 74LS154, 74LS21 new |
| RAM | 2114 × 16, 8 KB (UI1–UI8 low nibble, UJ1–UJ8 high) | yes |
| ROM | 2316B × 7: BASIC 1 at `$C000`–`$E7FF`, `$F000`–`$FFFF` (UH1–UH7) | 2316B new |
| Video timing | 8 MHz crystal; 74LS93 (UC9, UB5, UA1); 74LS107 (UB6, UC5, UC7, UC8, UD5); 74LS08, 74LS00, 74LS20 | yes |
| Video RAM | 74177 address counters (UD6, UD7); 74100 row-start latch (UE6); 74LS157 CPU/video address muxes (UD2–UD4); 2114 × 2 (UC3, UC4); 74LS244 onto the bus (UB3, UB4) | 74177, 74100 new |
| Characters | 2316B character ROM 901447-08 (UA2); 74LS165 (UB2); 74LS74 reverse video (UC1) | 2316B new |
| I/O | 6520 PIA1 keyboard, cassette, diagnostic sense, retrace interrupt (UG8); 6520 PIA2 IEEE-488 data (UB8); 6522 VIA user port, IEEE control, cassette, retrace, character set (UA5) | 6520, 6522 new |
| Keyboard | 74LS145 rows 0–9 (UG9), 10K pull-ups on the columns | 74LS145 new |
| IEEE-488, cassette, reset | MC3446 × 3, transistors, LM555 | not in this step |

The 74LS145 already exists as the plain `74145`. The PET fits the LS part, so `74LS145` comes in as a variant sharing its code. The IEEE-488 transceivers and the cassette drivers are analog edges, left out as on the BW-1; P5q decides what stands in for them. The 555 reset is our `POR`.

## Decided

With the roadmap row (2026-11-15): our own firmware runs on the BW-2001; the real ROMs, owned by the user, are local test references outside the repo, and their tests are skipped without them.

Asked at the start of the step (2026-11-15):

- **Board revision 320137**: 2316B ROMs and 2114 RAMs. The 2114 is already in the catalogue with its Faults, and the 2316B fits the DIP-24 package.
- **The PET's own chips** for the glue and the video circuit: 74LS154, 74177, 74100, 7417, 74LS21 and 74LS145, so that the BW-2001 is the PET's schematic, as the BW-1 is the KIM's.
- **The whole datasheet** for the 6520 and the 6522: every register and mode. On the VIA that includes both timers, the shift register (PET programs make sound on `CB2` with it), input latching and the handshakes.
- **The PET's own character ROM ships with the BW-2001** (asked during the step, 2026-11-15): `roms/901447-08.bin`, the user's dump of the 2316B-08 (SHA1 `3e067cc6…`, MAME's), committed in git, since every screen of the Machine goes through it. Offered instead: our own font in the PET's screen-code layout, or deciding at P5q. The program ROMs stay outside the repo.
- **BASIC 1 boots to its banner** on the bench: raw 2 KB `.bin` dumps (901447-01 to -07, the user's own) in git-ignored `external/roms/pet/`, read by `contents=`. The test is skipped without them.

## New

1. **Binary contents**: `contents=x.bin` reads a raw dump, one byte a word, its size exactly the ROM's. Text `.rom` files are unchanged.
2. **`2316B`**: 2K × 8 mask ROM, the 2716's pinout with three chip selects. Their polarities are set in the mask, so they are a parameter: `part UH1 2316B select=LLH contents=…`, one letter each for `CS1`, `CS2`, `CS3` (L active low, H active high).
3. **Real ROMs for tests**: a test helper finds `external/roms/pet/<file>` and skips the case when it is missing. A `README` beside the bench names the files and their SHA1s.
4. **TTL**: `74LS154` (4-to-16 decoder, DIP-24), `74177` (presettable binary counter, ÷2 and ÷8 counting on falling edges, asynchronous clear and load), `74100` (8-bit latch, two enables, DIP-24), `7417` (hex buffer, open collector), `74LS21` (dual 4-input AND), `74LS145` (the 74145 with its own delays).
5. **`6520`**: peripheral interface adapter. Two 8-bit ports with direction registers, four control lines with their edges, interrupts, handshake and pulse outputs. `PA` and `CA2` have pull-ups, `PB` and `CB2` are three-state, the IRQs open drain. Woken only by `PHI2`.
6. **`6522`**: versatile interface adapter, the whole data sheet. Ports with input latching, `CA1`/`CA2`/`CB1`/`CB2` in all their modes, T1 one-shot and free-running with `PB7` output, T2 one-shot and `PB6` pulse counting, the shift register's eight modes, `IFR`/`IER`. A core class apart from the Pins, stepped one `φ2` cycle at a time, under the Part woken only by `PHI2`.
7. **A bench** `tests/data/bw2001_bench.bwn`: the PET's CPU, decoding, 8 KB of RAM, video RAM, seven ROM sockets, PIA1, PIA2, the VIA, the keyboard's 74LS145 with two keys, and a 60 Hz square wave standing in for the retrace (`RETRACE`). Our program `roms/bw2001_bench.asm` sits in the `$F800` socket; the boot test puts the real BASIC 1 in all seven.

## Tests

- Contents: a `.bin` of the right size reads byte for byte; a wrong size and an unreadable file give their errors; text files unchanged.
- 2316B: every `select=` combination selects only at its levels; select delay, access delay and float; parameter errors (missing, wrong length, wrong letter, on a 2716).
- TTL: truth tables, X handling and datasheet delays of each Part; the 74177's ripple order, asynchronous clear and load over counting; the 74100's transparency and hold; the 7417 releases its 1.
- 6520: register selection by `RS0`, `RS1` and `CRx` bit 2; port reads (`PA` the pins, `PB` the latch on output bits); `CA1`/`CB1` edges set the flags and the IRQs when enabled; reading the port clears them; `CA2`/`CB2` as inputs, handshake, pulse and manual outputs with their delays; reset.
- 6522: every register; T1 read N, N−1 … 0, `$FFFF`, N; IRQ low N + 1.5 cycles after the write, then every N + 2 cycles; `PB7` one-shot pulse and free-running square wave; T2 one-shot and `PB6` counting; the shift register in each mode, at its bit rate, `IFR` bit 2 after 8 bits; latching; `PCR` modes; `IFR`/`IER` set and clear rules; reset keeping the timers and the shift register. Each rule checked to fail on a mutation of it.
- Bench, seeds 1–3: our program checks the RAM and the video RAM, sets up the PIAs and VIA on the real bus, and reports `$55` on PIA2's port B; the VIA's `PB7` square wave, its timer IRQ, its shift register and the retrace interrupt at their periods; two keys in the matrix read through the 74LS145; a stuck cell in the RAM or the video RAM fails its test.
- BASIC 1 (skipped without `external/roms/pet/`): by 0.6 s, video RAM holds `*** COMMODORE BASIC ***` on row 0, ` 7167 BYTES FREE` on row 2 and `READY.` on row 4.
- Release: the bench's headless ratio, idle and booting BASIC 1, recorded; the 110, `bus_load` and BW-1 ratios unchanged.

Design: [BW2001_CHIPS_DESIGN.md](BW2001_CHIPS_DESIGN.md). Plan: [BW2001_CHIPS_PLAN.md](BW2001_CHIPS_PLAN.md).
