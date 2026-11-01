# P4c — Manual Cleanup

Roadmap step P4c ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): make the game simpler by removing what players don't use. In play, the only manual text that helped was the outside description of what the Machine should do. The datasheets are the core of the game loop (follow a Trace, read the chip, go one chip up), and the role of each chip is useful in the Chip panel. Nobody used the rest of the Service Manual or the Net panel.

## User Manual

- `manuals/<machine>/user.md` is a title plus one `## What it does` section: one short paragraph describing the Machine from the outside. It says how to power it and what you see when it works, e.g. *"Connect +5 V to `J1`. After power-up, `D1` blinks about once a second."* No "Inside" list, no internal Nets, no `## Connections` section.
- Backticked references stay links (hover highlights the pad, click frames it) and are still checked against the netlist.
- It is shown only in the User Manual window (`U`). The Puzzle section shows only the Puzzle title and brief.

## Service Manual

- `manuals/<machine>/service.md` is a title plus one `## Components` section: one bullet per Component with its role on this Board (`` `U6` end-of-frame decoder ``). The check stays: every Component exactly once, every reference real.
- "How it works" and "Troubleshooting" are removed.
- No Service Manual window: the `H` toolbar entry goes, and `--manual` accepts only `user`.
- Roles are shown only in the Chip panel, on the line under its header (as today).

## Board hover

- Hovering a Component shows its reference, its Part and the Part's description (the same text as the datasheet title): *"U6 74LS20 · dual 4-input NAND"*. Pin hover is unchanged (the pin's datasheet line).
- When hover highlights a Net (on the Board, or from a Chip panel pin row), each Driver pad of that Net gets a distinct mark on the Board. Several Drivers get several marks, ready for buses later. The Drivers come from the design netlist (`DescribeNet`), so the mark does not leak faults.

## Removed

- The Net panel (the `Net` section of the side panel).
- Net selection: clicking copper and `--probe` no longer select a Net; nothing stays highlighted after the mouse leaves.
- Easy and hard modes, from all docs.

`DescribeNet` stays in `bw_netlist`; the Driver marks use it.

## Docs

- GLOSSARY: User Manual and Service Manual redefined as above; no hard mode.
- GAME_DESIGN: core loop, Board view, manuals and Schematic bullets drop the net panel, "How it works" and troubleshooting.
- ARCHITECTURE: design netlist row, the no-leak rule and the schematic decision drop the net panel and "How it works"; roadmap gets this step.
- Machine designs (COUNTER_DESIGN, LED_CHASER_DESIGN): `Manuals` sections describe the new short manuals.
- Old milestone docs (p3a, p3b) stay as they are.

## Tests

- Manual checks: user.md is exactly the title plus `What it does`; service.md is exactly the title plus `Components`; reference checks as today. Unit tests for both shapes, including rejection of extra sections.
- The three Machines' manuals (counter, chaser, test_pattern) pass the checks.
- Net panel tests and Net selection tests are removed; `DescribeNet` tests stay.
- Driver marks: the Driver pads of a Net are tested headlessly; the drawing is checked by the user (a screenshot has no hover).
- Release build: 1.00× and UI time unchanged; user check.
