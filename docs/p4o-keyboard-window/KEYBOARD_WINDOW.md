# P4o — Keyboard window

Roadmap step P4o ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the Machine's ASCII keyboard drawn as a real 1970s keyboard in its own window, in place of P4n's Type button and "Sent:" line. The keyboard Part changes only in what it may send (Ctrl and Esc). The 090 TV typewriter moves to P4p and its Puzzles to P4q.

The P4n keyboard section is just a toggle and a line of text: nothing says what the Machine's keyboard is, which keys it has or that it sends upper case only. A drawn keyboard shows all of that, and its key legends teach how ASCII works: on a bit-paired keyboard, Shift flips one bit of the code, so `!` sits over `1` and `*` over `:`.

Decided with the user (2026-11-11):

- **A new row P4o**, before the 090 TV typewriter (now P4p, its Puzzles P4q).
- **The look matters most.** Drawn keycaps with legends, pressed and released as the player types, like a vintage keyboard (Apple 1 style). If there is no room under the TV, it goes in a **floating window**. The side panel has no room for full-size caps (about 390 points wide, with the Probe, Chip and Repairs sections under the TV), so it is a floating window. A later UI pass will place every window.
- **Upper case only.** Letters are folded to upper case, as in P4n.
- **Black caps, white legends**: dark grey-black sculpted caps, warm-white legends, lighter grey modifier and control keys, on a black case.
- **Keys**: the 64 codes from 0x20 to 0x5F on bit-paired caps, plus **SHIFT** (two), **CTRL** (every keyboard had one), **RETURN** (CR), **BACK SPACE** (BS) and **ESC** (0x1B).
- **Typing while the window is focused**: clicking the keyboard (or F2) focuses it, and from then on every PC key goes to the Machine. Clicking anywhere else releases it, and so does **F2**, which toggles. **Escape is a key of the keyboard** (it sends ESC) and no longer stops typing. The Type button goes away.

Decided while designing (2026-11-11), for the user to overrule:

- **Layout** (bit-paired, as on the Teletype-era keyboards the Apple 1 and the TV Typewriter used), 14 key-widths wide:

  ```
          ! " # $ % & ' ( )   * =
          1 2 3 4 5 6 7 8 9 0 : -
      ESC  Q W E R T Y U I O P  BACK SPACE        O→_  P→@
      CTRL  A S D F G H J K L ; RETURN            K→[  L→\  ;→+
      SHIFT  Z X C V B N M , . /  SHIFT           N→^  M→]  ,→<  .→>  /→?
                  [    SPACE    ]
  ```

  Shifted, a digit or punctuation key sends its code with bit 4 flipped (`1` 0x31 → `!` 0x21). So do `K`–`P`, where the shifted symbol is printed small above the letter. Every other letter sends itself shifted or not. `0` shifted sends nothing, as on the originals.
