#!/usr/bin/env python3
"""Routes the connections a KiCad Board still lacks (P5q2).

For each Net that `bw check-board` reports open ("open NET: {A.1, B.2} | {C.3}"), routes one track
from a pad of the smallest group to the nearest copper of the rest, on a grid over both copper
layers, with vias where both layers are free, every other Net's tracks, pads and vias kept at the
Board's clearance (0.2 mm, plus 0.05 mm for the grid). It repeats until check-board says ok, then
runs KiCad's DRC. Meant for the last few connections an autorouter (Freerouting) leaves: each
takes seconds to a minute in plain Python; the tracks are correct, not pretty (staircases,
vias where a human would detour).

Usage (from the repository, release build present):
    python3 -I tools/route_missing.py machines/NNN-name/<m>.bwn machines/NNN-name/board/<m>.kicad_pcb -o OUT.kicad_pcb
Then look at OUT in KiCad (or its DRC report) and copy it over the Board with KiCad closed.
Options: --margin MM (the search area around the two ends, default 40), --step MM (the grid,
default 0.1; 0.05 for very tight spots, slower).
"""
import argparse, heapq, math, os, re, shutil, subprocess, sys, uuid

TRACK_W, CLEAR, VIA_D, VIA_DRILL, HOLE_CLEAR, EDGE_CLEAR = 0.2, 0.2, 0.6, 0.3, 0.25, 0.5
SAFE = 0.05  # extra margin for the grid
LAYERS = ["F.Cu", "B.Cu"]

