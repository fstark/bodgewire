# Board placement prototype

Scripts from the 070 Board (2026-11-10), kept for [P4k](../../ARCHITECTURE.md#roadmap). Run them from the repository root; they read `boards/tennis/tennis.net` and `boards/tennis/tennis.kicad_pcb`.

- `place.py COLS ROWS XPITCH YPITCH X0 Y0 FIXED_JSON SEED OUT_JSON`: chips on a grid, annealed on half-perimeter wire length (`VCC`/`GND` left out); parts that are not DIPs stay where the Board has them. The 070 Board came from `place.py 12 5 16 27 45 22 '{"R1":[24,108]}' 7 placement.json`, then routed by hand.
- `place2.py SEED OUT_JSON`: connectors move too, each along its edge, `J3` kept left of `J4`.
- `place3.py list | PATTERN FLIPS SEED OUT_JSON`: each row upright (0/180°) or lying (90/270°).
- `apply.py SRC PLACEMENT OUT EXTRA_JSON`, `apply3.py SRC PLACEMENT OUT`: a copy of the Board with footprints moved (and rotated), tracks and vias removed, ready to route.

What they showed on 070:

| Run | Wire length |
|-----|-------------|
| first hand layout (chips grouped by U-number) | 15.1 m |
| `place.py`, connectors fixed | 10.7–11.3 m (8 seeds) |
| `place2.py`, connectors free on their edges | 10.5–11.4 m; puts `J3` and `J4` side by side |
| `place3.py`, rows flipped 180° | 10.7–11.0 m |
| `place3.py`, best upright/lying mix | 10.45 m |

Connector positions matter little for wiring but a lot for the player: they come from the Machine design, not the optimiser. Rotation is not worth its complexity; every chip keeps one orientation.
