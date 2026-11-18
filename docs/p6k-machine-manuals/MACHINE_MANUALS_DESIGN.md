# P6k — Manuals for every Machine — Design

Spec: [MACHINE_MANUALS.md](MACHINE_MANUALS.md). Plan: [MACHINE_MANUALS_PLAN.md](MACHINE_MANUALS_PLAN.md). The manuals' sections follow [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 (its `user.md` and `service.md` bullets) and the writing rules [STYLE.md](../STYLE.md) Manuals; the model is the Counter's (`machines/010-counter/manuals/`). No kernel, Part, builder, device, UI or Session change.

## Files

| File | Change |
|---|---|
| `machines/090-typewriter/typewriter.bwn` | Nets `R1`–`R3` renamed `GL1`–`GL3` (three `net` lines, the `U13` comment) |
| `machines/090-typewriter/board/typewriter.kicad_pcb` | every `(net "R1")`, `(net "R2")`, `(net "R3")` renamed (pads and tracks); the footprint `R1`'s `Reference` property untouched |
| `machines/090-typewriter/docs/TYPEWRITER.md`, `TYPEWRITER_DESIGN.md` | the Net names |
| `machines/090-typewriter/manuals/service.md` | phase 1: the `U13` and `U23` bullets name `GL1`–`GL3` |
| `machines/NNN-name/manuals/{user,service}.md`, 14 Machines (020–140) | rewritten |
| `machines/NNN-name/docs/NAME_DESIGN.md`, 14 Machines | `## Manuals` replaced, as the Counter's (`machines/010-counter/docs/COUNTER_DESIGN.md` §Manuals) |
| `src/manual/manual.cpp` | the sections: `CheckShape` takes an ordered list with an optional flag per section |
| `tests/manual/manual_test.cpp` | the cases listed under "The sections required" |
| `tests/machines/manuals_test.cpp` | `the Counter's explain how it works` folded into `every Machine's manuals pass their checks`: each Machine's sections not empty (`Using it` when present) |
| `docs/recipes/ADD_MACHINE.md` §2 | with the checks (phase 9) |
| `docs/STYLE.md` Manuals, `docs/ARCHITECTURE.md` | the last phase |

## The Typewriter's rename

The netlist's three lines become `net GL1 U13.1Y U23.A1` (and `GL2`, `GL3`); `U13`'s comment `# GL1-GL3, ROW_TOP`. No test, Puzzle or `.place` names them (checked: `tests/machines/typewriter_test.cpp`, `puzzles/`, `board/typewriter.place` mention only the resistor). The Board's Nets are matched by name (`bw check-board`), so the `.kicad_pcb` is edited in place with KiCad closed: `sed -i '' 's/(net "R\([123]\)")/(net "GL\1")/'` (macOS) touches only Net names: the file has no top-level net table, the Nets appear only as pad and track `(net "R1")` lines (4 `R1`, 10 `R2`, 14 `R3`), and the resistor's reference is a `(property "Reference" "R1"`. The `.net` is not committed (a local one goes stale; `bw export-kicad` remakes it).

- The Machine's docs: only `TYPEWRITER.md:43` and `TYPEWRITER_DESIGN.md` lines 37, 122–124, 344, 354 (the Nets); the resistor's lines (`TYPEWRITER.md:62`; `TYPEWRITER_DESIGN.md` 66, 225–226, 286, 330, 373) stay.
- The Service Manual's `U13` and `U23` bullets, in the same phase: after the rename `R3` is neither part nor Net, so `every Machine's manuals pass their checks` would report it.

Check: `./build/release/src/cli/bw check-board machines/090-typewriter/typewriter.bwn machines/090-typewriter/board/typewriter.kicad_pcb` prints `ok` ([ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §3); `drc_typewriter` (KiCad's DRC on the edited Board) and `boards: typewriter matches its netlist` pass.

## Writing a Machine's manuals

For each Machine, in this order:

1. **Read** its `docs/NAME.md` (Lesson, Circuit) and `NAME_DESIGN.md`, the netlist, the old manuals and the datasheets of its chips. The development docs are the source of truth; the manuals never quote them (no roadmap step, no test name, no Puzzle).
2. **`user.md`**: `# Name`, then
   - `## What it does`: a welcome, what the Machine is and what the player sees it do; the old hookup steps ("Connect +5 V to `J1`…") go. For a computer: its CPU, memory, program and display in a sentence each.
   - `## Using it` where there is something to operate: the buttons by their legends (P6g: `START`, `RESET`, `CLEAR`), the knobs, the keyboard or keypad, the monitor's commands and BASIC on the computers. What the old `What it does` said about operating moves here. Absent on the Chaser, Test Pattern, Ball and Message.
   - `## How it works`: STYLE's rules: the goal first, the blocks in the order the signals flow, a familiar picture before each mechanism, no formulas, unused chip features in a table. For a TV Machine the blocks are the beam counters, the sync, then what is drawn; for a computer, the clock and reset, the CPU, the address decoder (a memory map table), the memory, the I/O, the program.
