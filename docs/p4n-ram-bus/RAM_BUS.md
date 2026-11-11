# P4n — RAM and bus

Roadmap step P4n ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 090 TV typewriter needs, tested without a Board. The Machine itself is P4o.

Decided with the user (2026-11-11):

- The RAM is a **2114** (1K × 4 static RAM, shared I/O pins, DIP-18). Two of them make one 8-bit word.
- The bus driver is a **74LS244** (octal tri-state buffer, two groups of four with their own enable, one way, DIP-20).
- The **ASCII keyboard** sends a key as 7 bits followed by a fixed high **STROBE** pulse, the pulse length a Part parameter, like the Apple 1 keyboard. Letters are folded to upper case; Return sends CR, Backspace BS.
- **Keyboard capture** in `bw_ui`: a Type toggle in a Keyboard section of the side panel, also on F2. While it is on, every key goes to the keyboard and the banner says so; Escape turns it off and is not sent. `bw run` takes `--type REF:START:TEXT`, one key every 100 ms from `START`.

Decided while designing (2026-11-11), for the user to overrule:

- **Bidirectional pin.** A new pin kind, read as an input and driven as a tri-state output. At power-on it starts released (Z), so nothing fights on a bus before the chip's first evaluation. It is marked as a Driver when its Net is hovered.
- **Pin faults on it.** A stuck bidirectional pin drives its value onto its Net (like a stuck output) and the chip reads that value (like a stuck input). A dry Joint takes it off its Net both ways: it drives nothing there and reads floating.
- **The 2114 is MOS.** Its pins are MOS family: an undriven data bus keeps its last driven value (bus charge), an X at any of its inputs is read as a seeded random bit (ARCHITECTURE.md, Logic values), and a fight between a 2114 and a 74LS244 gives X, not TTL's 0.
- **2114 timing** from Intel's 2114 (450 ns grade) maximums, the only figures given: address to output 450 ns, `CS_N` to output 120 ns (also used after `WE_N` goes back high), outputs off 100 ns. A write lasts while `CS_N` and `WE_N` are both low and keeps the address and data held just before it ends (no hold time). The contents are random at power-on.
- **The keyboard connector** is `KEYBOARD`, a 10-pin KK header: `D0`–`D6` on pins 1–7, `STROBE` 8, `VCC` 9, `GND` 10, Powered from the Board like the paddle controller. `strobe=` is optional, 10 µs by default, at least 1 µs. A key's code is on `D0`–`D6` 100 ns after the key is taken; `STROBE` rises 1 µs later, stays high for `strobe`, and the next key is taken no sooner than 1 µs after it falls. The code stays on `D0`–`D6` until the next key. Up to 32 keys wait in turn; more are dropped. `STROBE` is low at power-on, `D0`–`D6` random.
- **Keys sent**: codes 0x20–0x5F as typed, letters `a`–`z` folded to `A`–`Z`, Return 0x0D, Backspace 0x08. Every other key (`` ` `` `{` `|` `}` `~`, Tab, arrows, non-ASCII) is not sent. Typing does nothing with the power off, as with buttons; keys still waiting at power-off are dropped.
- **`--type`** in `bw_ui` too (as `--press`), so the UI check can show keys sent. In the text, `\n` is Return, `\b` Backspace, `\\` a backslash.
- **The side panel's Keyboard section** shows the last 16 keys sent (`[CR]`, `[BS]` for the two controls).
- **DIP-18 and DIP-20** join DIP-14 and DIP-16 in the chip grid of `bw place` (a DIP-20 fits the 27 mm row pitch), so the 090 Board needs no special rules for them.
- **Bench netlist** `tests/data/ram_bench.bwn`, no Board: keyboard → 74LS244 → a 7-bit bus ↔ two 2114s; each key is written at the address of a 74LS161 that steps after the key; a button clears it (before typing, as it powers on at a random count, and after, to read address 0 back).

## New code

1. **Bidirectional pins.** `PinKind::Bidirectional`: the pin is in its Net's Drivers (tri-state, as an output) and in its fan-out (as an input), so a chip reads the bus it drives. The builder takes a stuck fault on it as above and warns about an unconnected TTL one as about a TTL input; the Puzzle checks accept it for `stuck` and `joint`; `DescribeNet` lists it as a Driver.
2. **2114 static RAM.** `part U2 2114`, DIP-18, the datasheet pinout (`A6` `A5` `A4` `A3` `A0` `A1` `A2` `CS_N` `GND` `WE_N` `IO4` `IO3` `IO2` `IO1` `A9` `A8` `A7` `VCC`). `CS_N` low and `WE_N` high: `IO1`–`IO4` drive the word at `A0`–`A9`. `CS_N` and `WE_N` low: the word is written from `IO1`–`IO4`, which float. `CS_N` high: they float. Datasheet `2114.md`.
3. **74LS244 octal buffer.** `part U1 74LS244`, DIP-20 (`1OE_N` `1A1` `2Y4` `1A2` `2Y3` `1A3` `2Y2` `1A4` `2Y1` `GND` `2A1` `1Y4` `2A2` `1Y3` `2A3` `1Y2` `2A4` `1Y1` `2OE_N` `VCC`). Each `Y` follows its `A` while its group's `OE_N` is low, and floats while it is high. TI typicals: data 12 ns; enabled 15 ns to 1, 20 ns to 0; disabled 10 ns from 1, 15 ns from 0. Datasheet `74LS244.md`.
4. **DIP-18 and DIP-20 packages.** The KiCad footprints vendored in `boards/footprints/`; grid packages in `bw place`.
5. **ASCII keyboard.** `part J1 KEYBOARD strobe=10us`, the behaviour above; `KeyboardCode` maps a typed character to the code sent. Datasheet `KEYBOARD.md`.
6. **Typing through the Session.** `keyboards()`, `type(reference, key)` (wakes the keyboard), `typed(reference)`; `--type REF:START:TEXT` in `bw run` and `bw_ui`.
7. **Keyboard capture in `bw_ui`.** The Keyboard section (Type toggle, the keys sent), F2 to start, Escape to stop; while on, key presses and text go to the keyboard and not to ImGui (no tool keys, no knob keys); the banner reads "Typing on J1 · Esc to stop".

Nothing else is new: the kernel's resolution rules, the TV, the instruments and the Board pipeline are unchanged. The watch budget is unchanged.

## Tests

- Bidirectional pins: in a Net's Drivers and fan-out; drives like a tri-state output; starts Z; a Component wakes when its own bidirectional pin changes the Net; contention MOS against TTL gives X; a stuck one drives its Net and is read by its chip; an unconnected one reads floating; `DescribeNet` lists it as a Driver.
- 2114: catalogue row and pins (power at 9 and 18); read after the address, `CS_N` and `WE_N` delays; outputs float on `CS_N` high and during a write; a write keeps the data and address held before `WE_N` rises, also when the address changes at that same timestamp; data changed during a write keeps the last; X on an address or data bit read as a random bit; random contents at power-on, different seeds differ.
- 74LS244: catalogue row and pins (power at 10 and 20); each `Y` follows its `A`; each group's enable alone; the six delays; X on `OE_N` gives X; DIP-20 placed in the grid by `bw place`.
- Keyboard: catalogue row, pins and kinds; `strobe=` default, accepted, refused (bad, under 1 µs, unknown parameter); `KeyboardCode` for each class of key; a typed key's code at 100 ns, `STROBE` high from 1.1 µs for `strobe`; keys typed together come one after the other; the 33rd waiting key is refused; power-off drops waiting keys; Powered through its Connector.
- `ParseTyping`: start, text, escapes; refused (no colons, bad duration, empty text, a key not sent).
- Session: `keyboards()`, `type` refused for an unknown reference or with the power off, `typed` shows the last 16 keys.
- Bench (`tests/data/ram_bench.bwn`): loads without warnings; after the reset button, "HELLO" typed is in the RAMs at addresses 0–4; after the reset button the bus reads `H`; no X on the bus once a key is written and read back. `bw run --type` on the bench runs in the release check.
- UI check (release): `bw_ui tests/data/ram_bench.bwn --type J1:100ms:HELLO --screenshot …` shows the Keyboard section with `HELLO`; capture itself is a user check (Type, a few keys, the banner, `U` not opening the manual while typing, Escape).

Design: [RAM_BUS_DESIGN.md](RAM_BUS_DESIGN.md). Plan: [RAM_BUS_PLAN.md](RAM_BUS_PLAN.md).
