"""P8h mock-ups (docs/p8h-oscilloscope/OSCILLOSCOPE.md): the Oscilloscope drawn as SVG, not by the
game. `python3 -I scope_mock.py` writes the SVGs beside it."""
import os

OUT = os.path.dirname(os.path.abspath(__file__))
FONT = "Menlo, monospace"
COL = ["#ffe600", "#00e6ff", "#ff3cdc", "#6ea0ff"]
CASE = "#c9c3b6"
CASE_DARK = "#a59e90"
BEZEL = "#26282b"
SCREEN = "#000000"
BAND = "#08130d"
GRID = "#2b4234"
GRID_MINOR = "#15251b"
ORANGE = "#ff8c1a"
AMBER = "#e0b020"

# Signals over t in µs from the Trigger: list of (time, level) changes, level 1 or 0.
def clk(t0, t1):
    out, t = [], -5.0
    while t < t1:
        out += [(t, 1), (t + 0.5, 0)]
        t += 1
    return out


SIGNALS = {
    "U1.2 CLK": (clk(-1, 9), "1 MHz · 50 %"),
    "U1.14 QA": ([(-4, 1), (-1, 1), (0, 0), (1, 1), (2, 0), (3, 1), (4, 0), (5, 1), (6, 0),
                  (7, 1), (8, 0), (9, 1)], "500 kHz · 50 %"),
    "U1.13 QB": ([(-4, 0), (0, 1), (2, 0), (4, 1), (6, 0), (8, 1), (10, 0)], "250 kHz · 50 %"),
    "U1.15 RCO": ([(-4, 0), (1, 1), (2, 0)], "high 1 µs"),
}


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;")


def text(x, y, s, size=13, fill="#ddd", anchor="start", weight="normal", family=FONT):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{family}" font-size="{size}" fill="{fill}" '
            f'text-anchor="{anchor}" font-weight="{weight}">{esc(s)}</text>')


def rect(x, y, w, h, fill, rx=0, stroke=None, sw=1, extra=""):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" fill="{fill}"{s} {extra}/>'


def line(x1, y1, x2, y2, stroke, sw=1, extra=""):
    return f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{sw}" {extra}/>'


DEFS = f"""<defs>
<filter id="glow" x="-5%" y="-20%" width="110%" height="140%">
  <feGaussianBlur stdDeviation="2.4" result="b"/>
  <feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
</filter>
<radialGradient id="crt" cx="50%" cy="50%" r="75%">
  <stop offset="0%" stop-color="#ffffff" stop-opacity="0.035"/>
  <stop offset="100%" stop-color="#000000" stop-opacity="0.35"/>
</radialGradient>
<linearGradient id="case" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0%" stop-color="#d8d2c6"/><stop offset="100%" stop-color="#b9b2a4"/>
</linearGradient>
<radialGradient id="knob" cx="40%" cy="35%" r="70%">
  <stop offset="0%" stop-color="#5a5d62"/><stop offset="100%" stop-color="#1c1d20"/>
</radialGradient>
<radialGradient id="lamp" cx="40%" cy="35%" r="70%">
  <stop offset="0%" stop-color="#c8ffb0"/><stop offset="60%" stop-color="#3ce03c"/><stop offset="100%" stop-color="#1a7a1a"/>
</radialGradient>
</defs>"""


