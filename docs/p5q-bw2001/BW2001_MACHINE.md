# P5q — BW-2001

Roadmap step P5q ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the fourth of the BW-2001's (P5n chips, P5o Tiny BASIC spec, P5p firmware, P5q Machine, P5r Puzzles). It builds Machine 130 BW-2001 ([BW2001.md](../machines/130-bw2001/BW2001.md)): the PET 2001's main board 320137, transcribed from Commodore's schematic, running P5p's firmware, with its picture on the TV and its keyboard in a window of its own.

What is new around it:

- **The netlist is Commodore's.** The three schematic sheets, transcribed pin by pin ([schematic/](../machines/130-bw2001/schematic/README.md)), were joined and run before this spec was written: Commodore's BASIC 1 boots on them and draws its banner on the TV, in the right place. So the netlist is the PET's, and the step builds on a known-good transcription.
- **The video RAM is shared as on the 320137.** Each CPU access to `$8000` takes the RAM's address lines from the video counters, and the video draws the CPU's byte meanwhile: snow, from the netlist alone.
- **Our editor waits for the retrace**, as the PET's own does, so it prints without snow; the PET's killer poke stops the wait.
- **An NMOS port's low output loses to a TTL high**: after the killer poke the VIA drives the retrace line low against the video's TTL gate. On the real board the gate wins and the retrace interrupt goes on; in our kernel today both are Strong and the line goes X, which would stop the keyboard.
- **The monitor on separate drives**, as the PET's built-in one: video, horizontal drive, vertical drive.
- **The BW-2001 keyboard** behind the PET's keyboard connector: the PET 2001's 73-key matrix in a device that joins rows to columns through P5i's switch network (ghosts and all), drawn in its own window as the PET's chiclet keyboard, with PC typing by character and paste.
- **The PET's edges on headers**: IEEE-488 (with the MC3446 transceivers as a logic-only Part), the user port (where DIAG and the video signals also are) and both cassette ports.
- **A PET grid in `bw place`**: each chip at its own PET location.

## Decided

With the roadmap row (2026-11-15): the 6502, the 080/090 video chain on video RAM, CPU/video arbitration; the keyboard on the Board key by key, or floating and faultless; PC typing and paste as key presses; the netlist in tested stages with each stage's ratio measured (headless and `bw_ui`) and recorded; the real PET ROMs run on it as the reference; Board, manuals. From P5n: board 320137, the PET's own chips. From P5p: our firmware, split over the PET's sockets; "the screen is written at any time, so P5q decides whether CPU access shows snow".

Asked at the start of the step (2026-11-15):

