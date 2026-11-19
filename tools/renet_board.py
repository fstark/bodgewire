#!/usr/bin/env python3
"""A routed Board made ready for a new routing after its netlist changed (P6n).

Keeps every footprint where it is, deletes every track and via, and puts each pad on its Net from
the netlist (`bw export-kicad`), so KiCad needs no netlist import: open the Board, Freerouting on
the whole of it, save. Then `bw check-board`, and every crack Puzzle checked (its crack moves with
its Trace). For a Board laid out by hand with no `.place`, or one tidied after `bw place`, where
`bw place --force` would move the chips.

Usage (from the repository, release build present):
    python3 -I tools/renet_board.py machines/NNN-name/<m>.bwn machines/NNN-name/board/<m>.kicad_pcb -o OUT.kicad_pcb
Then copy OUT over the Board with KiCad closed.
"""
import argparse, os, re, subprocess, sys, tempfile


def pad_nets(bw, netlist):
    """{(reference, pad number): net} from `bw export-kicad`."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "board.net")
        run = subprocess.run([bw, "export-kicad", netlist, "-o", out], capture_output=True, text=True)
        if run.returncode != 0:
            sys.exit(run.stdout + run.stderr)
        text = open(out).read()
    nets = {}
    for net in re.finditer(r'\(net \(code "[^"]*"\) \(name "([^"]*)"\)(.*?)\)\)\s*(?=\(net |\)\s*\)\s*$)',
                           text, re.S):
        for ref, pin in re.findall(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', net.group(2)):
            nets[(ref, pin)] = net.group(1)
    return nets


def renet(text, nets):
    text = re.sub(r"^\t\((segment|via|arc)\b.*?\n\t\)\n", "", text, flags=re.M | re.S)

    def footprint(m):
        block = m.group(0)
        ref = re.search(r'\(property "Reference" "([^"]+)"', block).group(1)

        def pad(p):
            body = re.sub(r'\t*\(net "[^"]*"\)\n', "", p.group(0))
            net = nets.get((ref, p.group(1)))
            if net:
                at = re.search(r"\t+\((pinfunction|pintype|uuid)", body).start()
                indent = re.match(r"\t+", body[at:]).group(0)
                body = body[:at] + f'{indent}(net "{net}")\n' + body[at:]
            return body

        return re.sub(r'^\t\t\(pad "([^"]*)".*?\n\t\t\)\n', pad, block, flags=re.M | re.S)

    return re.sub(r"^\t\(footprint .*?\n\t\)\n", footprint, text, flags=re.M | re.S)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("netlist")
    ap.add_argument("board")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--bw", default="build/release/src/cli/bw")
    args = ap.parse_args()
    if os.path.abspath(args.output) == os.path.abspath(args.board):
        sys.exit("write to another file, then copy it over the Board with KiCad closed")
    nets = pad_nets(args.bw, args.netlist)
    open(args.output, "w").write(renet(open(args.board).read(), nets))
    check = subprocess.run([args.bw, "check-board", args.netlist, args.output], capture_output=True,
                           text=True)
    bad = [l for l in (check.stdout + check.stderr).splitlines() if l and not l.startswith("open ")]
    if bad and bad != ["ok"]:
        sys.exit("\n".join(bad))
    opened = sum(1 for l in check.stdout.splitlines() if l.startswith("open "))
    print(f"{args.output}: no tracks, {opened} Nets to route", file=sys.stderr)


if __name__ == "__main__":
    main()
