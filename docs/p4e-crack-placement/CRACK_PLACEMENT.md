# P4e — Crack Placement

Roadmap step P4e ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)). Today a Broken Trace is placed by a point on the Board (`fault crack F.Cu 137.59 99.17`). When a Board is laid out again, that point lands on nothing (the Puzzle fails to load) or on another Net (the Puzzle silently changes). After P4e a crack is placed by two pads and a position between them, so it survives a new layout.

## Puzzle line

```
fault crack U1.QA U13.2B 30%
```

- Two pads, as in `fault bridge`: a Pin by number or name (`U1.14`, `U1.QA`). They must be different pads, joined by Copper on the fault-free Board.
- A position from `0%` to `100%`, decimals allowed (`37.5%`).
- The Layer is not written: it is the Layer of the Trace the crack lands on.
- The old form (`fault crack F.Cu X Y`) is removed.

## Where the crack goes

1. **The path.** The Copper from the first pad to the second, through Traces, vias and other pads. If the Copper has loops, the path with the fewest pieces is used.
2. **The crackable part.** The stretches of straight Trace on that path where a crack's gap touches no other Copper: away from pads, vias and Trace corners and junctions. Arc Traces are never crackable.
3. **The position.** The crackable stretches are put end to end, from the first pad to the second: `0%` is the crackable point nearest the first pad, `100%` the one nearest the second. Every position in between lands on a crackable point.
4. **The check.** The crack must separate the two pads; otherwise (a loop around it) the Puzzle does not load.

The crack is then a Broken Trace as today: drawn, probed, mended and cut exactly as before.

## Load errors

Each names the Puzzle line:

- unknown reference, or a Component without that pin (as for `fault bridge`);
- `crack U1.QA U1.QA: two different pads`;
- `crack U1.QA U5.A: pads not joined by Copper`;
- `crack U1.QA U13.2B: no crackable Trace between the pads`;
- `crack U1.QA U13.2B: does not separate the pads`.

## After a new layout

- The crack always stays on the Copper between the same two pads, so it always breaks the same connection.
- Which other pads lose the signal can change: a Trace junction on the path can move from one side of the crack to the other. Accepted: a Puzzle test that checks the Symptom fails, and the author moves the position or rewrites the Brief.

## Existing Puzzles

The seven crack Puzzles (`broken_trace`, `unpowered_chip`, `dark_led`, `gap`, `dark_chaser`, `dead_button`, `loaded_dice`) are rewritten in the new form, each crack within 0.5 mm of its old point, so every Symptom stays the same.

## Tests

- Puzzle syntax: the new line, its errors, the old form refused.
- Placement on small hand-made Boards: one Trace, a corner, a via, a junction on the path, a pad stub too short to crack, an arc, a loop, pads not joined.
- Each load error from a Puzzle line.
- Tests stop using hard-coded crack points: they take the crack from the loaded Puzzle, or place one by pads and position.
- All existing Puzzle Symptom and Repair tests pass unchanged.

Design: [CRACK_PLACEMENT_DESIGN.md](CRACK_PLACEMENT_DESIGN.md). Plan: [CRACK_PLACEMENT_PLAN.md](CRACK_PLACEMENT_PLAN.md).
