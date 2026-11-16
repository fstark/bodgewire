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

If the BW-1 will not start or behaves oddly, run the self-test. It lives in `U3` and checks the memory, the ROMs and the display without using any RAM, so it runs even when the monitor cannot.

- To run it: join `U24` pad 4 (`STRAP`) to `PA0` with a wire (`U2` pad 2 is on `PA0`), then press RS or power on. Remove the wire to return to the monitor. A working monitor also starts it with AD 1800 GO. SST must be at OFF.
- While it runs, digit 1 shows the test: 1 main RAM ($0000–$03FF), 2 the RAM in `U2` ($17C0–$17FF), 3 the RAM in `U3` ($1780–$17BF), 4 the ROM in `U2`, 5 the ROM in `U3`. Then every segment of every digit lights (888888), then each digit alone, then PA55: the keypad test, which shows the code of a held key (0–F the hex keys, 10 AD, 11 DA, 12 `+`, 13 GO, 14 PC) and nothing else while no key is held.
- A failure stops the test and stays on the digits: digit 1 the test, digits 2–4 the last three hex digits of the address, digits 5–6 the bad bits (what was written XOR what was read back). For a ROM, digits 2–4 show its first address and digits 5–6 its sum, which should be 00. For example 123408 is $0234 bit 3, 27D501 is $17D5 bit 0, 4C0001 is the ROM in `U2` summing to 01.
- Main RAM keeps each bit of every byte in one chip: bit 7 in `U5`, bit 6 in `U6`, and so on to bit 0 in `U12`. One bad bit at every address points at one chip; several bits at once point at what they share: the buffers `U13` (bits 0–3) and `U14` (bits 4–7), the gates, the decoder.
