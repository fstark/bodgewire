"""P8i mock-ups (docs/p8i-logic-analyzer/LOGIC_ANALYZER.md): the Logic Analyzer drawn as SVG, not
by the game, at the game's sizes (keys 36 x 44, the scope's case). `python3 -I analyzer_mock.py`
writes the SVGs beside it and prints the strip's widths."""
import os

OUT = os.path.dirname(os.path.abspath(__file__))
FONT = "Menlo, monospace"
SANS = "Helvetica, Arial, sans-serif"
COL = ["#ffe600", "#00e6ff", "#ff3cdc", "#ff8c00", "#8cff64", "#6ea0ff", "#ff96b4", "#ffffff"]
PANEL = "#dedbd2"
FRAME = "#3c3e42"
GROUP = "#cfd5dc"
INK = "#2b2f36"
INK_FAINT = "#5a5f68"
LED = "#ff4a3a"
BEZEL = "#26282b"
SCREEN = "#08130d"
GRID = "#2b4234"
GAP_BLACK = "#000000"
PLATE = "#0c100e"
PLATE_EDGE = "#2b4234"
KEY_LINE = "#4a5550"
KEY_TEXT = "#96a59e"
READ = "#b8c4bc"
ORANGE = "#ff8c1a"
MARK = "#e0b020"
TAG = "#d8e0da"
BLUE = "#0a55a0"
M = 20  # bench margin around the case

# The game's sizes (src/view/scope_layout.cpp, src/ui/controls.hpp).
KEY_W, KEY_H = 36, 44
KEYS_TOP, GROUP_TOP, GROUP_BOTTOM = 22, 6, 70
PAD, GAPX, KEY_GAP = 6, 8, 2
STRIP = 80  # the bezel's top
CLOSE = 40  # CaseClose's corner
PLATE_W = 248
CORNER_H = 34  # above the plates: the overview's and the axis's rows
OVERVIEW_H = 10
AXIS_H = 18
ROW_MIN, ROW_MAX, ROW_GAP = 28, 56, 2


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;")


def text(x, y, s, size=13, fill="#ddd", anchor="start", weight="normal", family=FONT, extra=""):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{family}" font-size="{size}" fill="{fill}" '
            f'text-anchor="{anchor}" font-weight="{weight}" {extra}>{esc(s)}</text>')


def rect(x, y, w, h, fill, rx=0, stroke=None, sw=1, extra=""):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    return (f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" '
            f'fill="{fill}"{s} {extra}/>')


def line(x1, y1, x2, y2, stroke, sw=1, extra=""):
    return (f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" '
            f'stroke-width="{sw}" {extra}/>')


DEFS = """<defs>
<filter id="glow" x="-5%" y="-30%" width="110%" height="160%">
  <feGaussianBlur stdDeviation="1.6" result="b"/>
  <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
</filter>
<radialGradient id="lamp" cx="40%" cy="35%" r="70%">
  <stop offset="0%" stop-color="#c8ffb0"/><stop offset="60%" stop-color="#3ce03c"/><stop offset="100%" stop-color="#1a7a1a"/>
</radialGradient>
</defs>"""


def doc(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="0 0 {w} {h}">{DEFS}<rect width="{w}" height="{h}" fill="#141518"/>'
            f'{body}</svg>')


# --- The logo (as docs/p8h-oscilloscope/mockups/scope_mock.py's "plain") ---
LW, LH, CX, CY, R, LGAP = 1000, 622, 500, 311, 292, 18