def screen(x, y, w, h, hooks, label_w=176, marker=None, cursor=None, trigger_src=0):
    """The CRT: label column, axis strip, bands. Returns svg."""
    out = [rect(x, y, w, h, SCREEN, rx=8)]
    gx, gw = x + label_w, w - label_w - 8
    axis_h = 18
    top = y + 6 + axis_h
    bottom = y + h - 8
    gap = 6
    n = max(1, len(hooks))
    band_h = (bottom - top - gap * (n - 1)) / n
    t0, span = -1.0, 10.0
    X = lambda t: gx + (t - t0) / span * gw
    # Axis labels.
    for d in range(1, 10, 2):
        t = t0 + d
        lab = "0" if t == 0 else f"{t:+.0f} µs"
        out.append(text(X(t) + 3, y + 6 + 12, lab, 11, ORANGE if t == 0 else "#7f9a86"))
    # Trigger mark on the axis.
    tx = X(0)
    out.append(f'<path d="M{tx-5:.1f},{top-7:.1f} L{tx+5:.1f},{top-7:.1f} L{tx:.1f},{top:.1f} Z" fill="{ORANGE}"/>')
    bands = []
    for i, name in enumerate(hooks):
        by = top + i * (band_h + gap)
        bands.append(by)
        out.append(rect(gx, by, gw, band_h, BAND))
        for v in range(1, 5):
            yy = by + band_h * v / 5
            out.append(line(gx, yy, gx + gw, yy, GRID_MINOR))
        out.append(line(gx, by, gx + gw, by, GRID))
        out.append(line(gx, by + band_h, gx + gw, by + band_h, GRID))
    if not hooks:
        out.append(rect(gx, top, gw, bottom - top, BAND))
    # Vertical time lines through every band and gap.
    for d in range(0, 11):
        xx = gx + gw * d / 10
        out.append(line(xx, top, xx, bottom, GRID))
    # Traces and labels.
    for i, name in enumerate(hooks):
        by = bands[i]
        c = COL[i]
        changes, reading = SIGNALS[name]
        Y = lambda v, by=by: by + band_h - 4 - v / 5.0 * (band_h - 8)
        pts = []
        level = None
        for (t, lv) in changes:
            if t > t0 + span:
                break
            if t < t0:
                level = lv
                continue
            if level is None:
                level = lv
            if not pts:
                pts.append((X(t0), Y(3.5 if level else 0.2)))
            pts.append((X(t), Y(3.5 if level else 0.2)))
            level = lv
            pts.append((X(t), Y(3.5 if level else 0.2)))
        if not pts:
            pts.append((X(t0), Y(3.5 if level else 0.2)))
        pts.append((X(t0 + span), Y(3.5 if level else 0.2)))
        d = " ".join(f"{px:.1f},{py:.1f}" for px, py in pts)
        out.append(f'<polyline points="{d}" fill="none" stroke="{c}" stroke-width="1.6" filter="url(#glow)" stroke-linejoin="miter"/>')
        # 0 V arrow at the graticule's left edge.
        y0 = Y(0)
        out.append(f'<path d="M{gx-9:.1f},{y0-5:.1f} L{gx-1:.1f},{y0:.1f} L{gx-9:.1f},{y0+5:.1f} Z" fill="{c}"/>')
        # Label: name, T, ×; reading under it.
        lx = x + 10
        ly = by + 4
        out.append(rect(lx - 4, by, 3, band_h, c, extra='opacity="0.85"'))
        out.append(text(lx + 4, ly + 17, name, 13, c))
        bx = x + label_w - 10 - 28 * 2 - 4
        lit = i == trigger_src
        if lit:
            out.append(rect(bx, ly, 28, 28, ORANGE, rx=4))
            out.append(text(bx + 14, ly + 19, "▲T", 12, "#1a1a1a", "middle", "bold"))
        else:
            out.append(rect(bx, ly, 28, 28, "none", rx=4, stroke="#4a5550"))
            out.append(text(bx + 14, ly + 19, "T", 12, "#6c7a72", "middle"))
        out.append(rect(bx + 32, ly, 28, 28, "none", rx=4, stroke="#4a5550"))
        out.append(text(bx + 46, ly + 19, "×", 15, "#9aa8a0", "middle"))
        value = ""
        if cursor is not None:
            # Value under the cursor.
            lv = None
            for (t, l) in changes:
                if t <= cursor:
                    lv = l
            value = "  H" if lv else "  L"
        out.append(text(lx + 4, ly + 44, reading + value, 12, "#b8c4bc"))
    # Trigger line, marker, cursor.
    out.append(line(tx, top, tx, bottom, ORANGE, 1, 'opacity="0.75"'))
    if marker is not None:
        mx = X(marker)
        out.append(line(mx, top, mx, bottom, AMBER, 1))
        out.append(rect(mx - 14, top, 28, 20, AMBER, rx=3))
        out.append(text(mx, top + 15, "M", 12, "#1a1a1a", "middle", "bold"))
    if cursor is not None:
        cx = X(cursor)
        out.append(line(cx, top, cx, bottom, "#d8e0da", 1, 'opacity="0.8"'))
        tag = f"{cursor:+.0f} µs" + (f"   dt {cursor - marker:.2f} µs" if marker is not None else "")
        tw = len(tag) * 7.6 + 12
        left = cx + 4 if cx + 4 + tw < gx + gw else cx - 4 - tw
        out.append(rect(left, top, tw, 20, "#d8e0da", rx=3))
        out.append(text(left + 6, top + 15, tag, 12, "#111"))
    out.append(rect(x, y, w, h, "url(#crt)", rx=8))
    return "\n".join(out)


