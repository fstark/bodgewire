# P4p — TV Typewriter

Roadmap step P4p ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the 090 TV typewriter running, P4n's RAM, bus buffer and keyboard on a shared bus. The Machine is specified in [TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md). No Puzzles: P4q.

Decided with the user (2026-11-11):

- **32 × 16 characters at a 4 MHz dot clock**, the original TV Typewriter's page size.
- **Return only**: Return starts the next line; every other control code (Backspace, Esc, CTRL) does nothing. Printable codes 32–95 are written and step the cursor.
- **A blinking block cursor**, compared from the cursor and beam counters with parts in the catalogue.
- **A Clear button**: while held, spaces are written into every cell and the cursor goes home. The page is random at power-on until Clear is pressed, like the original.
- The original's memory, long MOS shift registers, is not modelled: the page is in two 2114 RAMs (P4n). A shift-register page would be a new Part.
- **Below real time accepted** (after the docs review measured the design, 2026-11-11): release headless 1.0× with a full page of characters (the random power-on page), 1.3× on a mostly blank typed page; `bw_ui` 0.93× and 1.00× without instruments. Each chip on the 4 MHz clock costs twice what it does at 2 MHz, and a full page keeps the RAMs and the 2513 switching every character (080 at 2 MHz: 2.29×). The release check records the ratios instead of requiring 1.00×.

Decided while designing (2026-11-11), for the user to overrule:

- **080's video chain at twice the rate**: 6-dot characters of 1.5 µs, 42 characters a line (32 text, 10 border), the same 63 µs line and 312-line frame, each glyph line drawn twice. On the TV a dot is 2 columns, so the page is as wide as 080's with letters half as wide.
- **Fewer chips on the dot clock**: the line counter and the cursor counter are clocked by a line-end pulse, and the write strobe and that pulse are re-timed in the 74LS175 that re-times video and sync, so the 4 MHz clock reaches five chips and the inverted clock one (080: eight and one at 2 MHz).
- **Writes during the right-hand border.** During characters 32–41 a 74LS157 multiplexer gives the RAMs the cursor's address; a key is written there, on characters 35 and 39 of the next line after its strobe, and the cursor steps at that line's end. One key a line at most (15 870 a second).
- **Keyboard strobe 100 µs** (`KEYBOARD strobe=100us`): the code must stay on the keyboard's lines until its write and step are over (at most 79 µs), and the next strobe must come after the key is taken. Keys typed faster wait in the keyboard, as P4n made them.
- **6 bits stored**: bit 6 of the code is not kept (the 2513 takes the low 6 bits); the second 2114 uses two of its four data pins.
- **The cursor** is the cell inverted (6 dots × 16 lines), on for 8 frames and off for 8 (3.2 Hz), from a 74LS93 counting frames.
- **Clear** drives 0x20 onto the bus from a second 74LS244 and holds the RAMs' write enable low while the beam sweeps every cell: the page is blank after one frame (20 ms).
- **Reference `typewriter`**, Machine directory `docs/machines/090-typewriter/`.

## New code

None in `src/`. Every Part is in the catalogue, the TV takes 4 MHz video (a dot is 2 of its 125 ns columns), `bw_ui` opens the keyboard window by itself for a Machine with a `KEYBOARD`, and `--type` and `--press` drive it from the command line.

## Machine

The TV typewriter is built as in [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md): netlist, Board (placed by `bw place`, routed by the user), manuals. The netlist grows in three tested stages: the video chain reading the RAMs, then the keyboard, write logic, cursor counter and Clear, then the cursor on the screen.

## Tests

`tests/machines/typewriter_test.cpp`:

- The netlist loads without warnings.
- On the TV, before any key: the page shown is the RAMs' power-on contents, each of the 512 cells its character in the font, nothing lit outside the text area.
- From the RAMs' contents (fast cases, no TV): Clear leaves 0x20 in every cell and the cursor at (0, 0); typed keys land in order from the cursor; Return starts the next line; Backspace, Esc and CTRL codes write nothing and leave the cursor where it is; the 33rd key of a line lands at the start of the next; 16 Returns bring the cursor back to the top; 32 keys queued at once all land.
- On the TV, after Clear and a few typed lines: the page is the typed text; the cursor's cell is inverted on some frames and plain on others, and every other cell is the same on every frame.
- The typewriter joins the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; a Session test with every instrument attached and a key typed.
- Release build: headless speed recorded (random and typed page); `bw_ui` with every instrument attached, its ratio and UI time recorded, a screenshot of a typed page with the keyboard window.

Design: [TV_TYPEWRITER_DESIGN.md](TV_TYPEWRITER_DESIGN.md). Plan: [TV_TYPEWRITER_PLAN.md](TV_TYPEWRITER_PLAN.md).
