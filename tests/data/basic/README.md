# Tiny BASIC conformance sessions

Each file is one session on a fresh machine, after the harness has typed `CLS`. The format, the views and the rules are in [TINY_BASIC.md](../../../docs/p5o-tiny-basic/TINY_BASIC.md#conformance-set):

- `~ text`: a comment. `~break`: the break key, after the last typed line. `~clear`: the screen was cleared here. `~typed`: needs keywords typed letter by letter.
- Any other line is one screen row: printed text, then `|` and what the player typed (`? |5`).
- `{RAM}`: a free RAM byte's address, given by the harness.

Checked by the reference interpreter: `python3 -I tests/basic/tiny_basic.py check tests/data/basic/*.txt` (`ctest -R "^basic_"`).