def logo(x, y, height):
    st, hb, top, bottom, rr = 56, 44, 196, 425, 40
    b0, b1 = 282, 442
    w0, s1, s2, w1 = 472, 562, 652, 708
    b = (f"M{b0},-60 H{b0 + st} V{top} H{b1 - rr} Q{b1},{top} {b1},{top + rr} V{bottom - rr} "
         f"Q{b1},{bottom} {b1 - rr},{bottom} H{b0} Z M{b0 + st},{top + hb} V{bottom - hb} "
         f"H{b1 - st} V{top + hb} Z")
    w = (f"M{w0},{top} H{w0 + st} V{bottom - hb} H{s1} V{top + 64} H{s1 + st} V{bottom - hb} "
         f"H{s2} V{top} H{w1} V{bottom - rr} Q{w1},{bottom} {w1 - rr},{bottom} H{w0 + rr} "
         f"Q{w0},{bottom} {w0},{bottom - rr} Z")
    k = height / LH
    slant = f"translate({CX},{CY}) skewX(-21) scale(0.94) translate({-CX},{-CY})"
    clip = f"clip{x:.0f}{y:.0f}"
    gaps = "".join(f'<path d="{p}" fill="#fff" stroke="#fff" stroke-width="{2 * LGAP}"/>'
                   for p in (b, w))
    fills = "".join(f'<path d="{p}" fill="{BLUE}" fill-rule="evenodd"/>' for p in (b, w))
    return (f'<g transform="translate({x},{y}) scale({k})"><clipPath id="{clip}"><rect '
            f'width="{LW}" height="{LH}" rx="74"/></clipPath><g clip-path="url(#{clip})">'
            f'<rect width="{LW}" height="{LH}" fill="{BLUE}"/><circle cx="{CX}" cy="{CY}" '
            f'r="{R}" fill="#fff"/><g transform="{slant}">{gaps}{fills}</g></g></g>')


# --- The case and its strip ---

def key(x, y, w, h, label, size=12, pressed=False):
    out = []
    if not pressed:
        out.append(rect(x, y + 2, w, h - 2, "#9c9a93", rx=4))
    face_y, face_h = (y + 2, h - 2) if pressed else (y, h - 2)
    out.append(rect(x, face_y, w, face_h, "#dcdbd5" if pressed else "#efeee9", rx=4,
                    stroke="#a9a69e"))
    out.append(text(x + w / 2, face_y + face_h / 2 + size * 0.36, label, size, INK, "middle",
                    "bold", SANS))
    return "".join(out)


def led(x, y, w, h, value):
    return (rect(x, y, w, h, "#1a0705", rx=3, stroke="#555")
            + text(x + w / 2, y + h / 2 + 5, value, 14, LED, "middle", "bold"))


def label(x0, x1, y, s):
    return text((x0 + x1) / 2, y, s, 10, INK, "middle", "bold", SANS)


def lamp(cx, cy, name, lit):
    out = []
    if lit:
        out.append(f'<circle cx="{cx}" cy="{cy}" r="14" fill="#3ce03c" opacity="0.18"/>')
        out.append(f'<circle cx="{cx}" cy="{cy}" r="9" fill="url(#lamp)" stroke="#222"/>')
    else:
        out.append(f'<circle cx="{cx}" cy="{cy}" r="9" fill="#1f3a1f" stroke="#222"/>')
    out.append(text(cx, cy + 22, name, 9, INK, "middle", "bold", SANS))
    return "".join(out)


def rocker(x, y, halves, lit, half_w):
    w = half_w * len(halves)
    out = [rect(x, y + 6, w, KEY_H - 12, "#9c9a93", rx=5)]
    for i, h in enumerate(halves):
        hx = x + i * half_w
        if i == lit:
            out.append(rect(hx + 2, y + 8, half_w - 4, KEY_H - 16, "#efeee9", rx=4,
                            stroke="#a9a69e"))
        out.append(text(hx + half_w / 2, y + KEY_H / 2 + 4, h, 11,
                        INK if i == lit else "#55585e", "middle", "bold" if i == lit else "normal",
                        SANS))
    return "".join(out)


def stepper(x, y, value, led_w):
    return (key(x, y, KEY_W, KEY_H, "◀") + led(x + KEY_W + KEY_GAP, y, led_w, KEY_H, value)
            + key(x + KEY_W + 2 * KEY_GAP + led_w, y, KEY_W, KEY_H, "▶"))


def stepper_w(led_w):
    return 2 * KEY_W + 2 * KEY_GAP + led_w


