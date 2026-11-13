# 6502 conformance data

Used by `tests/parts/mos6502_conformance_test.cpp` (P5c, [MOS6502_DESIGN.md](../../../docs/p5c-6502/MOS6502_DESIGN.md), Test data).

- `harte/<hh>.txt`: the first 100 cases of each opcode of Tom Harte's SingleStepTests, <https://github.com/SingleStepTests/65x02>, folder `6502/v1`, commit `2f6980a2d95757486c7bee24355c360e40e2a224`. MIT licence: [LICENSE-harte](LICENSE-harte). Run in every ctest.
- The full sets are not in git. `tests/data/6502/fetch.sh` downloads them into `external/6502/` (git-ignored, about 420 MB): all 10,000 cases of each opcode, and Klaus Dormann's `6502_functional_test.bin` and `.lst` (<https://github.com/Klaus2m5/6502_65C02_functional_tests>, GPL-3.0, so never committed here) with its success trap in `success`. Without them, the "Harte full" and "Dormann functional test" cases skip.
- `fetch.sh --subset` also rewrites `harte/` here. `convert_harte.py` turns one JSON file or a folder of them into this format.

## Format

`#` starts a comment line (the line before each case names it). A case is three lines, every number hex:

```
I <pc> <s> <a> <x> <y> <p> <address>:<value> …     initial registers and RAM
F <pc> <s> <a> <x> <y> <p> <address>:<value> …     final registers and RAM
C <address>:<value>:<r|w> …                        bus cycles in order
```
