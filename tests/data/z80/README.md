# Z80 conformance data

Used by `tests/parts/z80_conformance_test.cpp` (P5s, [Z80_DESIGN.md](../../../docs/p5s-z80/Z80_DESIGN.md), Test data).

- `harte/<name>.txt`: the first 25 cases of each of the 1,604 files of Tom Harte's Z80 SingleStepTests, <https://github.com/SingleStepTests/z80>, folder `v1`, commit `ebe1875d48f374bcfd4b505d8eb8ee751568b5f7`; a file name's spaces written as `_` (`dd cb __ 06.json` → `dd_cb____06.txt`). MIT licence: [LICENSE-harte](LICENSE-harte). Run in every ctest.
- The full sets are not in git. `tests/data/z80/fetch.sh` downloads them into `external/z80/` (git-ignored, about 630 MB converted): all 1,000 cases of each file, and Frank Cringle's `zexdoc.com` and `zexall.com` (<https://github.com/agn453/ZEXALL>, commit `8f71d418bae69a476a5a0e5c6e122c8801b8d9f4`, GPL, so never committed here). Without them, "Z80 core: Harte full", "Z80 core: ZEXDOC" and "Z80 core: ZEXALL" skip; they are hidden (`[.]`), run by name in release.
- `fetch.sh --subset` also rewrites `harte/` here. `convert_harte.py` turns one JSON file or a folder of them into this format (standard library only: `python3`).

## Format

`#` starts a comment line (the line before each case names it). A case is four or five lines, every number hex, `-` for a null address or data:

```
I <pc> <sp> <a> <f> <b> <c> <d> <e> <h> <l> <i> <r> <ix> <iy> <wz> <af'> <bc'> <de'> <hl'> <im> <iff1> <iff2> <ei> <p> <q> <address>:<value> …
F (the same fields, after the instruction)
C <address>:<data>:<flags> …     the bus at each T-state; flags are Harte's r w m i, '-' when not set
P <port>:<value>:<r|w> …         only when the instruction reads or writes a port
```

`wz` is MEMPTR; `ei` is set when the last instruction was `EI`, `p` when it was `LD A,I` or `LD A,R`, `q` is F when it set the flags, else 0.