def knob(cx, cy, r, label, readout, angle=-40):
    import math
    out = [text(cx, cy - r - 8, label, 10, "#3a3a3a", "middle", "bold")]
    for k in range(11):
        a = math.radians(-135 + 27 * k)
        out.append(line(cx + math.sin(a) * (r + 3), cy - math.cos(a) * (r + 3),
                        cx + math.sin(a) * (r + 6), cy - math.cos(a) * (r + 6), "#5a5650", 1.2))
    out.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#knob)" stroke="#111" stroke-width="1"/>')
    a = math.radians(angle)
    out.append(line(cx + math.sin(a) * r * 0.25, cy - math.cos(a) * r * 0.25,
                    cx + math.sin(a) * r * 0.85, cy - math.cos(a) * r * 0.85, "#f0f0f0", 2.5, 'stroke-linecap="round"'))
    if readout:
        w = max(64, len(readout) * 8 + 14)
        out.append(rect(cx - w / 2, cy + r + 8, w, 20, "#0d1a12", rx=3, stroke="#222"))
        out.append(text(cx, cy + r + 23, readout, 12, "#7dff9a", "middle"))
    return "\n".join(out)


def toggle(x, y, upper, lower, up=True):
    out = [text(x + 40, y + 12, upper, 10, "#3a3a3a", "middle", "bold" if up else "normal"),
           text(x + 40, y + 70, lower, 10, "#3a3a3a", "middle", "bold" if not up else "normal")]
    out.append(rect(x + 26, y + 18, 28, 40, "#2a2b2e", rx=6))
    out.append(rect(x + 30, y + (22 if up else 38), 20, 16, "#d0d0d0", rx=3))
    return "\n".join(out)


def lamp(cx, cy, label, lit=True):
    out = [text(cx, cy - 18, label, 10, "#3a3a3a", "middle", "bold")]
    out.append(f'<circle cx="{cx}" cy="{cy}" r="9" fill="{"url(#lamp)" if lit else "#1f3a1f"}" stroke="#222"/>')
    if lit:
        out.append(f'<circle cx="{cx}" cy="{cy}" r="14" fill="#3ce03c" opacity="0.18"/>')
    return "\n".join(out)


def pushbutton(x, y, w, h, label, lamp_col=None):
    out = [rect(x, y + 2, w, h, "#55524c", rx=5),
           rect(x, y, w, h, "#3a3936", rx=5, stroke="#1e1d1b"),
           text(x + w / 2, y + h / 2 + 5, label, 12, "#eee", "middle", "bold")]
    if lamp_col:
        out.append(f'<circle cx="{x + w - 9}" cy="{y + 9}" r="3.5" fill="{lamp_col}"/>')
    return "\n".join(out)


def case(x, y, w, h, title=True):
    out = [rect(x + 3, y + 5, w, h, "#000", rx=12, extra='opacity="0.45"'),
           rect(x, y, w, h, "url(#case)", rx=12, stroke="#7d776c", sw=1.5)]
    # Corner grip.
    for k in range(3):
        o = 6 + k * 6
        out.append(line(x + w - o, y + h - 4, x + w - 4, y + h - o, "#7d776c", 1.4))
    if title:
        out.append(text(x + 16, y + 24, "BODGEWIRE  OS-4", 12, "#4a463f", weight="bold"))
    # Preset and close on the case, as the TV's ×.
    out.append(rect(x + w - 70, y + 6, 28, 28, CASE_DARK, rx=5))
    out.append(rect(x + w - 63, y + 13, 14, 14, "none", stroke="#3a3a3a", sw=1.5))
    out.append(f'<circle cx="{x + w - 22}" cy="{y + 20}" r="12" fill="{CASE_DARK}"/>')
    out.append(text(x + w - 22, y + 25, "×", 16, "#2a2a2a", "middle", "bold"))
    return "\n".join(out)


def bezel(x, y, w, h):
    return rect(x, y, w, h, BEZEL, rx=10, stroke="#111", sw=1.5)


def doc(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">'
            f'{DEFS}{rect(0, 0, w, h, "#15161a")}{body}</svg>')


M = 20  # bench margin around the case


