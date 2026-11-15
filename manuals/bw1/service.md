# BW-1 — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `PHI0` of `U1`
- `J1` power connector: +5 V and 0 V for the whole board
- `U1` processor: a 6502; reads and writes on `A0`–`A12` (A13–A15 go nowhere) and `D0`–`D7`, `RW` low while it writes, `SYNC` high on each opcode fetch; stops into the monitor when `NMI_N` falls
- `U2` monitor: a 6530-002, ROM at $1C00–$1FFF (selected by `K7` on its RS0 pin), RAM at $17C0–$17FF and ports at $1740 (selected by `K5` on its CS1 pin); `PA0`–`PA6` light the segments and read the keypad's columns, `PB1`–`PB4` select a digit or a row of keys through `U24`
- `U3` self-test: a 6530-003, ROM at $1800–$1BFF (selected by `K6`), RAM at $1780–$17BF and ports at $1700 (selected by `K5`; the ports are not used)
- `U4` address decoder: `A10`–`A12` select one 1K block: `K0` low for $0000–$03FF, `K5` for $1400–$17FF, `K6` for $1800–$1BFF, `K7` for $1C00–$1FFF; A13–A15 are not decoded, so everything repeats every 8K
- `U5` RAM: bit 7 of each byte at $0000–$03FF, selected by `K0`, written while `RAM_RW` is low
- `U6` RAM: bit 6, as `U5`
- `U7` RAM: bit 5, as `U5`
- `U8` RAM: bit 4, as `U5`
- `U9` RAM: bit 3, as `U5`
- `U10` RAM: bit 2, as `U5`
- `U11` RAM: bit 1, as `U5`
- `U12` RAM: bit 0, as `U5`
- `U13` RAM buffer: puts `DOUT0`–`DOUT3` on `D0`–`D3` while `BUF_N` is low
- `U14` RAM buffer: puts `DOUT4`–`DOUT7` on `D4`–`D7` while `BUF_N` is low
- `U15` gates: `RAM_RW` low while `PHI2` is high in a write, `BUF_N` low while reading block 0, `SST_N` low on an opcode fetch outside $1C00–$1FFF
- `U16` inverters: `RW_N` and `K0_N`
- `U17` segment driver: `PA0`–`PA6` onto `SEG_A`–`SEG_G`, the segments of every digit
- `U24` display and keypad scanner: from `PB1`–`PB4`, outputs 0–2 pull `ROW0`–`ROW2` low, outputs 4–9 pull `DIG1`–`DIG6` low, one at a time; output 3 (pad 4, `STRAP`) is the self-test strap: joined to `PA0`, reset runs the self-test in `U3` instead of the monitor
- `U25` power-on reset: holds `RES_N` low for 20 ms after power-on, then lets go
- `DS1` digit: the leftmost, lit while `DIG1` is low
- `DS2` digit: lit while `DIG2` is low
- `DS3` digit: lit while `DIG3` is low
- `DS4` digit: lit while `DIG4` is low
- `DS5` digit: lit while `DIG5` is low
- `DS6` digit: the rightmost, lit while `DIG6` is low
- `SW1` key `0`: joins `ROW0` to `PA6`
- `SW2` key `1`: `ROW0` to `PA5`
- `SW3` key `2`: `ROW0` to `PA4`
- `SW4` key `3`: `ROW0` to `PA3`
- `SW5` key `4`: `ROW0` to `PA2`
- `SW6` key `5`: `ROW0` to `PA1`
- `SW7` key `6`: `ROW0` to `PA0`
- `SW8` key `7`: `ROW1` to `PA6`
- `SW9` key `8`: `ROW1` to `PA5`
- `SW10` key `9`: `ROW1` to `PA4`
- `SW11` key `A`: `ROW1` to `PA3`
- `SW12` key `B`: `ROW1` to `PA2`
- `SW13` key `C`: `ROW1` to `PA1`
- `SW14` key `D`: `ROW1` to `PA0`
- `SW15` key `E`: `ROW2` to `PA6`
- `SW16` key `F`: `ROW2` to `PA5`
- `SW17` key AD: `ROW2` to `PA4`
- `SW18` key DA: `ROW2` to `PA3`
- `SW19` key `+`: `ROW2` to `PA2`
- `SW20` key GO: `ROW2` to `PA1`
- `SW21` key PC: `ROW2` to `PA0`
- `SW22` key ST: pulls `NMI_N` low
- `SW23` key RS: pulls `RES_N` low
- `SW24` SST slide switch: at ON, joins `NMI_N` to `SST_N`, so the processor stops after each instruction outside the monitor
- `R1` pull-up on `K0`
- `R2` pull-up on `K5`
- `R3` pull-up on `K6`
- `R4` pull-up on `K7`
- `R5` pull-up on `RES_N`
- `R6` pull-up on `NMI_N`
- `R7` pull-up on `IRQ_N`

## Self-test

The self-test in `U3` checks the memory, the ROMs and the display without using any RAM, so it runs even when the monitor cannot.

- To run it: join `U24` pad 4 (`STRAP`) to `PA0` with a wire (`U2` pad 2 is on `PA0`), then press RS or power on. Remove the wire to return to the monitor. A working monitor also starts it with AD 1800 GO. SST must be at OFF.
- While it runs, digit 1 shows the test: 1 main RAM ($0000–$03FF), 2 the RAM in `U2` ($17C0–$17FF), 3 the RAM in `U3` ($1780–$17BF), 4 the ROM in `U2`, 5 the ROM in `U3`. Then every segment of every digit lights (888888), then each digit alone, then PA55: the keypad test, which shows the code of a held key (0–F the hex keys, 10 AD, 11 DA, 12 `+`, 13 GO, 14 PC) and nothing else while no key is held.
- A failure stops the test and stays on the digits: digit 1 the test, digits 2–4 the last three hex digits of the address, digits 5–6 the bad bits (what was written XOR what was read back). For a ROM, digits 2–4 show its first address and digits 5–6 its sum, which should be 00. For example 123408 is $0234 bit 3, 27D501 is $17D5 bit 0, 4C0001 is the ROM in `U2` summing to 01.
- Main RAM keeps each bit of every byte in one chip: bit 7 in `U5`, bit 6 in `U6`, and so on to bit 0 in `U12`. One bad bit at every address points at one chip; several bits at once point at what they share: the buffers `U13` (bits 0–3) and `U14` (bits 4–7), the gates, the decoder.
