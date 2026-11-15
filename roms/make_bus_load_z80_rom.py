#!/usr/bin/env python3
"""Writes roms/bus_load_z80.rom, the 2716 of netlists/bus_load_z80.bwn (docs/p5s-z80/Z80_DESIGN.md,
Bus load): a ZX80-style display program for the real Z80 on the P5a circuit.

0x0000-0x004A code; 0x0100-0x0425 the RAM image (6 HALTs, the page, 8 HALTs); 0x0600-0x07FF the
P5a glyphs. With --check, the T-state budget is checked and the file on disk compared, nothing
written. Run from the repository root: python3 roms/make_bus_load_z80_rom.py [--check]

The picture: 38 rows of 8 lines, each line 207 T-states, then about 9 lines of VSYNC: 312 lines.
A row is entered by JP (HL) at its first byte in RAM's mirror at $C000 (A15 high): a display
row's 32 characters are fetched with D6 low, so the circuit forces NOPs and shows them; its
NEWLINE ($76) runs as HALT. A border row is a lone HALT. INT_N is on A6, and the CPU looks at it
at the last T-state of each instruction, a fetch's T4, when the refresh address I:R is on the
bus: LD R,A with A = $5D, EI, JP (HL), and the 35th fetch after EI shows R's 7 bits wrapped to
$00 (A6 low). Display line: JP (HL), 32 characters, the HALT, one halted fetch = 35. Border
line: JP (HL), the HALT, 33 halted fetches = 35. The IM 1 handler counts lines (C) and rows (B);
both its paths take 54 T-states to JP (HL): 13 (acknowledge) + 54 + 35 x 4 = 207.
"""

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
sys.dont_write_bytecode = True  # no roms/__pycache__ from the import below
import make_bus_load_rom  # noqa: E402  (the P5a glyphs)

TITLE = " BODGEWIRE P5A  BUS LOAD  SPIKE "
ROWS, COLUMNS = 24, 32
NEWLINE = 0x76  # HALT
TOP_ROWS, BOTTOM_ROWS = 6, 8
LINE, FRAME_LINES, ROW_LINES = 207, 312, 8
R_LOAD = 0x80 - 35
IMAGE = 0x0100
DISPLAY_RAM = 0x4000
DELAY = 120  # DJNZ turns of the VSYNC delay