def layout_a(W, H, hooks, **kw):
    """Panel on the right, knobs stacked: the classic bench scope."""
    x, y = M, M
    panel = 150
    body = [case(x, y, W, H)]
    bx, by = x + 12, y + 40
    bw, bh = W - 24 - panel - 6, H - 40 - 14
    body.append(bezel(bx, by, bw, bh))
    body.append(screen(bx + 8, by + 8, bw - 16, bh - 16, hooks, **kw))
    px = x + W - panel - 6
    cx = px + panel / 2
    body.append(knob(cx, by + 52, 30, "TIME / DIV", "1 µs", -20))
    body.append(knob(cx, by + 168, 22, "POSITION", "0", -135))
    body.append(line(px + 14, by + 228, px + panel - 14, by + 228, "#9a9386"))
    body.append(text(cx, by + 246, "TRIGGER", 10, "#3a3a3a", "middle", "bold"))
    body.append(toggle(px + 6, by + 256, "AUTO", "NORM"))
    body.append(lamp(px + panel - 40, by + 290, "TRIG'D"))
    body.append(pushbutton(px + 20, by + bh - 50, panel - 40, 40, "RUN / STOP", "#3ce03c"))
    return doc(W + 2 * M, H + 2 * M, "\n".join(body))


def layout_b(W, H, hooks, **kw):
    """Panel under the screen: the screen takes the full width."""
    x, y = M, M
    panel = 92
    body = [case(x, y, W, H)]
    bx, by = x + 12, y + 40
    bw, bh = W - 24, H - 40 - panel - 8
    body.append(bezel(bx, by, bw, bh))
    body.append(screen(bx + 8, by + 8, bw - 16, bh - 16, hooks, **kw))
    py = by + bh + 8
    cy = py + 40
    body.append(knob(x + 70, cy - 4, 24, "TIME / DIV", None, -20))
    body.append(rect(x + 104, cy - 14, 72, 20, "#0d1a12", rx=3, stroke="#222"))
    body.append(text(x + 140, cy + 1, "1 µs", 12, "#7dff9a", "middle"))
    body.append(knob(x + 230, cy - 4, 20, "POSITION", None, -135))
    body.append(rect(x + 258, cy - 14, 64, 20, "#0d1a12", rx=3, stroke="#222"))
    body.append(text(x + 290, cy + 1, "0", 12, "#7dff9a", "middle"))
    body.append(toggle(x + W - 268, py, "AUTO", "NORM"))
    body.append(lamp(x + W - 158, cy - 2, "TRIG'D"))
    body.append(pushbutton(x + W - 120, cy - 22, 96, 40, "RUN/STOP", "#3ce03c"))
    return doc(W + 2 * M, H + 2 * M, "\n".join(body))


def stepper_hw(x, y, label, value, w=150):
    """Drawn rocker: ◀ value ▶ under a small label."""
    out = [text(x + w / 2, y - 4, label, 10, "#3a3a3a", "middle", "bold"),
           pushbutton(x, y, 32, 30, "◀"),
           rect(x + 36, y, w - 72, 30, "#0d1a12", rx=3, stroke="#222"),
           text(x + w / 2, y + 20, value, 12, "#7dff9a", "middle"),
           pushbutton(x + w - 32, y, 32, 30, "▶")]
    return "\n".join(out)


def layout_c(W, H, hooks, **kw):
    """Packed: the panel is the case's top strip of push buttons; no knobs."""
    x, y = M, M
    body = [case(x, y, W, H, title=False)]
    body.append(stepper_hw(x + 14, y + 20, "TIME / DIV", "1 µs", 150))
    body.append(stepper_hw(x + 176, y + 20, "POSITION", "0", 128))
    # AUTO / NORM as a two-way rocker.
    rx = x + 318
    body.append(text(rx + 44, y + 16, "TRIGGER", 10, "#3a3a3a", "middle", "bold"))
    body.append(rect(rx, y + 20, 88, 30, "#2a2b2e", rx=5))
    body.append(rect(rx + 2, y + 22, 42, 26, "#d0d0d0", rx=4))
    body.append(text(rx + 23, y + 40, "AUTO", 11, "#111", "middle", "bold"))
    body.append(text(rx + 66, y + 40, "NORM", 11, "#8a8a8a", "middle"))
    body.append(f'<circle cx="{rx + 104}" cy="{y + 35}" r="7" fill="url(#lamp)" stroke="#222"/>')
    body.append(f'<circle cx="{rx + 104}" cy="{y + 35}" r="11" fill="#3ce03c" opacity="0.18"/>')
    body.append(pushbutton(x + W - 168, y + 20, 84, 30, "RUN", "#3ce03c"))
    bx, by = x + 12, y + 62
    bw, bh = W - 24, H - 62 - 14
    body.append(bezel(bx, by, bw, bh))
    body.append(screen(bx + 8, by + 8, bw - 16, bh - 16, hooks, **kw))
    return doc(W + 2 * M, H + 2 * M, "\n".join(body))



