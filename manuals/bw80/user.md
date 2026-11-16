# BW-80

## What it does

Connect +5 V to `J1` and the TV to `J2`. For a moment the TV shows nothing while the BW-80 clears its memory, then it locks on a picture: white, with one black square at the bottom left holding a white `K`. That is the cursor of the editor, waiting for a command. The keyboard is the window beside the Board (`J3`).

The BW-80 runs the Sinclair ZX80's own program, so it is used as a ZX80 was. One key types a whole keyword: while the cursor shows `K`, the O key types PRINT, the K key LET, the A key LIST, and so on, as written above each key. After the keyword the cursor shows `L` and the keys type their letters; THEN, TO, AND, OR and NOT are SHIFT with a digit or B, the symbols SHIFT with a letter, as written in yellow on the keys. NEW LINE enters the line. `PRINT 2+2` shows 4 at the top of the screen and the report 0/-2 at the bottom: no error (0), and line -2, the edit line where a direct command runs. After RUN the report names the program's last line, 0/20. A line with a number is kept as part of the program; RUN runs it, LIST lists it, NEW clears it. The picture goes blank whenever the BW-80 is busy, while it works on a key and while a program runs: that is how a ZX80 is, not a fault.

On a PC keyboard, click the keyboard window or press F2, then type: each PC key presses the BW-80 key with the same character, with SHIFT where the ZX80 needs it (a `"` presses SHIFT and Y). Enter is NEW LINE, Backspace RUBOUT, the arrows the cursor keys, Home HOME, Escape BREAK, Shift+Enter EDIT. Cmd+V (Ctrl+V) pastes a program listing, typed keyword by keyword, so a program copied from a book runs.

There is no cassette recorder: SAVE writes nothing, and LOAD waits until BREAK (the SPACE key) stops it. The edge connector at the back (`J4`) is the ZX80's expansion bus, with nothing plugged in.
