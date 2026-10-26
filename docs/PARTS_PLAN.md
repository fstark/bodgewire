# Parts and Netlist — Implementation Plan (P1b)

Design: [PARTS_DESIGN.md](PARTS_DESIGN.md)
Spec: [PARTS.md](PARTS.md)

Completed 2026-10-26, all 11 phases, commits `505e3ee..857dc02`.

Notes:

- Phase 2 delays: TI datasheet PDFs could not be fetched during implementation, so none of the design §5 values were re-checked against the datasheets (74LS00, 02, 04, 08, 20, 30, 32, 86, 74, 161 all unverified).
- Phase 11: `bw run netlists/counter.bwn --time 1ms --record-all --vcd counter.vcd` exits 0, no file exits 2, broken netlist exits 1. Still to do by hand: look at `counter.vcd` in a viewer (spec "Done when").