# Round 2: layout C, the labels over the screen, the timebase grid.

PLATE_PX = 156  # the labels' plate, in px; the Trigger one division right of it


def screen2(x, y, w, h, hooks, marker=None, cursor=None, trigger_src=0):
    out = [rect(x, y, w, h, SCREEN, rx=8)]
    gx, gw = x + 14, w - 22
    axis_h = 18
    top = y + 6 + axis_h
    bottom = y + h - 8
    gap = 6
    n = max(1, len(hooks))
    band_h = (bottom - top - gap * (n - 1)) / n
    div = gw / 10
    span = 10.0
    plate_w = PLATE_PX
    trig_div = plate_w / div + 1
    t0 = -trig_div
    X = lambda t: gx + (t - t0) / span * gw
    for t in [0, 2, 4, 6, 8]:
        if X(t) + 3 + 7 * 6 > gx + gw:
            break
        lab = "0" if t == 0 else f"{t:+.0f} µs"
        out.append(text(X(t) + 3, y + 6 + 12, lab, 11, ORANGE if t == 0 else "#7f9a86"))
    tx = X(0)
    out.append(f'<path d="M{tx-5:.1f},{top-7:.1f} L{tx+5:.1f},{top-7:.1f} L{tx:.1f},{top:.1f} Z" fill="{ORANGE}"/>')
    bands = []
    for i in range(n):
        by = top + i * (band_h + gap)
        bands.append(by)
        out.append(rect(gx, by, gw, band_h, BAND))
        for v in range(1, 5):
            yy = by + band_h * v / 5
            out.append(line(gx, yy, gx + gw, yy, GRID_MINOR))
        out.append(line(gx, by, gx + gw, by, GRID))
        out.append(line(gx, by + band_h, gx + gw, by + band_h, GRID))
    # Time lines from the Trigger: a division each.
    k = -int(trig_div)
    while X(k) <= gx + gw + 0.5:
        if X(k) >= gx - 0.5:
            out.append(line(X(k), top, X(k), bottom, GRID))
        k += 1
    for i, name in enumerate(hooks):
        by = bands[i]
        c = COL[i]
        changes, reading = SIGNALS[name]
        Y = lambda v, by=by: by + band_h - 4 - v / 5.0 * (band_h - 8)
        pts, level = [], None
        for (t, lv) in changes:
            if t > t0 + span:
                break
            if t < t0:
                level = lv
                continue
            if level is None:
                level = lv
            if not pts:
                pts.append((X(t0), Y(3.5 if level else 0.2)))
            pts.append((X(t), Y(3.5 if level else 0.2)))
            level = lv
            pts.append((X(t), Y(3.5 if level else 0.2)))
        if not pts:
            pts.append((X(t0), Y(3.5 if level else 0.2)))
        pts.append((X(t0 + span), Y(3.5 if level else 0.2)))
        d = " ".join(f"{px:.1f},{py:.1f}" for px, py in pts)
        out.append(f'<polyline points="{d}" fill="none" stroke="{c}" stroke-width="1.6" filter="url(#glow)"/>')
        y0 = Y(0)
        out.append(f'<path d="M{gx-10:.1f},{y0-5:.1f} L{gx-2:.1f},{y0:.1f} L{gx-10:.1f},{y0+5:.1f} Z" fill="{c}"/>')
        # The plate over the first divisions: the trace dimmed under it.
        out.append(rect(gx + 1, by + 1, plate_w - 1, band_h - 2, "#050a07"))
        lx, ly = gx + 6, by + 5
        lit = i == trigger_src
        if lit:
            out.append(rect(lx, ly, 28, 28, ORANGE, rx=4))
            out.append(text(lx + 14, ly + 19, "▲T", 12, "#1a1a1a", "middle", "bold"))
        else:
            out.append(rect(lx, ly, 28, 28, "#0b120e", rx=4, stroke="#4a5550"))
            out.append(text(lx + 14, ly + 19, "T", 12, "#6c7a72", "middle"))
        out.append(text(lx + 34, ly + 19, name, 12, c))
        xx = gx + plate_w - 6 - 28
        out.append(rect(xx, ly, 28, 28, "#0b120e", rx=4, stroke="#4a5550"))
        out.append(text(xx + 14, ly + 19, "×", 15, "#9aa8a0", "middle"))
        value = ""
        if cursor is not None:
            lv = None
            for (t, l) in changes:
                if t <= cursor:
                    lv = l
            value = "  H" if lv else "  L"
        out.append(text(lx + 2, ly + 46, reading + value, 12, "#b8c4bc"))
    out.append(line(tx, top, tx, bottom, ORANGE, 1, 'opacity="0.75"'))
    if marker is not None:
        mx = X(marker)
        out.append(line(mx, top, mx, bottom, AMBER, 1))
        out.append(rect(mx - 14, top, 28, 20, AMBER, rx=3))
        out.append(text(mx, top + 15, "M", 12, "#1a1a1a", "middle", "bold"))
    if cursor is not None:
        cx = X(cursor)
        out.append(line(cx, top, cx, bottom, "#d8e0da", 1, 'opacity="0.8"'))
        tag = f"{cursor:+.0f} µs" + (f"   dt {cursor - marker:.2f} µs" if marker is not None else "")
        tw = len(tag) * 7.6 + 12
        left = cx + 4 if cx + 4 + tw < gx + gw else cx - 4 - tw
        out.append(rect(left, top, tw, 20, "#d8e0da", rx=3))
        out.append(text(left + 6, top + 15, tag, 12, "#111"))
    out.append(rect(x, y, w, h, "url(#crt)", rx=8))
    return "\n".join(out)