def page():
    """Z80Stub::Page(): 24 rows of 32 character codes and a NEWLINE."""
    out = []
    for row in range(ROWS):
        for column in range(COLUMNS):
            if row == 0:
                code = ord(TITLE[column]) & 0x3F
            elif row >= 2:
                code = (row * COLUMNS + column) & 0x3F
                code |= 0x80 if (row // 2) % 2 == 1 else 0
            else:
                code = ord(" ") & 0x3F
            out.append(code)
        out.append(NEWLINE)
    return out


def image():
    return [NEWLINE] * TOP_ROWS + page() + [NEWLINE] * BOTTOM_ROWS


# The program: (address, bytes, text, T-states). T-states as "a/b" for taken/not taken.
def program():
    n = len(image())
    frame = 0x0015
    tail = 0x0041
    same = 0x0045
    code = [
        (0x0000, [0xF3], "DI", "4"),
        (0x0001, [0x31, 0x00, 0x44], "LD SP,$4400", "10"),
        (0x0004, [0xED, 0x56], "IM 1", "8"),
        (0x0006, [0x3E, 0x0E], "LD A,$0E", "7"),
        (0x0008, [0xED, 0x47], "LD I,A: refresh A9, A10 high, the glyphs at $0600", "9"),
        (0x000A, [0x21, IMAGE & 0xFF, IMAGE >> 8], "LD HL,image", "10"),
        (0x000D, [0x11, DISPLAY_RAM & 0xFF, DISPLAY_RAM >> 8], "LD DE,$4000", "10"),
        (0x0010, [0x01, n & 0xFF, n >> 8], f"LD BC,{n}", "10"),
        (0x0013, [0xED, 0xB0], "LDIR", "21/16"),
        # frame:
        (frame, [0xDB, 0xFE], "frame: IN A,($FE): VSYNC on", "11"),
        (0x0017, [0x06, DELAY], f"LD B,{DELAY}", "7"),
        (0x0019, [0x10, 0xFE], "DJNZ $", "13/8"),
        (0x001B, [0x1E, 0x00], "LD E,0: padding", "7"),
        (0x001D, [0x1E, 0x00], "LD E,0: padding", "7"),
        (0x001F, [0x00], "NOP: padding", "4"),
        (0x0020, [0xD3, 0xFE], "OUT ($FE),A: VSYNC off", "11"),
        (0x0022, [0x21, 0x00, 0xC0], "LD HL,$C000: the first top-border HALT", "10"),
        (0x0025, [0x01, ROW_LINES, TOP_ROWS + ROWS + BOTTOM_ROWS], "LD BC: B rows, C lines", "10"),
        (0x0028, [0x3E, R_LOAD], f"LD A,${R_LOAD:02X}", "7"),
        (0x002A, [0xCD, tail & 0xFF, tail >> 8], "CALL tail: the frame's lines", "17"),
        (0x002D, [0x18, (frame - 0x002F) & 0xFF], "JR frame", "12"),
        # IM 1 handler: the pushed PC is the byte after the HALT, the next row's start.
        (0x0038, [0x0D], "DEC C: lines left in the row", "4"),
        (0x0039, [0xC2, same & 0xFF, same >> 8], "JP NZ,same", "10"),
        (0x003C, [0xE1], "POP HL: the next row", "10"),
        (0x003D, [0x05], "DEC B", "4"),
        (0x003E, [0xC8], "RET Z: the frame done, back to frame's JR", "11/5"),
        (0x003F, [0xCB, 0xD9], "SET 3,C: 8 lines", "8"),
        (tail, [0xED, 0x4F], "tail: LD R,A", "9"),
        (0x0043, [0xFB], "EI", "4"),
        (0x0044, [0xE9], "JP (HL)", "4"),
        (same, [0xD1], "same: POP DE: the pushed PC dropped", "10"),
        (0x0046, [0xC8], "RET Z: never taken (Z clear), 5 T-states", "11/5"),
        (0x0047, [0x18, (tail - 0x0049) & 0xFF], "JR tail", "12"),
    ]
    for (at, data, _, _), nxt in zip(code, code[1:]):
        if nxt[0] != at + len(data) and nxt[0] not in (0x0038,):
            raise SystemExit(f"program: {at:04X} + {len(data)} is not {nxt[0]:04X}")
    if code[[c[0] for c in code].index(0x002D)][0] + 2 > 0x0038:
        raise SystemExit("program: the frame loop runs into the handler")
    return code


def t(code, address, taken=True):
    for at, _, _, states in code:
        if at == address:
            parts = states.split("/")
            return int(parts[0] if taken or len(parts) == 1 else parts[1])
    raise SystemExit(f"no instruction at {address:04X}")


def check(code):
    ack, fetches = 13, 35 * 4
    tail = t(code, 0x0041) + t(code, 0x0043)  # LD R,A, EI
    same = t(code, 0x0038) + t(code, 0x0039) + t(code, 0x0045) + t(code, 0x0046, False) + t(
        code, 0x0047) + tail
    row = (t(code, 0x0038) + t(code, 0x0039) + t(code, 0x003C) + t(code, 0x003D) +
           t(code, 0x003E, False) + t(code, 0x003F) + tail)
    for name, path in (("same row", same), ("next row", row)):
        if ack + path + fetches != LINE:
            raise SystemExit(f"{name}: a line of {ack + path + fetches} T-states, not {LINE}")
    end = (t(code, 0x0038) + t(code, 0x0039) + t(code, 0x003C) + t(code, 0x003D) +
           t(code, 0x003E) + t(code, 0x002D))  # through RET Z to JR frame
    vsync = (t(code, 0x0015) + t(code, 0x0017) + DELAY * t(code, 0x0019) - 5 + t(code, 0x001B) +
             t(code, 0x001D) + t(code, 0x001F) + t(code, 0x0020) + t(code, 0x0022) +
             t(code, 0x0025) + t(code, 0x0028) + t(code, 0x002A))
    across = ack + end + vsync + tail + fetches
    rows = TOP_ROWS + ROWS + BOTTOM_ROWS
    total = (rows * ROW_LINES - 1) * LINE + across
    if total != FRAME_LINES * LINE:
        raise SystemExit(f"frame: {total} T-states, not {FRAME_LINES * LINE}")
    if (R_LOAD + 35) & 0x7F != 0 or any(((R_LOAD + k) & 0x40) == 0 for k in range(1, 35)):
        raise SystemExit("R: A6 must stay high until the 35th fetch after EI")
    if (TOP_ROWS * ROW_LINES) % 8 != 0:
        raise SystemExit("top border: the line counter would not be 0 on row 0")
    return across


def text():
    code = program()
    across = check(code)
    lines = [
        "; Bus load ROM for the real Z80 (2716), docs/p5s-z80/Z80_DESIGN.md. Generated by",
        "; roms/make_bus_load_z80_rom.py: do not edit, rerun the script.",
        "; 0x000-0x04A code; 0x100-0x425 the RAM image copied to $4000 (6 HALTs, the page, 8",
        "; HALTs); 0x600-0x7FF the P5a glyphs (address 0x600 + code x 8 + line).",
        f"; Lines of {LINE} T-states; the line across VSYNC {across}; {FRAME_LINES} lines a frame.",
        "",
    ]
    last = None
    for at, data, what, states in code:
        if last is None or at != last:
            lines.append(f"@{at:03X}")
        lines.append(f"{' '.join(f'{b:02X}' for b in data):<9}; {what} ({states})")
        last = at + len(data)
    lines += ["", f"@{IMAGE:03X}"]
    data = image()
    for i in range(0, len(data), 16):
        lines.append(" ".join(f"{b:02X}" for b in data[i : i + 16]))
    lines += ["", "@600"]
    rows = make_bus_load_rom.glyph_rows()
    for code_ in range(64):
        lines.append(f"; 0x{code_:02X}")
        for row in rows[code_ * 8 : code_ * 8 + 8]:
            lines.append("." + row + "..")
    return "\n".join(lines) + "\n"


def main():
    out = ROOT / "bus_load_z80.rom"
    content = text()
    if "--check" in sys.argv[1:]:
        if not out.exists() or out.read_text() != content:
            raise SystemExit(f"{out.name}: out of date, rerun the script")
        print("bus_load_z80.rom: up to date, budget checked")
        return
    out.write_text(content)


if __name__ == "__main__":
    main()