3. **`service.md`**: `# Name — Service Manual`, then
   - `## Components`: kept (every part, netlist order), its wording tightened where it uses formulas or development terms; the Typewriter's `U13` and `U23` bullets already name `GL1`–`GL3` (phase 1).
   - `## Test points`: a table, as the Counter's: `Probe` (a pin: `` `U5.QD` `` or `` `D1` pin 1 ``), `Net`, `A working board shows`. The clock, the reset, each block's main output, the slow signals the eye can follow. For the TV and computer Machines (050–140), rows for the logic analyzer and oscilloscope: the pins to clip or hook, the trigger (a channel and an edge: `--clip REF.PIN:r` is the UI's own form), the sample time or timebase, and what a working board shows (a line's sync pulse and its length, a bus counting, a write strobe). The player sets them up by hand: no new link kind.
   - `## Troubleshooting`: general charts only (the spec): for each broad symptom (the Machine dead, no picture or no lock, a wrong count, a key that does nothing), ordered checks from the working Board, each a pin and what it must read. Never a fault's name, never a Puzzle's symptom as such.
4. **Check every reading** before writing it ([ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 `service.md`): `./build/release/src/cli/bw run machines/NNN-name/<m>.bwn --time T --probe REF.PIN,…` at two or three times (a slow Net read at a time it is high and one it is low); buttons, knobs and keys with `--press`, `--knob`, `--type` for readings that need them. An analyzer or scope row: `./build/release/src/ui/bw_ui machines/NNN-name/<m>.bwn --clip … --sample … --view … --screenshot /tmp/x.png --after 1s` (or `--hook`, `--timebase`, `--delay`), the PNG viewed. A Machine slow in the simulator (the BW-2001, the BW-80: P5z) reads at short `--time`s.
5. **The design doc's `## Manuals`**: the sections and what each says, in one bullet per manual, as the Counter's, and how the readings were checked (the times).
6. **Check**: `ctest --preset release -j -R "manual|<m>"`, then screenshots of both manuals (`bw_ui machines/NNN-name/<m>.bwn --manual user --screenshot …`, `--manual service`), viewed: the links resolved (underlined), the tables drawn, nothing cut.

The prose is drafted per Machine by a sub-agent briefed with this section, STYLE's Manuals, the Counter's manuals and the Machine's paths, the Machines of a batch in parallel (they share no file). The agent running the plan reads each draft whole against STYLE, corrects it, re-runs the checks and the screenshots, then commits the batch. The BW-80 is done last and alone: its manuals are the ones that motivated P6i.

## The sections required (`src/manual/manual.cpp`)

```cpp
struct SectionRule {
    std::string_view name;
    bool optional = false;
};
// A Title first, then the sections in order: each required one exactly once, each optional one
// at most once, in its place.
void CheckShape(const Markdown& doc, Problems& problems, std::span<const SectionRule> sections);
```

- User Manual: `{"What it does"}, {"Using it", true}, {"How it works"}`. Service Manual: `{"Components"}, {"Test points"}, {"Troubleshooting"}`. Datasheets: `{"Function"}, {"Pins"}` (all required, as today).
- The match: walk the headings and the rules together; a heading equal to the current rule's name consumes both; an optional rule not matched is skipped; a heading left over, or a required rule left over, fails.
- The message: each required name, each optional one in brackets with its comma inside: `sections must be: What it does[, Using it], How it works`; `sections must be: Components, Test points, Troubleshooting`; `sections must be: Function, Pins` (unchanged).
- Test changes (`tests/manual/manual_test.cpp`):
  - `kGoodService` stays a Title and Components (`manual: component role` uses it as is); a new `kServiceTail = "## Test points\nx\n## Troubleshooting\nx\n"` is appended where a passing manual is wanted (`manual: optional sections`, `manual: checks`' clean case). In `manual: checks`' bad case the extra `U1` pin 17 bullet goes before the tail, so it stays in Components and the line numbers hold.
  - Every User Manual text expected to give only reference errors, or none, gains `## How it works` appended after its body (line numbers hold): `manual: run and pin name checks`, `manual: references in table cells`, `manual: checks` (the `Connect +5 V` text and the `See \`J7\`` text).
  - `manual: checks`' `"# S\n## Components\nx\n"` gains the tail, so its first message stays the bullet's.
  - The messages: `What it does[, Using it], How it works` and `Components, Test points, Troubleshooting`.
  - `manual: optional sections`: `What it does` alone fails; `What it does, How it works` and `What it does, Using it, How it works` pass; `Using it` after `How it works` fails; a Service Manual without `Test points`, and one without `Troubleshooting`, fail.
  - Run in debug too (`-R manual`): the change is C++ logic.

In the same phase (a recipe fixed with the change it describes): `docs/recipes/ADD_MACHINE.md` §2: `How it works`, `Test points` and `Troubleshooting` required, `Using it` where there is something to operate; analyzer setups in `Test points` for the TV and computer Machines; the design doc's `## Manuals` as the Counter's.

## Docs (last phase)

- `docs/STYLE.md` Manuals: any rule the rollout found missing (each noted in the plan as it is found).
- `docs/ARCHITECTURE.md`: P6k done with the date and links; the Puzzle order moved to the Later of a row still open (P6 rows after P6k) or kept as an open note.