def strip(x, y, W):
    body = [stepper_hw(x + 14, y + 20, "TIME / DIV", "1 µs", 150),
            stepper_hw(x + 176, y + 20, "POSITION", "0", 128)]
    rx = x + 318
    body.append(text(rx + 44, y + 16, "TRIGGER", 10, "#3a3a3a", "middle", "bold"))
    body.append(rect(rx, y + 20, 88, 30, "#2a2b2e", rx=5))
    body.append(rect(rx + 2, y + 22, 42, 26, "#d0d0d0", rx=4))
    body.append(text(rx + 23, y + 40, "AUTO", 11, "#111", "middle", "bold"))
    body.append(text(rx + 66, y + 40, "NORM", 11, "#8a8a8a", "middle"))
    body.append(f'<circle cx="{rx + 104}" cy="{y + 35}" r="7" fill="url(#lamp)" stroke="#222"/>')
    body.append(f'<circle cx="{rx + 104}" cy="{y + 35}" r="11" fill="#3ce03c" opacity="0.18"/>')
    body.append(pushbutton(x + W - 168, y + 20, 84, 30, "RUN", "#3ce03c"))
    return "\n".join(body)


def layout_c2(W, H, hooks, popup=False, **kw):
    x, y = M, M
    body = [case(x, y, W, H, title=False), strip(x, y, W)]
    bx, by = x + 12, y + 62
    bw, bh = W - 24, H - 62 - 14
    body.append(bezel(bx, by, bw, bh))
    body.append(screen2(bx + 8, by + 8, bw - 16, bh - 16, hooks, **kw))
    if popup:
        body.append(timebase_popup(x + 14, y + 56))
    return doc(W + 2 * M, H + 2 * M, "\n".join(body))


def timebase_popup(x, y):
    rows = [("ns", ["5", "10", "20", "50", "100", "200", "500"]),
            ("µs", ["1", "2", "5", "10", "20", "50", "100", "200", "500"]),
            ("ms", ["1", "2", "5", "10"])]
    cw, ch, pad = 44, 34, 8
    w = pad * 2 + 34 + cw * 9
    h = pad * 2 + ch * 3 + 4
    out = [rect(x + 3, y + 5, w, h, "#000", rx=8, extra='opacity="0.5"'),
           rect(x, y, w, h, "#2b2c30", rx=8, stroke="#111")]
    for r, (unit, values) in enumerate(rows):
        yy = y + pad + r * (ch + 2)
        out.append(text(x + pad + 14, yy + 22, unit, 13, "#9aa8a0", "middle", "bold"))
        for k, v in enumerate(values):
            xx = x + pad + 34 + k * cw
            sel = unit == "µs" and v == "1"
            out.append(rect(xx + 2, yy, cw - 4, ch, "#3d7a4a" if sel else "#3a3b40", rx=4,
                            stroke="#7dff9a" if sel else "#202125"))
            out.append(text(xx + cw / 2, yy + 22, v, 13, "#fff" if sel else "#ddd", "middle",
                            "bold" if sel else "normal"))
    return "\n".join(out)



# Round 3: the HP-like look. A charcoal frame round a light grey panel, light key caps, red LED
# readouts, the groups on tinted panels, the "bw" badge where RUN was.

