# P4c Manual Cleanup — Implementation Plan

Design: [MANUAL_CLEANUP_DESIGN.md](MANUAL_CLEANUP_DESIGN.md)
Spec: [MANUAL_CLEANUP.md](MANUAL_CLEANUP.md)

Phases 1–3 and the docs/release part of phase 4 done 2026-11-01 (205f83d..0bc9326 and the docs commit after): short manuals, `BoardScene::drivers`, UI removals, Driver marks, Part description on hover; 469 tests pass; release 1.00×, ui 0.3 ms.

## Remaining

- User check: play one Puzzle per Machine; User Manual window (U) shows the short text; no Service Manual button; Chip panel shows the role; Component hover shows the Part description; hovering a Trace or pin marks its Driver pad(s); no Net section.
- Then ARCHITECTURE P4c row: "done YYYY-MM-DD: …"; commit `"docs: P4c done"`.
