# BW-2001

## What it does

Connect +5 V to `J8`, the monitor to `J7` and the keyboard to `J5`. The monitor shows random characters for a moment while the BW-2001 checks its memory, then clears and prints:

```
*** BW-2001 TINY BASIC ***

7166 BYTES FREE

READY
```

with a blinking square under `READY`: the cursor. Type a line on the keyboard and press RETURN. A line with a number is kept as part of the program; a line without one runs at once (`PRINT 2+2` prints 4). `LIST` shows the program, `RUN` runs it, `NEW` clears it. The cursor keys move the cursor anywhere on the screen; RETURN on a line that is already there enters it again, so a listed line is changed by typing over it. HOME puts the cursor top left, SHIFT and HOME (CLR) clears the screen, DEL deletes, SHIFT and DEL (INST) inserts. STOP stops a running program.

The keyboard is the PET 2001's: the letters and the top row's symbols on the left, the digits and `/ * + - = .` on the pad at the right. On a PC keyboard, click the keyboard's window (or press F2): each PC key types the BW-2001 key with the same character; Return is RETURN, Backspace DEL, Escape STOP, Home HOME, the arrows the cursor keys. Cmd+V (Ctrl+V) pastes text, typed one key at a time.

`PRINT USR(61440)` enters the machine-code monitor (`C*`): `.M 0400 0420` shows memory, `.X` goes back to BASIC.

The screen is written only while the beam is outside the text, so printing is slow and the picture stays clean. On the original PET 2001, `POKE 59458,62` stopped that wait: the screen became fast and filled with white specks (snow) while it printed. It does the same here.

If the BW-2001 will not start or behaves oddly, run its self-test. It is in the chip at $F800 with the reset, so it runs even when BASIC, the editor or the monitor cannot, and it needs no working memory to report the first faults.

- To run it: join user port pin 5 (`J2` pad 9, `DIAG`) to a ground pin of the same connector (`J2` pad 1) with a wire, then power off and on. Remove the wire to return to BASIC. A working machine also starts it from the monitor with `.T`.
- Each test writes its row: ZERO PAGE, VIDEO RAM, RAM, then each ROM by its address. A row ending in OK passed. A failure writes the address and the bad bits (what was written XOR what was read back) and stops: `RAM BAD 1234 08` is $1234 bit 3. A ROM row gives its sum, which should be 00, and the test goes on.
- Then the KEY row shows, for as long as the test runs, the first held key: its row and column in the keyboard matrix and its legend (`KEY 40 A`). With no key held it shows KEY alone.
- RAM keeps the low 4 bits of each byte in one row of chips and the high 4 bits in another: a bad bit at every address of a 1 KB block points at one chip.