PANEL = "#dedbd2"
FRAME = "#3c3e42"
GROUP = "#cfd5dc"
INK = "#2b2f36"
LED = "#ff4a3a"


def hp_case(x, y, w, h):
    out = [rect(x + 3, y + 5, w, h, "#000", rx=12, extra='opacity="0.45"'),
           rect(x, y, w, h, FRAME, rx=12, stroke="#1d1e20", sw=1.5),
           rect(x + 6, y + 6, w - 12, h - 12, PANEL, rx=7)]
    for k in range(3):
        o = 9 + k * 6
        out.append(line(x + w - o, y + h - 8, x + w - 8, y + h - o, "#8f8c84", 1.4))
    out.append(rect(x + w - 74, y + 14, 28, 28, "#c4c1b8", rx=4, stroke="#9c9a93"))
    out.append(rect(x + w - 67, y + 21, 14, 14, "none", stroke=INK, sw=1.5))
    out.append(rect(x + w - 40, y + 14, 28, 28, "#c4c1b8", rx=4, stroke="#9c9a93"))
    out.append(text(x + w - 26, y + 33, "×", 17, INK, "middle", "bold"))
    return "\n".join(out)


def hp_key(x, y, w, h, label, size=12):
    return "\n".join([rect(x, y + 2, w, h, "#9c9a93", rx=4),
                      rect(x, y, w, h, "#efeee9", rx=4, stroke="#a9a69e"),
                      text(x + w / 2, y + h / 2 + 5, label, size, INK, "middle", "bold")])


def hp_led(x, y, w, h, value):
    return "\n".join([rect(x, y, w, h, "#1a0705", rx=3, stroke="#555"),
                      text(x + w / 2, y + h / 2 + 5, value, 13, LED, "middle", "bold")])


def hp_stepper(x, y, label, value, w):
    return "\n".join([text(x + w / 2, y - 5, label, 10, INK, "middle", "bold"),
                      hp_key(x, y, 30, 30, "◀"),
                      hp_led(x + 34, y, w - 68, 30, value),
                      hp_key(x + w - 30, y, 30, 30, "▶")])


# The BW logo: blue rounded rectangle, white circle, slanted straight-stroke letters; strokes
# that leave the circle cut the blue with white gaps.
BLUE = "#0a55a0"
LW, LH = 1000, 622
CX, CY, R = 500, 311, 292
GAP = 18  # white gap each side of a stroke in the blue


def letters(variant):
    """Upright paths (before the slant): stems 56, bars 44, x-height 196..425, inside the circle."""
    st, hb = 56, 44
    top, bottom = 196, 425
    rx = 40
    paths = []
    # b: the stem from above the rectangle, the bowl rounded on the right.
    b0, b1 = 282, 442
    paths.append(f"M{b0},-60 H{b0 + st} V{top} H{b1 - rx} Q{b1},{top} {b1},{top + rx} "
                 f"V{bottom - rx} Q{b1},{bottom} {b1 - rx},{bottom} H{b0} Z "
                 f"M{b0 + st},{top + hb} V{bottom - hb} H{b1 - st} V{top + hb} Z")
    # w: three stems on a bar, the middle one shorter; rounded below, or the last stem running
    # on down out of the circle (the bodge wire).
    w0, gap = 472, 34
    s1, s2 = w0 + st + gap, w0 + 2 * (st + gap)
    w1 = s2 + st
    inner = (f"M{w0 + st},{top} V{bottom - hb} H{s1} V{top + 64} H{s1 + st} V{bottom - hb} "
             f"H{s2} V{top}")
    if variant == "wire":
        paths.append(f"M{w0},{top} {inner[1:].replace(f'M{w0 + st}', '')} H{w1} V{LH + 60} H{s2} "
                     f"V{bottom} H{w0 + rx} Q{w0},{bottom} {w0},{bottom - rx} Z")
    else:
        paths.append(f"M{w0},{top} {inner[1:].replace(f'M{w0 + st}', '')} H{w1} V{bottom - rx} "
                     f"Q{w1},{bottom} {w1 - rx},{bottom} H{w0 + rx} Q{w0},{bottom} {w0},{bottom - rx} Z")
    return paths


