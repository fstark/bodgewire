# Bodge Wires and Trace Cuts — Implementation Plan (P3e)

Design: [BODGE_WIRES_DESIGN.md](BODGE_WIRES_DESIGN.md)
Spec: [BODGE_WIRES.md](BODGE_WIRES.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `BodgeWire` in `Board`; `FindIslands` joins wired pads | done e5e4cad |
| 2 | Machine: `cut`, `addWire`, `removeWire`, `mend` removes Cuts | done ca07690 |
| 3 | Repairs `cut:`/`wire:`/`unwire:` through Session and `bw run`; TV-frame tests | done 6d13e38 |
| 4 | UI: `Cut (K)`, `Wire (W)`, drawing, clicks, `--repair` errors, screenshots | done d3e6b14 |
| 5 | Release check, user check, docs | release done b133bf8; user check open |

Implemented 2026-10-31 (e5e4cad..b133bf8, 416 tests). Release: all tests pass; rebuild mean 0.8 ms with a wire, a Mend and a Cut per loop; `broken_trace --repair wire:U1.14:U13.6` 1.00×, ui 0.4 ms.

## Implementation notes

- The Cut mark is an `X` with arms of at least 4 px (at fit zoom 0.3 mm was ~1.5 px and invisible).
- `bw_ui --repair` reports any `Unknown` Repair as `cannot apply '<text>'` (Swap included); `bw run` keeps `unknown reference` for Swap.
- `[.perf]` rebuild loop now does Mend + Cut at `kH0Crack` each pass, with one wire soldered before.

---

## Phase 5 (rest) — User check, docs

### 5.2 — User check

The user fixes `broken_trace` with a Bodge Wire instead of a Mend, and breaks then restores the picture with a Cut and a Mend. Record feedback under "Later".

### 5.3 — Docs

ARCHITECTURE roadmap P3e row → done; this plan: prune; repo memory.

### Fence

- [ ] User confirmed
- [ ] Commit: `"docs: P3e done"`
