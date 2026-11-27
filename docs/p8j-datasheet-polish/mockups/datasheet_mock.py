"""P8j mock-ups (docs/p8j-datasheet-polish/DATASHEET_POLISH.md): a chip's Datasheets page and a
section hovered on the Board, drawn as SVG, not by the game, at the game's sizes (the page's 16 px
Atkinson Hyperlegible, rows kControl 28 px high, the pinout's body 34 px). `python3 -I
datasheet_mock.py` writes the SVGs beside it. Grey notes in the margin are the mock-up's, not the
game's."""
import os

OUT = os.path.dirname(os.path.abspath(__file__))
SANS = "Atkinson Hyperlegible, Helvetica, Arial, sans-serif"
MONO = "Atkinson Hyperlegible Mono, Menlo, monospace"

# The binder's colours (src/ui/manual_window.cpp, src/ui/manual_view.hpp).
PAPER = "#f6f2e4"
INK = "#1e1e23"
LINK = "#1950af"
CODE = "#965a0a"
POWER = "#807a70"  # chip_panel.cpp kPowerText
FAINT = "#a8a294"  # a pin that goes nowhere
KEY = "#e0d2ae"  # a page's button
BODY = "#3c3c42"
NOTE = "#8a8a8a"
# Proposed: one colour for every pin symbol, on the page and on the Board (none of the Board's
# meanings: levels red / blue / yellow, the hovered Net yellow, the clips magenta and blue).
SYM = "#8a4fd8"
GROUP = "#ece2fa"  # a hovered pin's group, on the page
HOVER = "#dccaf6"  # the hovered row
NET_FILL, NET_EDGE = "#e9e3d0", "#b8ad8e"

ROW, ROW_GAP = 28, 6
PAGE_W = 520
STUB = 20  # a pin's line between its number and the body
BODY_W = 34
SHOW = 28  # the crosshair key


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=16, fill=INK, anchor="start", weight="normal", family=SANS, extra=""):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{family}" font-size="{size}" '
            f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}" {extra}>{esc(s)}</text>')


def rich(x, y, runs, size=16):
    """One line of mixed runs: (text, kind) with kind plain | ref | part | pin | net | bold."""
    out = [f'<text x="{x:.1f}" y="{y:.1f}" font-family="{SANS}" font-size="{size}" fill="{INK}" '
           f'xml:space="preserve">']
    for s, kind in runs:
        if kind == "ref":
            out.append(f'<tspan fill="{LINK}" text-decoration="underline">{esc(s)}</tspan>')
        elif kind == "part":
            out.append(f'<tspan font-weight="bold">{esc(s)}</tspan>')
        elif kind == "pin":
            out.append(f'<tspan font-family="{MONO}" fill="{CODE}" text-decoration="underline" '
                       f'text-decoration-style="dotted">{esc(s)}</tspan>')
        elif kind == "code":
            out.append(f'<tspan font-family="{MONO}" fill="{CODE}">{esc(s)}</tspan>')
        elif kind == "net":
            out.append(f'<tspan fill="{LINK}" font-style="italic">{esc(s)}</tspan>')
        elif kind == "bold":
            out.append(f'<tspan font-weight="bold">{esc(s)}</tspan>')
        else:
            out.append(esc(s))
    out.append("</text>")
    return "".join(out)


def rect(x, y, w, h, fill, rx=0, stroke=None, sw=1, extra=""):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    return (f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" '
            f'fill="{fill}"{s} {extra}/>')


def line(x1, y1, x2, y2, stroke, sw=1.5, extra=""):
    return (f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" '
            f'stroke-width="{sw}" {extra}/>')


def poly(points, fill, stroke="none", sw=1.5):
    p = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return f'<polygon points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'


def head(x, y, d, colour, size=5.5):
    """An arrowhead at (x, y) pointing along d (+1 right, -1 left; 'u'/'d' up/down)."""
    if d in (1, -1):
        return poly([(x + d * size, y), (x - d * size * 0.2, y - size), (x - d * size * 0.2, y + size)],
                    colour)
    s = -1 if d == "u" else 1
    return poly([(x, y + s * size), (x - size, y - s * size * 0.2), (x + size, y - s * size * 0.2)],
                colour)