def logo(variant, x=0, y=0, height=None):
    k = (height / LH) if height else 1
    slant = f"translate({CX},{CY}) skewX(-21) scale(0.94) translate({-CX},{-CY})"
    ps = letters(variant)
    gaps = "".join(f'<path d="{p}" fill="#fff" stroke="#fff" stroke-width="{2 * GAP}" stroke-linejoin="round"/>' for p in ps)
    fills = "".join(f'<path d="{p}" fill="{BLUE}"/>' for p in ps)
    clip = f"clip{variant}{x}{y}"
    return (f'<g transform="translate({x},{y}) scale({k})">'
            f'<clipPath id="{clip}"><rect width="{LW}" height="{LH}" rx="74"/></clipPath>'
            f'<g clip-path="url(#{clip})">'
            f'<rect width="{LW}" height="{LH}" fill="{BLUE}"/>'
            f'<circle cx="{CX}" cy="{CY}" r="{R}" fill="#fff"/>'
            f'<g transform="{slant}">{gaps}{fills}</g></g></g>')



def hp_badge(x, y, words=True, variant="plain"):
    """The BW logo (an HP spoof) and, where there is room, the maker and the model."""
    out = [logo(variant, x, y, 32)]
    if words:
        out.append(text(x + 60, y + 14, "BODGE·WIRE", 11, INK, weight="bold",
                        family="Helvetica, Arial, sans-serif"))
        out.append(text(x + 60, y + 28, "OSCILLOSCOPE", 9, "#5a5f68",
                        family="Helvetica, Arial, sans-serif"))
    return "\n".join(out)


def hp_strip(x, y, W):
    out = [rect(x + 12, y + 10, 302, 50, GROUP, rx=5),   # horizontal
           rect(x + 320, y + 10, 124, 50, GROUP, rx=5),  # trigger
           hp_stepper(x + 18, y + 26, "TIME / DIV", "1 µs", 150),
           hp_stepper(x + 176, y + 26, "POSITION", "0", 132)]
    rx = x + 326
    out.append(text(rx + 56, y + 21, "TRIGGER", 10, INK, "middle", "bold"))
    out.append(rect(rx, y + 26, 88, 30, "#9c9a93", rx=4))
    out.append(rect(rx + 2, y + 28, 42, 26, "#efeee9", rx=3))
    out.append(text(rx + 23, y + 46, "AUTO", 11, INK, "middle", "bold"))
    out.append(text(rx + 66, y + 46, "NORM", 11, "#55585e", "middle"))
    out.append(f'<circle cx="{rx + 104}" cy="{y + 41}" r="7" fill="url(#lamp)" stroke="#222"/>')
    out.append(f'<circle cx="{rx + 104}" cy="{y + 41}" r="11" fill="#3ce03c" opacity="0.18"/>')
    room = W - 78 - (x + 452 - M)
    out.append(hp_badge(x + 452, y + 22, words=W >= 719))
    return "\n".join(out)


def layout_final(W, H, hooks, popup=False, **kw):
    x, y = M, M
    body = [hp_case(x, y, W, H), hp_strip(x, y, W)]
    bx, by = x + 14, y + 68
    bw, bh = W - 28, H - 68 - 16
    body.append(bezel(bx, by, bw, bh))
    body.append(screen2(bx + 8, by + 8, bw - 16, bh - 16, hooks, **kw))
    if popup:
        body.append(timebase_popup(x + 18, y + 62))
    return doc(W + 2 * M, H + 2 * M, "\n".join(body))



def logo_sheet():
    body = (logo("plain", 20, 20, 200) + logo("wire", 380, 20, 200)
            + logo("plain", 20, 240, 32) + logo("wire", 80, 240, 32))
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="720" height="290">'
            f'<rect width="720" height="290" fill="{PANEL}"/>{body}</svg>')


three = ["U1.2 CLK", "U1.14 QA", "U1.13 QB"]
four = three + ["U1.15 RCO"]
one = ["U1.2 CLK"]
JOBS = {
    # The three layouts compared (round 1).
    "layout_a": lambda: layout_a(680, 520, three, marker=2.0, cursor=5.0),
    "layout_b": lambda: layout_b(680, 520, three, marker=2.0, cursor=5.0),
    "layout_c": lambda: layout_c(680, 520, three, marker=2.0, cursor=5.0),
    # The chosen one.
    "scope_3_hooks": lambda: layout_final(680, 520, three, marker=2.0, cursor=5.0),
    "scope_1_hook": lambda: layout_final(680, 520, one),
    "scope_minimal_4_hooks": lambda: layout_final(620, 400, four),
    "bw_logo": lambda: logo_sheet(),
    "scope_timebase_grid": lambda: layout_final(680, 520, three, popup=True),
}
if __name__ == "__main__":
    for name, make in JOBS.items():
        with open(os.path.join(OUT, name + ".svg"), "w") as f:
            f.write(make())
        print(name)
