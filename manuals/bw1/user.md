# BW-1

## What it does

Connect +5 V to `J1`. After a moment the six digits show `0200` and a byte: an address and what memory holds there. The keypad at the right edits and runs memory, as on a KIM-1:

- AD, then hex keys: each key shifts into the address from the right (`AD 0 3 0 0` shows `0300`).
- DA, then hex keys: each key shifts into the byte at the address, written to memory.
- `+`: the next address. PC: back to where a stopped program was.
- GO: runs the program from the address shown. A `BRK` (`00`) in it, or ST, stops it and shows where it stopped.
- SST (the slide switch, top right): at ON, each GO runs one instruction and stops.
- RS: reset, back to `0200`.

Programs usually start at $0200: `AD 0200`, then `DA` and the first byte, `+` and the next byte, and so on, then `AD 0200 GO`.

On a PC keyboard, after F2 (Typing): `0`–`9` and `A`–`F` the hex keys, Enter GO, `+`, `/` AD, `*` DA, `P` PC, Escape ST, Backspace RS.

If the BW-1 will not start or behaves oddly, the Service Manual's self-test checks its memory, ROMs and display.