- **CTRL** with a key whose code is 0x40–0x5F sends that code AND 0x1F: `CTRL`+`A` is 0x01, `CTRL`+`SHIFT`+`P` (`@`) is 0x00 (NUL), `CTRL`+`SHIFT`+`K` (`[`) is 0x1B. CTRL with a digit, punctuation or Space sends the key's code unchanged. The keyboard can therefore send every code from 0x00 to 0x5F. `KEYBOARD.md` says so.
- **Pressed and released.** A cap goes down while its PC key is held, while the mouse holds it, and for 150 ms of wall-clock time when `--type` sends it (until the next key, if that comes sooner). A key typed on the PC is drawn by what it types, not by where it sits on the PC keyboard, so any PC layout works. A character that needs SHIFT on this keyboard (`!`, `@`) draws SHIFT down too. PC Shift and Ctrl held draw SHIFT and CTRL down.
- **The mouse**: clicking a cap types it. SHIFT and CTRL clicked latch until the next key (drawn down while latched). Clicking a latched one again releases it.
- **PC keys**: printable characters by what they type; Return and keypad Enter → RETURN, Backspace → BACK SPACE, Escape → ESC, Ctrl with a letter or with `@ [ \ ] ^ _` (as the PC layout types them, Shift included) → that control code, Ctrl with a digit or punctuation → the key itself; held keys repeat. F2 stops typing. Tab, arrows, function keys and non-ASCII characters type nothing.
- **Look**: each cap is a skirt (darker) with a dished top face inset more at the bottom than the top (the cap seen slightly from the front), a soft shadow under it and a highlight along the top of the face. Pressed: the cap sinks by a tenth of its height, loses its shadow and darkens a little. Legends in a sans-serif (Roboto Medium, shipped with ImGui under the Apache 2.0 licence), single legends centred, double legends stacked (shifted above). Names on the control keys are smaller and left-aligned at the bottom, as on the originals. The case has rounded corners and a thin lighter top edge. A small engraved label `ASCII KEYBOARD · J1` sits above the top row. The window resizes, and the caps scale with it.
- **Focus shown**: while typing, the case edge glows amber and the banner reads `Typing on J1 · F2 to stop`.
- **The window** opens with the Machine when it has a `KEYBOARD`, at the bottom of the Board area, and can be closed. F2 reopens it focused. Only the first `KEYBOARD` is drawn, as in P4n.
- **Typing with the power off** moves the caps (it is a physical keyboard) but sends nothing, as in P4n.
- **Keys sent** (P4n's "Sent:" line) move to the Debug section, with control codes shown as `[CR]`, `[BS]`, `[ESC]`, `^A`…

## New code

1. **Codes the keyboard sends.** `Session::typeCode(reference, code)` for any code from 0x00 to 0x5F; `Session::type(reference, char)` goes through `KeyboardCode` and then `typeCode`, unchanged for text and `--type`. The `Keyboard` Part is unchanged. `KEYBOARD.md` gains CTRL and ESC.
2. **The keyboard layout** (`src/view/keyboard_layout`): the caps, their positions in key-widths, legends, codes plain and shifted, CTRL's rule, and the cap that types a given code. Pure data and functions, tested headless.
3. **The keyboard window** (`src/ui/keyboard_window`): the drawing, mouse presses, latches, flashes for `--type`, and focus as typing. The legend font is embedded at build.
4. **Keyboard capture** reworked: on while the window is focused, F2 toggles, Escape sent, Ctrl codes, and the PC keys held mapped to the caps they typed.
5. **Side panel**: the Keyboard section is removed and the keys sent go in Debug.

Nothing changes in the simulator, the Parts' timing, `bw run` or the Board.

## Tests

- Layout: every code from 0x20 to 0x5F is typed by exactly one cap, plain or shifted, and `0` shifted types nothing; shift flips bit 4 on digits, punctuation and `K`–`P`, and changes nothing on the other letters; CTRL gives `code & 0x1F` for 0x40–0x5F and nothing new otherwise, so all 0x00–0x5F are reachable; RETURN, BACK SPACE and ESC send 0x0D, 0x08 and 0x1B; `CapFor` of each code gives a cap and the modifiers that type it; no two caps overlap; every row fits in 14 key-widths.
- Session: `typeCode` takes 0x00, 0x01, 0x1B and 0x5F and refuses 0x60 and 0x7F; the bench RAM holds a control code typed; `type` behaves as before.
- UI check (release): `bw_ui tests/data/ram_bench.bwn --type 'J1:100ms:THE QUICK BROWN FOX' --screenshot … --after 1s` shows the keyboard window with one cap down: keys are sent every 100 ms of simulated time and each stays down 150 ms of wall-clock time, so this holds in release at 1.00× (the ratio printed). No amber edge and no Typing banner: the window does not take focus by appearing. `--type 'J1:100ms:!!!!!!!!!!!!!!!'` shows `1` and SHIFT down. The same screenshot on `puzzles/gibberish.bwp`, which has no keyboard, shows no keyboard window. A user check covers typing: F2, a few keys with caps going down and up, `!` drawing SHIFT, Ctrl-G, Ctrl-Shift-2 (`^@`), Escape sent, clicking the Board releases, F2 again.

Design: [KEYBOARD_WINDOW_DESIGN.md](KEYBOARD_WINDOW_DESIGN.md). Plan: [KEYBOARD_WINDOW_PLAN.md](KEYBOARD_WINDOW_PLAN.md).