def strip(x, y, W, state, words):
    """The strip from the left; returns (svg, the x where it ends). state: armed, triggered, done."""
    out, cx = [], x + 12
    out.append(logo(cx, y + KEYS_TOP + 6, 32))
    if words:
        out.append(text(cx + 59, y + KEYS_TOP + 19, "BODGE", 11, INK, weight="bold",
                        family=SANS))
        out.append(text(cx + 59, y + KEYS_TOP + 35, "WIRE", 11, INK, weight="bold",
                        family=SANS))
    cx += 51 + (58 if words else 0) + GAPX
    ky = y + KEYS_TOP

    def group(x0, x1, name, parts=None):
        out.append(rect(x0, y + GROUP_TOP, x1 - x0, GROUP_BOTTOM - GROUP_TOP, GROUP, rx=5))
        for a, b, n in parts or [(x0, x1, name)]:
            out.append(label(a, b, y + GROUP_TOP + 11, n))

    # CAPTURE: ARM, three lamps.
    g0 = cx
    arm_w, lamp_step = 48, 40
    g1 = g0 + PAD + arm_w + GAPX + 3 * lamp_step + PAD
    group(g0, g1, "CAPTURE")
    out.append(key(g0 + PAD, ky, arm_w, KEY_H, "ARM"))
    for i, (n, s) in enumerate([("ARMED", "armed"), ("TRIG'D", "triggered"), ("DONE", "done")]):
        out.append(lamp(g0 + PAD + arm_w + GAPX + lamp_step * i + lamp_step / 2, ky + 15, n,
                        state == s))
    cx = g1 + GAPX
    # SAMPLE: ◀ LED ▶.
    g0 = cx
    g1 = g0 + PAD + stepper_w(64) + PAD
    group(g0, g1, "SAMPLE")
    out.append(stepper(g0 + PAD, ky, "100 ns", 64))
    cx = g1 + GAPX
    # TRIGGER AT: EARLY | MID | LATE.
    g0 = cx
    g1 = g0 + PAD + 3 * 44 + PAD
    group(g0, g1, "TRIGGER AT")
    out.append(rocker(g0 + PAD, ky, ["EARLY", "MID", "LATE"], 0, 44))
    cx = g1 + GAPX
    # VIEW: ◀ LED ▶ for the zoom, FIT.
    g0 = cx
    g1 = g0 + PAD + stepper_w(64) + GAPX + 44 + PAD
    group(g0, g1, "", [(g0, g0 + PAD + stepper_w(64), "VIEW / DIV")])
    out.append(stepper(g0 + PAD, ky, "1 µs", 64))
    out.append(key(g0 + PAD + stepper_w(64) + GAPX, ky, 44, KEY_H, "FIT"))
    cx = g1 + GAPX
    # The size key, left of CaseClose's corner.
    sx = x + W - CLOSE - 2 - KEY_W
    out.append(key(sx, ky + 4, KEY_W, KEY_W, ""))
    out.append(rect(sx + 10, ky + 12, 16, 16, "none", stroke=INK, sw=1.4))
    out.append(rect(sx + 10, ky + 20, 8, 8, INK))
    return "".join(out), cx


def case(x, y, W, H):
    out = [rect(x + 3, y + 5, W, H, "#000", rx=12, extra='opacity="0.45"'),
           rect(x, y, W, H, FRAME, rx=10, stroke="#1d1e20", sw=1.5),
           rect(x + 6, y + 6, W - 12, H - 12, PANEL, rx=6)]
    # CaseClose's ×, the corner grip.
    out.append(f'<circle cx="{x + W - 20}" cy="{y + 20}" r="13" fill="#c4c1b8" stroke="#8f8c84"/>')
    out.append(text(x + W - 20, y + 25, "×", 16, INK, "middle", "bold"))
    for k in range(3):
        o = 9 + k * 6
        out.append(line(x + W - o, y + H - 8, x + W - 8, y + H - o, "#8f8c84", 1.4))
    return "".join(out)


# --- The screen ---