- **Arbitration as the 320137**: the 74LS157s hand the video RAM to the CPU on every `$8000` access and the video draws what is there; snow comes from the netlist. Offered instead: snow-free arbitration (the later 2001N's).
- **Our editor waits on VIA `PB5`** before writing to the screen, as the PET's editor does (BASIC 1, `$E7AD`: `LDA $E840`, `AND #$20`, `BNE`); the killer poke (`POKE 59458,62`) then makes it fast and snowy, as on the real machine. Offered instead: leave P5p's editor, snow on every print.
- **The keyboard: a C++ matrix behind a Connector**, floating and faultless (a drawn PET chiclet keyboard in its own window); the Board keeps the connector, the 74LS145, the PIA and the pull-ups, as the PET whose keyboard is a separate board on a cable. Offered instead: the 73 keys on the Board.
- **Speed: accept and record.** The 320137 built faithfully; each stage measured headless and in `bw_ui`; gated where gating keeps the circuit (070/090); deeper kernel work its own roadmap row. Offered instead: real time required within the step; stop at the first stage below 1.00×. Measured on the joined transcription (2026-11-15): **0.24×** headless, the 8 MHz clock and its gated copy 28 % of all Net changes.
- **A PET-style monitor connector**: video, horizontal drive and vertical drive, with the TV locking on separate drives. Offered instead: mix a composite sync on the Board with a gate the PET does not have.
- **Chips at the PET's grid** (`UA1` … `UJ9`): a `bw place` rule putting each at its location. Offered instead: annealed as usual.
- **IEEE-488, user port and both cassette ports on headers, as the PET.** No MC3446s or cassette drivers were planned; with all three ports on headers the MC3446s come as a logic-only Part, so the IEEE header behaves as the PET's. Offered instead: the user port only; none.
- **PC typing by character**: a PC key presses the PET key that types its character, with the PET's SHIFT when needed; arrows, Home, Backspace, Escape to CRSR, HOME, DEL, STOP; paste queued as presses. Offered instead: by position.

Chosen in this spec (not asked):

- the PET's expansion connector `J4` left out (about 50 Nets for nothing to plug in);
- the 555 replaced by `POR time=1ms`, not the PET's 1.1 s;
- the crystal oscillator by an 8 MHz `OSC`;
- the cassette motor transistors replaced by the control pins themselves on the headers;
- each scripted key held 40 ms and released 40 ms (more than two retraces each);
- the edge connectors as pin headers (2 × 12), not card-edge fingers;
- DIAG by a Bodge Wire on the user port (as on the PET, a jumper there), no DIAG switch;
- P5p's minimal netlist `tests/data/bw2001_firmware.bwn` kept for the full conformance set in release (it runs at 1.04–1.07×, the Machine at about 0.24×); the Machine runs a session and the reference cases.

## New

1. **Port drive strength.** `DriveStrength::Port` between `Weak` and `Strong`: a `PullUp` Pin (6520, 6522, 6530 ports and control lines) drives 0 at `Port` strength, 1 `Weak` as now. A port's 0 still beats a pull-up and another port's 1; a TTL or NMOS output's 1 beats it, as an LS output's high (tens of mA) beats an NMOS port's sink (about 2 mA).
2. **`74LS139`**: dual 2-to-4 decoder (DIP-16).
3. **`MC3446`**: quad IEEE-488 bus transceiver, logic only: each channel's bus pin is pulled low while its driver input and its enable are low, else released to the chip's termination (a `PullUp` Pin); its receiver output follows the bus. Motorola's MC3446A data sheet (1979 interface data book, p. 5-54).
4. **Passive connectors** `IEEE488`, `USERPORT` (2 × 12), `CASSETTE` (1 × 6): the PET's pin labels as pin names, Inputs (`Family::Mos`, no "floats high"), no behaviour.
5. **`MONITOR`** (1 × 7): `VIDEO_N`, `HDRIVE`, `VDRIVE` and grounds, the PET's `J7`. The TV gains a separate-drive input: a line starts at `HDRIVE`'s rise, a frame at `VDRIVE`'s fall, each when accepted as today's syncs are; the dot is lit while `VIDEO_N` is low. The Session and `bw run --tv` find either connector.
6. **`MATRIXKB`** (1 × 20): the BW-2001 keyboard. Ten row pins, eight column pins; 73 keys inside, each a contact between its row and its column, joined by the Board's switch network as a P5i switch is; held from the UI (a click, PC typing) or pressed from a queue in simulation time (`type`: each character's keys held 40 ms, then 40 ms released; `bw run --type J5:500ms:TEXT`). No Faults. Its window draws the PET 2001's keyboard (the main block and the numeric pad, chiclet caps); capture mode, PC keys by character, paste (Cmd/Ctrl+V while it captures).
7. **`grid pet`** in `.place`: chips named `U<A–J><1–9>` (and `UDE9`) at their PET location, lying horizontally; columns as wide as their longest chip.
8. **The editor waits for the retrace**: every access to the screen, read or write (printing, clearing, scrolling, the cursor, RETURN reading a row), waits until VIA `PB5` reads low (`VIDEO_ON` low: the 60 blank lines), as BASIC 1's. Our reset sets the VIA's `CA2` low (`PCR` = `$0C`) for the upper-case character set, as BASIC 1's. The model (`bw2001_model`) gets `PB5`.
9. **Machine 130 BW-2001**: `netlists/bw2001.bwn` in three stages, manuals, `boards/bw2001/`, tests ([BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md)).

## Tests

- Port strength: a `PullUp` 0 against a TTL `Output` 1 resolves to 1, against a pull-up resistor to 0, against another port's 1 to 0; the BW-1, P5n bench and P5p netlist suites unchanged.
- `74LS139`, `MC3446`: truth tables, delays, X; the MC3446's bus released while its enable is high, its receiver following an outside pull-down.
- TV: separate drives lock at 60 Hz with 260 lines; a missing `VDRIVE` rolls; the picture's dots where `VIDEO_N` is low; the composite TV unchanged (the test pattern, 080, 090 cases).
- `MATRIXKB`: pins, a held key joins its row and column; three keys in a rectangle ghost the fourth; `type` holds and releases in simulation time; the character map (`!` unshifted, CLR as SHIFT+HOME, lower case typed as upper); the window's layout headless (every matrix key drawn once).
- `grid pet`: `UC9` at row C, column 9; a column as wide as its DIP-40; a reference outside the grid refused unless it has another rule.
- Firmware: the 31 P5o sessions and P5p's own cases on the model with `PB5` (blank 60 of every 260 lines); no screen access, read or write, in the display window in any session; the P5p minimal netlist's cases unchanged.
- Machine, stage by stage ([BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md#tests)): stage 1 the locked picture of the power-on video RAM; stage 2 the banner on the TV, no snow; stage 3 typing through `J5`, a listing without snow, the self-test by `J2` pin 5, the real BASIC 1's banner and the killer poke's snow with the keyboard still working.
- Release: each stage's headless ratio (`bw run --time 2s`) and `bw_ui` ratio and UI time; the Board with every instrument; screenshots viewed: the banner, a listing, the killer poke's snow, the keyboard window.

Design: [BW2001_MACHINE_DESIGN.md](BW2001_MACHINE_DESIGN.md). Plan: [BW2001_MACHINE_PLAN.md](BW2001_MACHINE_PLAN.md). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md).