def crosshair(cx, cy, r=6, colour=INK):
    out = [f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{colour}" stroke-width="1.5"/>']
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        out.append(line(cx + dx * r * 0.45, cy + dy * r * 0.45, cx + dx * r * 1.45, cy + dy * r * 1.45,
                        colour))
    return "".join(out)


def pulse(x, y, colour):
    """A small clock pulse, centred on (x, y)."""
    pts = [(x - 6, y + 3), (x - 3, y + 3), (x - 3, y - 3), (x + 3, y - 3), (x + 3, y + 3), (x + 6, y + 3)]
    p = " ".join(f"{a:.1f},{b:.1f}" for a, b in pts)
    return f'<polyline points="{p}" fill="none" stroke="{colour}" stroke-width="1.5"/>'


def stub(x_body, y, side, kind, low=False, clock=False):
    """A pin's line from the body's edge `x_body` outwards (side -1 left, +1 right) with its kind's
    symbol: in, out, both, power, nowhere-in, nowhere-out."""
    x_out = x_body + side * STUB
    out = []
    if kind == "power":
        return line(x_body, y, x_out, y, POWER, 2)
    dashed = 'stroke-dasharray="3 3"' if kind.startswith("nowhere") else ""
    colour = SYM
    x_in = x_body + side * (8 if low else 0)  # the bubble takes the body end
    out.append(line(x_in, y, x_out, y, colour, 1.5, dashed))
    mid = (x_in + x_out) / 2
    if kind in ("in", "nowhere-in"):
        out.append(head(mid - side * 1, y, -side, colour))
    elif kind in ("out", "nowhere-out"):
        out.append(head(mid + side * 1, y, side, colour))
    elif kind == "both":
        out.append(head(x_in + side * 5, y, -side, colour))
        out.append(head(x_out - side * 5, y, side, colour))
    if low:
        out.append(f'<circle cx="{x_body + side * 4:.1f}" cy="{y}" r="3.6" fill="{PAPER}" '
                   f'stroke="{colour}" stroke-width="1.5"/>')
    if clock:
        out.append(pulse(mid, y - 8, colour))
    return "".join(out)


def net_tag(x, y, s, anchor):
    """A Net's name, further from the body than the pin's, in its own look (italic link blue)."""
    return text(x, y, s, 16, LINK, anchor, extra='font-style="italic"')


def pinout(x0, y0, rows, tint=None):
    """A DIP's pinout at (x0, y0), PAGE_W - 2 * 20 wide. rows: (number, name, net, kind, low, clock)
    pairs, left then right, pin 1 first. tint: {number: colour} for a hovered group."""
    tint = tint or {}
    inner = PAGE_W - 40
    body_x = x0 + inner / 2 - BODY_W / 2
    out = []
    step = ROW + ROW_GAP
    n = len(rows)
    out.append(rect(body_x, y0 - 4, BODY_W, n * step + 2, BODY, 2))
    out.append(f'<circle cx="{body_x + BODY_W / 2}" cy="{y0 - 4}" r="{BODY_W / 5}" fill="#19191c"/>')
    out.append(f'<circle cx="{body_x + 6}" cy="{y0 + ROW / 2}" r="3" fill="#dcdcdc"/>')
    for r, (left, right) in enumerate(rows):
        y = y0 + r * step
        cy = y + ROW / 2
        base = cy + 5.5
        for side, pin in ((-1, left), (1, right)):
            number, name, net, kind, low, clock = pin
            edge = body_x if side < 0 else body_x + BODY_W
            cell_x = x0 + SHOW + 4 if side < 0 else edge + STUB
            cell_w = (edge - STUB) - cell_x if side < 0 else (x0 + inner - SHOW - 4) - cell_x
            if number in tint:
                out.append(rect(cell_x, y, cell_w + STUB, ROW, tint[number], 3) if side < 0 else
                           rect(edge, y, cell_w + STUB, ROW, tint[number], 3))
            out.append(stub(edge, cy, side, kind, low, clock))
            ink = POWER if kind == "power" else FAINT if kind.startswith("nowhere") else INK
            num_x = edge - STUB - 4 if side < 0 else edge + STUB + 4
            anchor = "end" if side < 0 else "start"
            out.append(text(num_x, base, str(number), 15, POWER if kind == "power" else NOTE, anchor))
            name_x = num_x + side * 26
            out.append(text(name_x, base, name, 16, ink, anchor, family=MONO))
            if net:
                name_w = len(name) * 9.6
                out.append(net_tag(name_x + side * (name_w + 14), base, net, anchor))
            key_x = x0 if side < 0 else x0 + inner - SHOW
            out.append(rect(key_x, y, SHOW, ROW, KEY, 2))
            out.append(crosshair(key_x + SHOW / 2, cy))
    return "".join(out), n * step


def note(x, y, lines, anchor="start"):
    return "".join(text(x, y + i * 17, s, 13, NOTE, anchor) for i, s in enumerate(lines))


def arrow_note(x1, y1, x2, y2):
    return line(x1, y1, x2, y2, NOTE, 1, 'stroke-dasharray="2 3"')


def svg(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="0 0 {w} {h}">{rect(0, 0, w, h, "#2a2d33")}{body}</svg>\n')


def key(x, y, label, w):
    return rect(x, y, w, ROW, KEY, 3) + text(x + w / 2, y + 19, label, 16, INK, "middle")


def list_icon(x, y):
    """☰ drawn: three short lines, a dot before each (a list)."""
    out = []
    for i in range(3):
        yy = y + 9 + i * 5
        out.append(f'<circle cx="{x + 9}" cy="{yy}" r="1.3" fill="{INK}"/>')
        out.append(line(x + 12.5, yy, x + 20, yy, INK, 1.6))
    return "".join(out)


def page_top(px, y):
    """Back and the list, the page's only navigation."""
    out = [rect(px, y, 74, ROW, KEY, 3), head(px + 14, y + ROW / 2, -1, INK, 5),
           text(px + 24, y + 19, "Back", 16)]
    out.append(rect(px + 82, y, 118, ROW, KEY, 3))
    out.append(list_icon(px + 84, y + 2))
    out.append(text(px + 108, y + 19, "All chips", 16))
    return "".join(out)


# --- U5 on the Counter, at rest ---------------------------------------------------------------
U5 = [
    ((1, "CLR_N", "RESET_N", "in", True, False), (16, "VCC", "", "power", False, False)),
    ((2, "CLK", "CLK", "in", False, True), (15, "RCO", "", "nowhere-out", False, False)),
    ((3, "A", "GND", "in", False, False), (14, "QA", "", "nowhere-out", False, False)),
    ((4, "B", "GND", "in", False, False), (13, "QB", "", "nowhere-out", False, False)),
    ((5, "C", "GND", "in", False, False), (12, "QC", "", "nowhere-out", False, False)),
    ((6, "D", "GND", "in", False, False), (11, "QD", "C19", "out", False, False)),
    ((7, "ENP", "VCC", "in", False, False), (10, "ENT", "CARRY4", "in", False, False)),
    ((8, "GND", "", "power", False, False), (9, "LOAD_N", "VCC", "in", True, False)),
]


def u5_page():
    mx = 20  # the bench around the page
    px = mx + 20  # the page's text margin
    w = mx + PAGE_W + 330
    out = [rect(mx, 20, PAGE_W, 1180, PAPER, 2)]
    y = 36
    out.append(page_top(px, y))
    out.append(note(mx + PAGE_W + 20, y + 12, ["Back and the list: the page's only", "navigation (§ Back, § list icon)"]))
    y += 62
    out.append(rich(px, y, [("U5", "ref"), (" · ", "plain"), ("74LS161", "part"),
                            (" · synchronous 4-bit", "bold")], 22))
    out.append(rich(px, y + 28, [("binary counter", "bold")], 22))
    out.append(line(px, y + 42, mx + PAGE_W - 20, y + 42, "#cfc6ad", 1.5))
    out.append(note(mx + PAGE_W + 20, y - 4, ["The title is the chip: U5 a link", "(hover lights it, click centres the",
                                              "Board on it); \"Zoom to\" is gone"]))
    y += 72
    out.append(rich(px, y, [("Counter bits 16–19: counts when ", "plain"), ("U4", "ref"),
                            (" carries;", "plain")]))
    out.append(rich(px, y + 24, [("C19", "net"), (" drives ", "plain"), ("D1", "ref"), (".", "plain")]))
    out.append(note(mx + PAGE_W + 20, y - 4, ["Its role, from the Service Manual:", "a sentence, its references links"]))
    y += 52
    body, h = pinout(px, y, U5)
    out.append(body)
    nx = mx + PAGE_W + 20
    out.append(note(nx, y + 4, ["Pin names in mono next to the body;", "Nets in italic blue, further out"]))
    out.append(note(nx, y + 50, ["Symbols, one colour:", "  ▶ into the body  in", "  ◀ out of it  out",
                                 "  ◀▶  both ways", "  ○ at the body  active low (_N kept)",
                                 "  ⎍ a clock", "  grey, no symbol  power"]))
    out.append(note(nx, y + 180, ["RCO, QA, QB, QC go nowhere: dashed", "line, faint name, no Net (C16–C18", "removed from the netlist)"]))
    out.append(note(nx, y + 238, ["LOAD_N and ENP are tied to VCC:", "black, not grey (grey is the kind,", "not the Net)"]))
    y += h + 26
    out.append(text(px, y, "Function", 19, weight="bold"))
    out.append(note(nx, y - 4, ["No folds: the pinout, Function and", "Pins all open"]))
    y += 30
    func = [
        [("A counter that steps 0, 1, 2, … 15, 0 on each", "plain")],
        [("rising edge of ", "plain"), ("CLK", "pin"), (". Its value appears on ", "plain"), ("QA", "pin")],
        [("(bit 0, toggles fastest) to ", "plain"), ("QD", "pin"), (" (bit 3, slowest).", "plain")],
        [("Each output runs at half the frequency of the one", "plain")],
        [("before: with a 1 MHz clock, ", "plain"), ("QA", "pin"), (" is 500 kHz, ", "plain"),
         ("QB", "pin")],
        [("250 kHz, ", "plain"), ("QC", "pin"), (" 125 kHz, ", "plain"), ("QD", "pin"), (" 62.5 kHz.", "plain")],
    ]
    for i, runs in enumerate(func):
        out.append(rich(px, y + i * 24, runs))
    out.append(note(nx, y + 4, ["The sheet's pin names, dotted: hover", "lights the pad (and its Net) on the",
                                "Board and the row above; a click", "acts as on the row"]))
    y += len(func) * 24 + 14
    out.append(rich(px, y, [("On each rising clock edge, in this order of priority:", "plain")]))
    y += 28
    bullets = [
        [("CLR_N", "pin"), (" low: the count is 0 at once", "plain")],
        [("LOAD_N", "pin"), (" low: the count becomes ", "plain"), ("A", "pin"), ("–", "plain"),
         ("D", "pin")],
        [("ENP", "pin"), (" and ", "plain"), ("ENT", "pin"), (" both high: up by one", "plain")],
        [("otherwise: the count holds", "plain")],
    ]
    for runs in bullets:
        out.append(f'<circle cx="{px + 8}" cy="{y - 5}" r="2.5" fill="{INK}"/>')
        out.append(rich(px + 20, y, runs))
        y += 26
    y += 22
    out.append(text(px, y, "Pins", 19, weight="bold"))
    y += 30
    pins = [
        [("CLK", "pin"), (": clock; the count changes on its rising edge", "plain")],
        [("CLR_N", "pin"), (": clear, active low", "plain")],
        [("LOAD_N", "pin"), (": load ", "plain"), ("A", "pin"), ("–", "plain"), ("D", "pin"),
         (" on the next clock, active low", "plain")],
        [("A", "pin"), ("–", "plain"), ("D", "pin"), (": value to load", "plain")],
        [("ENP", "pin"), (", ", "plain"), ("ENT", "pin"), (": count enables; both high to count", "plain")],
        [("QA", "pin"), ("–", "plain"), ("QD", "pin"), (": the count, ", "plain"), ("QA", "pin"),
         (" lowest bit", "plain")],
        [("RCO", "pin"), (": ripple carry out, high at count 15", "plain")],
        [("VCC", "pin"), (": +5 V", "plain")],
        [("GND", "pin"), (": 0 V", "plain")],
    ]
    for runs in pins:
        out.append(f'<circle cx="{px + 8}" cy="{y - 5}" r="2.5" fill="{INK}"/>')
        out.append(rich(px + 20, y, runs))
        y += 26
    h = y + 20
    out[0] = rect(mx, 20, PAGE_W, h - 30, PAPER, 2)
    return svg(w, h, "".join(out))


# --- U3 (74LS00) on the Dice, 1Y hovered: its group on the page and on the Board ---------------
U3 = [
    ((1, "1A", "N1", "in", False, False), (14, "VCC", "", "power", False, False)),
    ((2, "1B", "N2", "in", False, False), (13, "4B", "GND", "in", False, False)),
    ((3, "1Y", "SIX_N", "out", False, False), (12, "4A", "GND", "in", False, False)),
    ((4, "2A", "GND", "in", False, False), (11, "4Y", "", "nowhere-out", False, False)),
    ((5, "2B", "GND", "in", False, False), (10, "3B", "GND", "in", False, False)),
    ((6, "2Y", "", "nowhere-out", False, False), (9, "3A", "GND", "in", False, False)),
    ((7, "GND", "", "power", False, False), (8, "3Y", "", "nowhere-out", False, False)),
]

GREEN = "#0b3a25"
TRACE = "#3d9a3a"
TRACE_LIT = "#f0c020"
PAD = "#d8d6c8"
LEVEL = {"high": "#d23a2a", "low": "#3a8ad8", "busy": "#d8a020", None: PAD}


def board_pad(cx, cy, level, hovered=False):
    out = [f'<circle cx="{cx}" cy="{cy}" r="12" fill="{LEVEL[level]}"/>',
           f'<circle cx="{cx}" cy="{cy}" r="6" fill="#1a1a1a"/>']
    if hovered:
        out.append(f'<circle cx="{cx}" cy="{cy}" r="15" fill="none" stroke="#ffffff" stroke-width="2"/>')
    return "".join(out)


def board_label(cx, y, s, mark=None, up=True):
    """A pin label's black box under (up=False: over) its pad; a section's mark beside the label,
    on the pad's side: an arrow towards the chip (in) or away (out), a pulse (clock)."""
    w = len(s) * 8.4 + 8
    out = [rect(cx - w / 2, y, w, 18, "#0c0c0c", 3), text(cx, y + 14, s, 13, "#ffffff", "middle", family=MONO)]
    if mark:
        mx, my = cx + w / 2 + 9, y + 9
        if mark == "in":
            out.append(line(mx, my + 7, mx, my - 5, SYM, 2))
            out.append(head(mx, my - 6, "u" if up else "d", SYM, 5))
        elif mark == "out":
            out.append(line(mx, my - 7, mx, my + 5, SYM, 2))
            out.append(head(mx, my + 6, "d" if up else "u", SYM, 5))
    return "".join(out)


def u3_hover():
    mx = 20
    px = mx + 20
    out = []
    tint = {1: GROUP, 2: GROUP, 3: HOVER}
    page_h = 7 * (ROW + ROW_GAP) + 120
    out.append(rect(mx, 20, PAGE_W, page_h, PAPER, 2))
    y = 52
    out.append(rich(px, y, [("U3", "ref"), (" · ", "plain"), ("74LS00", "part"),
                            (" · quad 2-input NAND", "bold")], 22))
    out.append(line(px, y + 14, mx + PAGE_W - 20, y + 14, "#cfc6ad", 1.5))
    y += 46
    body, h = pinout(px, y, U3, tint)
    out.append(body)
    # the pointer on row 3
    cx, cy = px + 150, y + 2 * (ROW + ROW_GAP) + 18
    out.append(poly([(cx, cy), (cx, cy + 17), (cx + 4.5, cy + 13), (cx + 8, cy + 20), (cx + 10.5, cy + 19),
                     (cx + 7, cy + 12), (cx + 12, cy + 12)], "#ffffff", "#000000", 1))
    out.append(note(mx, 20 + page_h + 22, ["The page: 1Y hovered; its group (1A, 1B) tinted. On a",
                                           "74LS273, CLK hovered would tint all eight sections;",
                                           "3Q hovered: 3D, CLK and CLR_N, all alike."]))
    # The Board, as the page's hover shows it (the same when the pad itself is hovered).
    bx, by, bw, bh = mx + PAGE_W + 30, 20, 520, page_h
    out.append(rect(bx, by, bw, bh, GREEN, 2))
    pitch = 54
    x1 = bx + 70
    top, bottom = by + 120, by + 230
    # the hovered Net, SIX_N: from pin 3 down and away, lit
    p3 = x1 + 2 * pitch
    out.append(f'<polyline points="{p3},{bottom} {p3},{bottom + 70} {bx + bw - 30},{bottom + 70}" '
               f'fill="none" stroke="{TRACE_LIT}" stroke-width="5"/>')
    # other traces, dim
    for k, dx in ((0, -1), (1, 0)):
        xx = x1 + k * pitch
        out.append(f'<polyline points="{xx},{bottom} {xx},{bottom + 40 + k * 14} {bx + 20},{bottom + 40 + k * 14}" '
                   f'fill="none" stroke="{TRACE}" stroke-width="4" opacity="0.8"/>')
    # the chip's courtyard (its body hidden under a hover)
    out.append(rect(x1 - 30, top - 22, 6 * pitch + 60, bottom - top + 44, "none", 6, "#ffffff", 1.5,
                    'opacity="0.55"'))
    out.append(text(x1 - 40, (top + bottom) / 2 + 5, "U3", 15, "#d0d0d0", "end"))
    levels_bottom = ["busy", "busy", "busy", "low", "low", None, "low"]
    names_bottom = ["1A", "1B", "1Y", "2A", "2B", "2Y", "GND"]
    marks = {0: "in", 1: "in", 2: "out"}
    for i in range(7):
        xx = x1 + i * pitch
        out.append(board_pad(xx, bottom, levels_bottom[i], hovered=(i == 2)))
        out.append(board_label(xx, bottom + 18, names_bottom[i], marks.get(i), up=True))
    names_top = ["VCC", "4B", "4A", "4Y", "3B", "3A", "3Y"]
    levels_top = ["high", "low", "low", None, "low", "low", None]
    for i in range(7):
        xx = x1 + i * pitch
        out.append(board_pad(xx, top, levels_top[i]))
        out.append(board_label(xx, top - 38, names_top[i], None, up=False))
    # the tooltip, as today's but with the title's separator
    tx, ty = p3 + 22, bottom - 86
    out.append(rect(tx, ty, 230, 44, "#1c1c1c", 4, "#555555", 1))
    out.append(text(tx + 10, ty + 18, "U3 · pin 3 · 1Y · SIX_N", 14, "#eeeeee"))
    out.append(text(tx + 10, ty + 36, "1Y–4Y: outputs", 14, "#bbbbbb"))
    out.append(note(bx, by + bh + 22, ["The Board: SIX_N lit as today; 1A and 1B marked by their labels",
                                       "(▲ into the chip, ▼ out of it), small, never the pad's outline;",
                                       "hidden when the pads are too small. Same when the pad 1Y is",
                                       "hovered on the Board, or held by a finger."]))
    w = bx + bw + 20
    return svg(w, 20 + page_h + 100, "".join(out))


def main():
    for name, content in (("u5_page.svg", u5_page()), ("u3_hover.svg", u3_hover())):
        with open(os.path.join(OUT, name), "w") as f:
            f.write(content)
        print(name)


if __name__ == "__main__":
    main()