# --- a small s-expression reader ---------------------------------------------------------------
def tokens(s):
    for m in re.finditer(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+', s):
        yield m.group(0)


def parse(s):
    stack = [[]]
    for t in tokens(s):
        if t == "(":
            stack.append([])
        elif t == ")":
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(t.strip('"') if t.startswith('"') else t)
    return stack[0][0]


def find(node, head):
    return [c for c in node[1:] if isinstance(c, list) and c and c[0] == head]


def first(node, head):
    f = find(node, head)
    return f[0] if f else None




def route(text, NET, FREF, FPAD, MARGIN, STEP):
    """The Board's text with one new track of NET from pad FREF.FPAD, or None if none found."""
    root = parse(text)
    segs, vias, pads, edges = [], [], [], []
    for c in root[1:]:
        if not isinstance(c, list):
            continue
        if c[0] == "segment":
            s, e = first(c, "start"), first(c, "end")
            segs.append(((float(s[1]), float(s[2])), (float(e[1]), float(e[2])),
                         float(first(c, "width")[1]), first(c, "layer")[1], first(c, "net")[1]))
        elif c[0] == "via":
            a = first(c, "at")
            vias.append(((float(a[1]), float(a[2])), float(first(c, "size")[1]),
                         float(first(c, "drill")[1]), first(c, "net")[1]))
        elif c[0] == "gr_rect" and first(c, "layer")[1] == "Edge.Cuts":
            s, e = first(c, "start"), first(c, "end")
            edges = [float(s[1]), float(s[2]), float(e[1]), float(e[2])]
        elif c[0] == "footprint":
            at = first(c, "at")
            ox, oy = float(at[1]), float(at[2])
            ang = float(at[3]) if len(at) > 3 else 0.0
            ref = None
            for p in find(c, "property"):
                if p[1] == "Reference":
                    ref = p[2]
            rad = math.radians(ang)
            cs, sn = math.cos(rad), math.sin(rad)
            for p in find(c, "pad"):
                pa = first(p, "at")
                px, py = float(pa[1]), float(pa[2])
                pang = float(pa[3]) if len(pa) > 3 else ang
                size = first(p, "size")
                w, h = float(size[1]), float(size[2])
                if round(pang) % 180 == 90:
                    w, h = h, w
                net = first(p, "net")
                pads.append((ref, p[1], (ox + px * cs + py * sn, oy - px * sn + py * cs), w, h,
                             net[1] if net else None))

    # --- the grid over the area of the connection -----------------------------------------------
    source = [p for p in pads if p[0] == FREF and p[1] == FPAD]
    if not source or source[0][5] != NET:
        return None
    sx, sy = source[0][2]
    targets_xy = [p[2] for p in pads if p[5] == NET and not (p[0] == FREF and p[1] == FPAD)]
    tx = min(targets_xy, key=lambda q: math.hypot(q[0] - sx, q[1] - sy))
    x0 = max(edges[0], min(sx, tx[0]) - MARGIN)
    x1 = min(edges[2], max(sx, tx[0]) + MARGIN)
    y0 = max(edges[1], min(sy, tx[1]) - MARGIN)
    y1 = min(edges[3], max(sy, tx[1]) + MARGIN)
    W, H = int((x1 - x0) / STEP) + 1, int((y1 - y0) / STEP) + 1
    print(f"grid {W} x {H} over x {x0:.1f}-{x1:.1f} y {y0:.1f}-{y1:.1f}", file=sys.stderr)
    blocked = [bytearray(W * H) for _ in LAYERS]  # 1: a track centre may not be here
    noVia = bytearray(W * H)                      # 1: a via centre may not be here
    goal = [bytearray(W * H) for _ in LAYERS]     # the net's other copper


    def cell(x, y):
        return int(round((x - x0) / STEP)), int(round((y - y0) / STEP))


    def disk(grid, x, y, r, value=1):
        i0, j0 = cell(x - r, y - r)
        i1, j1 = cell(x + r, y + r)
        for j in range(max(0, j0), min(H - 1, j1) + 1):
            for i in range(max(0, i0), min(W - 1, i1) + 1):
                if math.hypot(x0 + i * STEP - x, y0 + j * STEP - y) <= r:
                    grid[j * W + i] = value


    def rect(grid, x, y, w, h, grow, value=1):
        i0, j0 = cell(x - w / 2 - grow, y - h / 2 - grow)
        i1, j1 = cell(x + w / 2 + grow, y + h / 2 + grow)
        for j in range(max(0, j0), min(H - 1, j1) + 1):
            row = j * W
            for i in range(max(0, i0), min(W - 1, i1) + 1):
                grid[row + i] = value


    def capsule(grid, a, b, r, value=1):
        (ax, ay), (bx, by) = a, b
        if max(ax, bx) + r < x0 or min(ax, bx) - r > x1 or max(ay, by) + r < y0 or min(ay, by) - r > y1:
            return
        length = math.hypot(bx - ax, by - ay)
        n = max(1, int(length / (STEP / 2)))
        for k in range(n + 1):
            disk(grid, ax + (bx - ax) * k / n, ay + (by - ay) * k / n, r, value)


    half = TRACK_W / 2
    for (a, b, w, layer, net) in segs:
        li = LAYERS.index(layer)
        if net == NET:
            capsule(goal[li], a, b, w / 2)
            continue
        capsule(blocked[li], a, b, w / 2 + CLEAR + half + SAFE)
        capsule(noVia, a, b, w / 2 + CLEAR + VIA_D / 2 + SAFE)
    for (at, size, drill, net) in vias:
        if net == NET:
            for li in range(2):
                disk(goal[li], at[0], at[1], size / 2)
            continue
        for li in range(2):
            disk(blocked[li], at[0], at[1], size / 2 + CLEAR + half + SAFE)
        disk(noVia, at[0], at[1], max(size / 2 + CLEAR + VIA_D / 2, drill / 2 + HOLE_CLEAR + VIA_DRILL / 2) + SAFE)
    for (ref, num, at, w, h, net) in pads:
        if net == NET and not (ref == FREF and num == FPAD):
            for li in range(2):
                rect(goal[li], at[0], at[1], w, h, 0)
            continue
        if ref == FREF and num == FPAD:
            continue
        for li in range(2):
            rect(blocked[li], at[0], at[1], w, h, CLEAR + half + SAFE)
        rect(noVia, at[0], at[1], w, h, CLEAR + VIA_D / 2 + SAFE)
    # The board's edge.
    for li in range(2):
        for j in range(H):
            for i in range(W):
                x, y = x0 + i * STEP, y0 + j * STEP
                if (x < edges[0] + EDGE_CLEAR + half or x > edges[2] - EDGE_CLEAR - half or
                        y < edges[1] + EDGE_CLEAR + half or y > edges[3] - EDGE_CLEAR - half):
                    blocked[li][j * W + i] = 1

    # --- A* from the source pad to any copper of the net -----------------------------------------
    si, sj = cell(sx, sy)
    VIA_COST = 30.0
    moves = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
             (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
    ti, tj = cell(*tx)
    best = {}
    openq = []
    for li in range(2):
        start = (si, sj, li)
        best[start] = 0.0
        heapq.heappush(openq, (math.hypot(si - ti, sj - tj), 0.0, start, None))
    came = {}
    found = None
    while openq:
        f, g, node, parent = heapq.heappop(openq)
        if node in came:
            continue
        came[node] = parent
        i, j, li = node
        if goal[li][j * W + i] and node[:2] != (si, sj):
            found = node
            break
        for di, dj, c in moves:
            ni, nj = i + di, j + dj
            if not (0 <= ni < W and 0 <= nj < H):
                continue
            k = nj * W + ni
            # Leaving the source pad, its own area is free.
            free = not blocked[li][k] or math.hypot(ni - si, nj - sj) * STEP < 1.0
            if not free:
                continue
            nn = (ni, nj, li)
            ng = g + c
            if ng < best.get(nn, 1e18):
                best[nn] = ng
                heapq.heappush(openq, (ng + math.hypot(ni - ti, nj - tj), ng, nn, node))
        k = j * W + i
        if not noVia[k] and not blocked[1 - li][k]:
            nn = (i, j, 1 - li)
            ng = g + VIA_COST
            if ng < best.get(nn, 1e18):
                best[nn] = ng
                heapq.heappush(openq, (ng + math.hypot(i - ti, j - tj), ng, nn, node))
    if not found:
        return None

    path = []
    node = found
    while node is not None:
        path.append(node)
        node = came[node]
    path.reverse()

    # --- straight runs, vias ------------------------------------------------------------------------
    out = []
    def xy(n):
        return round(x0 + n[0] * STEP, 4), round(y0 + n[1] * STEP, 4)

    start = path[0]
    prev = path[0]
    direction = None
    for n in path[1:]:
        if n[2] != prev[2]:  # a via
            if prev != start:
                out.append(("segment", xy(start), xy(prev), LAYERS[prev[2]]))
            out.append(("via", xy(prev)))
            start, prev, direction = n, n, None
            continue
        d = (n[0] - prev[0], n[1] - prev[1])
        if direction is not None and d != direction:
            out.append(("segment", xy(start), xy(prev), LAYERS[prev[2]]))
            start = prev
        direction = d
        prev = n
    if prev != start:
        out.append(("segment", xy(start), xy(prev), LAYERS[prev[2]]))
    # The first point from the pad's centre exactly.
    if out and out[0][0] == "segment":
        out[0] = ("segment", (round(sx, 4), round(sy, 4)), out[0][2], out[0][3])
        if math.dist(out[0][1], out[0][2]) == 0:
            out.pop(0)
    # The last point onto the net's copper exactly: the nearest pad or via centre, or the nearest
    # point of a track's centreline on the final layer.
    end = xy(path[-1])
    endLayer = LAYERS[path[-1][2]]
    cands = [p[2] for p in pads if p[5] == NET and not (p[0] == FREF and p[1] == FPAD)]
    cands += [v[0] for v in vias if v[3] == NET]
    for (a, b, w, layer, net) in segs:
        if net == NET and layer == endLayer:
            ax, ay = a; bx, by = b
            L2 = (bx - ax) ** 2 + (by - ay) ** 2
            t = 0 if L2 == 0 else max(0, min(1, ((end[0] - ax) * (bx - ax) + (end[1] - ay) * (by - ay)) / L2))
            cands.append((ax + t * (bx - ax), ay + t * (by - ay)))
    snap = min(cands, key=lambda q: math.dist(q, end))
    snap = (round(snap[0], 4), round(snap[1], 4))
    print(f"last point {end} snapped {math.dist(snap, end):.3f} mm to {snap}", file=sys.stderr)
    if math.dist(snap, end) > 0:
        out.append(("segment", end, snap, endLayer))

    items = []
    for o in out:
        if o[0] == "segment":
            items.append(f'\t(segment\n\t\t(start {o[1][0]} {o[1][1]})\n\t\t(end {o[2][0]} {o[2][1]})\n'
                         f'\t\t(width {TRACK_W})\n\t\t(layer "{o[3]}")\n\t\t(net "{NET}")\n'
                         f'\t\t(uuid "{uuid.uuid4()}")\n\t)\n')
        else:
            items.append(f'\t(via\n\t\t(at {o[1][0]} {o[1][1]})\n\t\t(size {VIA_D})\n\t\t(drill {VIA_DRILL})\n'
                         f'\t\t(layers "F.Cu" "B.Cu")\n\t\t(net "{NET}")\n\t\t(uuid "{uuid.uuid4()}")\n\t)\n')
    pos = text.rstrip().rfind(")")
    print(f"  {NET}: {len([o for o in out if o[0]=='segment'])} segments, "
          f"{len([o for o in out if o[0]=='via'])} vias", file=sys.stderr)
    return text[:pos] + "".join(items) + text[pos:]


def open_nets(bw, netlist, board):
    """The open connections check-board reports: [(net, [groups of "REF.PAD"])]."""
    run = subprocess.run([bw, "check-board", netlist, board], capture_output=True, text=True)
    nets = []
    for line in (run.stdout + run.stderr).splitlines():
        m = re.match(r"open (\S+): (.*)", line.strip())
        if m:
            groups = [re.findall(r"([A-Za-z0-9_]+\.[A-Za-z0-9]+)", g) for g in m.group(2).split("|")]
            nets.append((m.group(1), groups))
    if not nets and "ok" not in run.stdout:
        sys.exit(run.stdout + run.stderr)
    return nets


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("netlist")
    ap.add_argument("board")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--margin", type=float, default=40.0)
    ap.add_argument("--step", type=float, default=0.1)
    ap.add_argument("--bw", default="build/release/src/cli/bw")
    args = ap.parse_args()
    if os.path.abspath(args.output) == os.path.abspath(args.board):
        sys.exit("write to another file, then copy it over the Board with KiCad closed")
    shutil.copy(args.board, args.output)
    project = os.path.splitext(args.board)[0] + ".kicad_pro"
    if os.path.exists(project):  # the design rules, for KiCad's DRC
        shutil.copy(project, os.path.splitext(args.output)[0] + ".kicad_pro")
    tried = set()
    while True:
        nets = open_nets(args.bw, args.netlist, args.output)
        if not nets:
            print("check-board: ok", file=sys.stderr)
            break
        net, groups = nets[0]
        groups.sort(key=len)
        ref, pad = groups[0][0].split(".")
        key = (net, ref, pad, len(groups))
        if key in tried:
            sys.exit(f"{net}: still open after routing; try --step 0.05 or a larger --margin")
        tried.add(key)
        text = open(args.output).read()
        routed = route(text, net, ref, pad, args.margin, args.step)
        if routed is None:
            sys.exit(f"{net}: no path from {ref}.{pad}; try --step 0.05 or a larger --margin")
        open(args.output, "w").write(routed)
    if shutil.which("kicad-cli"):
        report = os.path.splitext(args.output)[0] + "-drc.rpt"
        drc = subprocess.run(["kicad-cli", "pcb", "drc", "-o", report, args.output],
                             capture_output=True, text=True)
        print("\n".join(l for l in drc.stdout.splitlines() if l.startswith("Found")), file=sys.stderr)
        print(f"DRC report: {report}", file=sys.stderr)


if __name__ == "__main__":
    main()