class Row:
    def __init__(self, name, kind, data, trigger="", value_at=None, bits=1, ticked=False,
                 member=False, folded=None, empty=False):
        self.name, self.kind, self.data = name, kind, data  # kind: "bit" or "bus"
        self.trigger = trigger  # "" any, else the lit key's text
        self.value_at = value_at  # function t -> text
        self.bits, self.ticked, self.member, self.folded = bits, ticked, member, folded
        self.empty = empty


def value(data, t, bus):
    v = data[0][1]
    for when, level in data:
        if when <= t:
            v = level
    return v if bus else str(v)


def plate(x, y, w, h, row, colour, cursor_value, ticks_shown, dim):
    out = [rect(x, y, w, h, PLATE), line(x + w, y, x + w, y + h, PLATE_EDGE)]
    cy = y + h / 2
    # The swatch, or its tick box when grouping is possible.
    sx = x + 8 + (14 if row.member else 0)
    if ticks_shown and not row.kind == "bus":
        out.append(rect(sx, cy - 7, 14, 14, colour if row.ticked else "none", rx=2,
                        stroke=colour, sw=1.5))
        if row.ticked:
            out.append(f'<path d="M{sx + 3},{cy} l3,4 l6,-8" stroke="#000" stroke-width="2" '
                       f'fill="none"/>')
    else:
        out.append(rect(sx, cy - 6, 12, 12, colour, rx=2))
    nx = sx + 20
    if row.folded is not None:
        arrow = (f"M{nx},{cy - 5} l8,5 l-8,5 Z" if row.folded else
                 f"M{nx - 1},{cy - 3} l10,0 l-5,7 Z")
        out.append(f'<path d="{arrow}" fill="{KEY_TEXT}"/>')
        nx += 14
    out.append(text(nx, cy + 4.5, row.name, 13, colour, family=FONT,
                    extra='opacity="0.55"' if dim else ""))
    # The value, right-aligned before the Trigger key.
    tk_w = 40 if row.kind == "bus" else 28
    tk_x = x + w - 6 - 22 - 4 - tk_w
    if cursor_value:
        out.append(text(tk_x - 6, cy + 4.5, cursor_value, 13, READ, "end"))
    # The Trigger key: dark on any, lit orange with its condition.
    lit = bool(row.trigger)
    kh = min(24, h - 6)
    if not row.member:
        out.append(rect(tk_x, cy - kh / 2, tk_w, kh, ORANGE if lit else "none", rx=4,
                        stroke=ORANGE if lit else KEY_LINE))
        out.append(text(tk_x + tk_w / 2, cy + 4.5, row.trigger or ("x" * ((row.bits + 3) // 4)
                                                                   if row.kind == "bus" else "·"),
                        14 if row.trigger == "↑" or row.trigger == "↓" else 13, "#111" if lit else KEY_TEXT, "middle", "bold"))
    # ×
    xx = x + w - 6 - 22
    out.append(rect(xx, cy - 11, 22, 22, "none", rx=4, stroke=KEY_LINE))
    out.append(text(xx + 11, cy + 4.5, "×", 13, KEY_TEXT, "middle"))
    return "".join(out)


def lane(x0, x1, top, h, row, colour, tx, dim):
    out = []
    if row.empty:
        out.append(text(x0 + 8, top + h / 2 + 4, "not in this Capture", 12, "#5d6b63"))
        return "".join(out)
    op = 'opacity="0.35"' if dim else ""
    hi, lo, mid = top + 6, top + h - 6, top + h / 2
    out.append(f'<g clip-path="url(#lanes)" {op}>')
    data = row.data
    if row.kind == "bit":
        pts = []
        for i, (t, level) in enumerate(data):
            t_end = data[i + 1][0] if i + 1 < len(data) else 1e9
            xa, xb = max(tx(t), x0 - 2), min(tx(t_end), x1 + 2)
            if xb < x0 or xa > x1:
                continue
            y = hi if level else lo
            if pts:
                pts.append((xa, pts[-1][1]))
            pts += [(xa, y), (xb, y)]
        d = " ".join(f"{px:.1f},{py:.1f}" for px, py in pts)
        out.append(f'<polyline points="{d}" stroke="{colour}" stroke-width="1.6" fill="none" '
                   f'filter="url(#glow)"/>')
    else:
        for i, (t, v) in enumerate(data):
            t_end = data[i + 1][0] if i + 1 < len(data) else 1e9
            xa, xb = tx(t), tx(t_end)
            if xb < x0 or xa > x1:
                continue
            s = 4
            pts = [(xa, mid), (xa + s, hi), (xb - s, hi), (xb, mid), (xb - s, lo), (xa + s, lo)]
            d = " ".join(f"{px:.1f},{py:.1f}" for px, py in pts)
            out.append(f'<polygon points="{d}" stroke="{colour}" stroke-width="1.4" '
                       f'fill="none" filter="url(#glow)"/>')
            cxm = (max(xa, x0) + min(xb, x1)) / 2
            if min(xb, x1) - max(xa, x0) > 34:
                out.append(text(cxm, mid + 4.5, v, 12, colour, "middle"))
    out.append("</g>")
    return "".join(out)


def screen(x, y, w, h, rows, view, trigger=0.0, marker=None, cursor=None, state="done",
           ticks_shown=False, group_lit=False, bus_mode=False, count="5 of 64"):
    """`view`: (left µs, µs per division); divisions every 1 µs scaled to the lanes."""
    out = [rect(x, y, w, h, GAP_BLACK)]
    lx0, lx1 = x + PLATE_W, x + w
    left, per = view
    px_per_div = 100
    tx = lambda t: lx0 + (t - left) / per * px_per_div
    dim = state == "armed"
    out.append(f'<clipPath id="lanes"><rect x="{lx0}" y="{y}" width="{lx1 - lx0}" '
               f'height="{h}"/></clipPath>')
    # The corner above the plates: the Clips' controls (PIN | BUS, GROUP, the count).
    cy = y + 4
    rw = 40
    out.append(rect(x + 6, cy, 2 * rw, 24, "#1c2420", rx=4, stroke=KEY_LINE))
    out.append(rect(x + 6 + (rw if bus_mode else 0) + 1, cy + 1, rw - 2, 22, "#3a4a42", rx=3))
    out.append(text(x + 6 + rw / 2, cy + 16, "PIN", 11, "#e8efe9" if not bus_mode else KEY_TEXT,
                    "middle", "bold", SANS))
    out.append(text(x + 6 + rw * 1.5, cy + 16, "BUS", 11, "#e8efe9" if bus_mode else KEY_TEXT,
                    "middle", "bold", SANS))
    gx = x + 6 + 2 * rw + 8
    out.append(rect(gx, cy, 60, 24, ORANGE if group_lit else "none", rx=4,
                    stroke=ORANGE if group_lit else "#33403a"))
    out.append(text(gx + 30, cy + 16, "GROUP", 11, "#111" if group_lit else "#4a5a52", "middle",
                    "bold", SANS))
    out.append(text(x + PLATE_W - 8, cy + 16, count, 11, "#6f7d75", "end", family=SANS))
    # The overview: the whole Capture, the view as a box, T and M as ticks.
    oy = y + 4
    out.append(rect(lx0 + 4, oy + 2, lx1 - lx0 - 8, OVERVIEW_H, "#141a17", rx=2))
    rec0, rec1 = -655.0, 5898.0  # µs: the Capture, the Trigger early
    ox = lambda t: lx0 + 4 + (t - rec0) / (rec1 - rec0) * (lx1 - lx0 - 8)
    out.append(line(ox(trigger), oy, ox(trigger), oy + OVERVIEW_H + 4, ORANGE, 2))
    vx0 = ox(left)
    vx1 = max(ox(left + per * (lx1 - lx0) / px_per_div), vx0 + 6)
    out.append(rect(vx0, oy + 1, vx1 - vx0, OVERVIEW_H + 2, "none", rx=1.5, stroke="#e6e6e6",
                    sw=1.4))
    # The axis.
    ay = y + CORNER_H - AXIS_H + 14
    t = int(left) - 1
    while tx(t) <= lx1:
        if tx(t) >= lx0:
            out.append(line(tx(t), y + CORNER_H - 5, tx(t), y + CORNER_H, "#6f7d75"))
            if t % 2 == 0:
                s = "0" if t == 0 else f"{t:+d} µs"
                out.append(text(tx(t) + 3, ay, s, 12, ORANGE if t == 0 else "#9aa8a0"))
        t += 1
    # The screen under the rows, to its bottom: background, grid and the plates' column, so the
    # part no row uses still reads as the screen.
    top = y + CORNER_H
    out.append(rect(lx0, top, lx1 - lx0, y + h - top, SCREEN))
    out.append(rect(x, top, PLATE_W, y + h - top, PLATE))
    out.append(line(x + PLATE_W, top, x + PLATE_W, y + h, PLATE_EDGE))
    t = int(left) - 1
    while tx(t) <= lx1:
        if tx(t) >= lx0:
            out.append(line(tx(t), top, tx(t), y + h, GRID))
        t += 1
    # Rows: they share the height up to ROW_MAX each.
    room = y + h - top
    rh = max(ROW_MIN, min(ROW_MAX, (room - ROW_GAP * (len(rows) - 1)) / len(rows)))
    for i, row in enumerate(rows):
        ry = top + i * (rh + ROW_GAP)
        colour = row.colour
        out.append(rect(lx0, ry, lx1 - lx0, rh, SCREEN))
        # The grid, every division.
        t = int(left) - 1
        while tx(t) <= lx1:
            if tx(t) >= lx0:
                out.append(line(tx(t), ry, tx(t), ry + rh, GRID))
            t += 1
        out.append(lane(lx0, lx1, ry, rh, row, colour, tx, dim))
        at = cursor if cursor is not None else (marker if marker is not None else trigger)
        v = "" if row.empty else value(row.data, at, row.kind == "bus")
        out.append(plate(x, ry, PLATE_W, rh, row, colour, v, ticks_shown, dim))
        out.append(rect(x, ry + rh, w, ROW_GAP, GAP_BLACK))  # the black under each row
    # The Trigger, the Marker, the cursor and its tag.
    bottom = y + h  # the marks run to the screen's bottom, as the grid does
    out.append(f'<g clip-path="url(#lanes)">')
    out.append(line(tx(trigger), top, tx(trigger), bottom, ORANGE, 1.5))
    out.append(f'<path d="M{tx(trigger) - 6},{top} h12 l-6,8 Z" fill="{ORANGE}"/>')
    if marker is not None:
        mx = tx(marker)
        out.append(line(mx, top, mx, bottom, MARK, 1))
        out.append(rect(mx - 14, top, 28, 20, MARK, rx=3))
        out.append(text(mx, top + 15, "M", 13, "#111", "middle", "bold"))
    if cursor is not None:
        cx = tx(cursor)
        out.append(line(cx, top, cx, bottom, "#cfd8d2", 1, 'opacity="0.7"'))
        tag = f"{cursor:+g} µs" + (f"   dt {cursor - marker:g} µs" if marker is not None else "")
        tw = 7.8 * len(tag) + 14
        out.append(rect(cx + 6, top + 26, tw, 22, TAG, rx=4))
        out.append(text(cx + 13, top + 41, tag, 13, "#111"))
    out.append("</g>")
    return "".join(out)


def analyzer(W, H, rows, state="done", words=None, **kw):
    x, y = M, M
    if words is None:
        words = W >= 930
    svg_strip, end = strip(x, y, W, state, words)
    size_x = x + W - CLOSE - 2 - KEY_W
    print(f"  {W} x {H}: strip groups end at {end - x - GAPX:.0f} px, size key at "
          f"{size_x - x:.0f}: {'fits' if end - GAPX <= size_x - GAPX else 'TOO WIDE'}")
    body = [case(x, y, W, H), svg_strip]
    bx, by, bw, bh = x + 14, y + STRIP, W - 28, H - STRIP - 12
    body.append(rect(bx, by, bw, bh, BEZEL, rx=8, stroke="#111", sw=1.5))
    body.append(screen(bx + 8, by + 8, bw - 16, bh - 16, rows, state=state, **kw))
    return doc(W + 2 * M, H + 2 * M, "".join(body))


# --- Signals (µs from the Trigger) ---

def clock(period, t0=-12.0, t1=20.0, phase=0.0):
    out, t = [], t0 + phase
    while t < t1:
        out += [(t, 1), (t + period / 2, 0)]
        t += period
    return out


def led_computer_rows(empty_last=False):
    # The 6502 at reset: FFFC, FFFD, then F800's first instructions (as the game's README shot).
    cycles = [(-4, "F0E2", "FF"), (-3, "FFFB", "00"), (-2, "FFFC", "00"), (-1, "FFFD", "F8"), (0, "F800", "A2"), (1, "F801", "FF"),
              (2, "F802", "9A"), (3, "F803", "D8"), (5, "F804", "A9"), (7, "F805", "01"),
              (8, "F806", "8D")]
    sync = [(-12, 0), (0, 1), (1, 0), (2, 1), (3, 0), (3, 1), (5, 0), (5, 1), (6, 0), (8, 1),
            (9, 0)]
    rows = [Row("U1.7 SYNC", "bit", sync, trigger="↑"),
            Row("U1.39 PHI2", "bit", clock(1.0, phase=0.5)),
            Row("U1.34 RW", "bit", [(-12, 1)]),
            Row("U1 A[15:0]", "bus", [(t, a) for t, a, _ in cycles], trigger="F800", folded=True,
                bits=16),
            Row("U1 D[7:0]", "bus", [(t + 0.5, d) for t, _, d in cycles], folded=True, bits=8)]
    if empty_last:
        rows.append(Row("U3.12 CE_N", "bit", [], empty=True))
    for i, r in enumerate(rows):
        r.colour = COL[i % len(COL)]
    return rows


def counter_rows():
    qa = [(t, (int(t) + 1) % 2) for t in range(-12, 20)]
    qb = [(t, ((int(t) + 1) // 2) % 2) for t in range(-12, 20, 1)]
    qc = [(t, ((int(t) + 1) // 4) % 2) for t in range(-12, 20, 1)]
    qd = [(t, ((int(t) + 1) // 8) % 2) for t in range(-12, 20, 1)]
    rows = [Row("U1.14 QA", "bit", qa, trigger="↓", ticked=False),
            Row("U1.13 QB", "bit", qb, ticked=True),
            Row("U1.12 QC", "bit", qc, ticked=True),
            Row("U1.11 QD", "bit", qd)]
    for i, r in enumerate(rows):
        r.colour = COL[i]
    return rows


JOBS = {
    # The LED computer at the normal size: the Trigger SYNC rising AND the address at F800, a
    # Marker at +1 µs and the cursor at +2.3 µs.
    "analyzer_normal": lambda: analyzer(940, 520, led_computer_rows(), view=(-2.0, 1.0),
                                        marker=1.0, cursor=2.3),
    # The counter at the minimal size (880: the strip needs about 870), two plates ticked for a group: GROUP lit.
    "analyzer_minimal": lambda: analyzer(880, 340, counter_rows(), view=(-1.0, 1.0),
                                         ticks_shown=True, group_lit=True, count="4 of 64"),
    # Armed after a new Clip (CE_N): the shown Capture dimmed, the new pin's lane empty.
    "analyzer_armed": lambda: analyzer(940, 520, led_computer_rows(empty_last=True),
                                       view=(-2.0, 1.0), state="armed", count="6 of 64"),
}

if __name__ == "__main__":
    for name, make in JOBS.items():
        print(name)
        svg = make()
        with open(os.path.join(OUT, name + ".svg"), "w") as f:
            f.write(svg)
