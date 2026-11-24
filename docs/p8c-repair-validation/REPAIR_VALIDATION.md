# P8c — Repair Validation

Roadmap step P8c ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the game sees by itself that a Puzzle is completed, by the rule already decided (ARCHITECTURE.md, Repair validation: the player's Machine builds the same Circuit as the Golden Machine), and acknowledges it on the bench: a tick on the post-it, and once per bench a card that thanks the player and offers the next Puzzle. Design: [REPAIR_VALIDATION_DESIGN.md](REPAIR_VALIDATION_DESIGN.md). Plan: [REPAIR_VALIDATION_PLAN.md](REPAIR_VALIDATION_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

Since P3f the player decides when a Machine is fixed: nothing on the bench ever says so. A player who mends a Trace and sees the LED blink has no way to know whether the Machine is really repaired, or only looks it (a Stuck Cell never read, a second Fault on the same Net). P8c gives every one of the 114 Puzzles that answer, by one rule and nothing added to a `.bwp`, and a way on to the next Puzzle without going back to the Launcher.

## User decisions

From the roadmap row and ARCHITECTURE.md, Repair validation (the user, 2026-11-24):

- One rule for every Puzzle: no Fault left in a Component or a Joint, and the Copper gives the fault-free Copper's electrical Nets and Powered chips. Nets, not Copper: a Bodge Wire across a Broken Trace completes as a mend does.
- Never a false "completed"; the game never says "not repaired"; a working redesign is not seen.
- Each Puzzle's test checks it: not completed as loaded, completed after its Repair.
- "The Machine has shown it" is left for later.

Asked at the step's start (the user, 2026-11-24):

| Question | Decision |
|---|---|
| The acknowledgement | both: a tick on the post-it (kept for later, when progress is saved and the Launcher shows it), and a modal panel that congratulates the player, since a player zoomed into the Board would miss the post-it. The panel offers: back to the Board, the Launcher, the Machine's next Puzzle (when there is one) |
| How long the Machine runs first | 3 s of wall-clock time from the power-on |
| Afterwards | the tick follows the Board: gone when the player breaks the Machine again, back once it is repaired again (if not too much code: it is not). The panel only once per bench |
| The working Machine ("Start the Machine") | no acknowledgement: it has no Fault, so nothing to complete (and no post-it) |

The agent's view on the panel, given when it was asked: agreed. The panel is what tells the player; the tick is what stays. Points settled with it, chosen without asking (overturn any):

- **The panel is the owner's index card**, the Launcher's white "Start the Machine" card look (red rule, blue lines), centred on the bench over a light dimming, so the post-it and its new tick stay visible beside it.
- **The Machine keeps running** behind the card: the player sees it work while reading it.
- **Its words**: the heading "Repaired!", then "<the Puzzle's title>: it works again. Thank you!".
- **Its buttons**, in this order: "Back to the Board" (closes the card), "Next: <next Puzzle's title>" (only when the Machine has a next Puzzle), "Back to the Machines" (the Launcher). Mouse only, as the bench's other questions; Esc or a click outside does nothing (the card waits for a choice). A question already open (power cycle, leave, Reset) is answered first: the card waits for it.
- **Leaving from the card asks nothing**: the repairs on this Board were the point; the leave question (P7a) is skipped, for the Launcher as for the next Puzzle.
- **The next Puzzle** is the next file in the Machine's `puzzles/`, as the Launcher lists them; after the last Puzzle there is no Next button (not the next Machine). It opens a fresh bench, as from the Launcher. Back to the Machines after it selects that Puzzle in the Launcher, as any bench does.
- **The 3 s count** starts at each power-on, whatever the run switch (paused or `Max` still count: it is wall time), and restarts at the next one. Every Repair needs the power off, so a Machine is always judged on a run that started after its last Repair.
- **The tick follows the Board**: the verdict is worked out again after every Repair and Reset. A verdict "not completed" takes the tick off at once; "completed" brings it back only after a run of 3 s.
- **The tick on the post-it**: a ballpoint-blue tick and "Works again, thank you!" under the brief. A post-it put away in the Shelf stays put away: the card is what tells.
- **`bw run FILE.bwp --repair …`** prints one line on stderr, `bw: Puzzle completed` or `bw: Puzzle not completed`: the Puzzle author's check from the terminal (the developer's tool may say "not").

## What the player sees

### Completing a Puzzle

1. The player repairs the Machine (any tools, any order) and powers it on.
2. 3 s after that power-on, if the Machine now builds as the Golden Machine:
   - the post-it gets a blue tick and "Works again, thank you!" under its brief;
   - the first time on this bench only, the card opens in the middle of the bench, the bench dimmed lightly around it: "Repaired!", "<title>: it works again. Thank you!", and its buttons (§3).
3. The card's buttons:
   - **Back to the Board** closes it; the bench is as it was, the Machine still running;
   - **Next: <title>** closes the bench and opens the Machine's next Puzzle, as the Launcher would;
   - **Back to the Machines** shows the Launcher, the Puzzle just completed selected.
4. Nothing appears on a Machine not yet repaired, before the 3 s, or on the working Machine. Nothing says "not repaired".

### After it

5. The card never comes back on this bench, whatever the player does next.
6. The tick follows the Board: a Repair or a Reset that leaves the Machine not completed takes the tick off at once; repaired again, the tick comes back 3 s after the next power-on (without the card).
7. Leaving the bench forgets all of it: the Launcher shows no mark (P7: nothing saved yet).

### What counts as completed

8. Completed: no stuck pin, Stuck Cell or Column, Switch Fault or dry Joint left (each removed by its Repair: a swap, a resolder), and the Board's Copper joins the same pins into Nets, and powers the same chips, as the fault-free Board. So:
   - a Bodge Wire across a Broken Trace, or a mend, completes it alike;
   - a Cut left open, or a wire that joins two Nets, does not;
   - a good chip swapped, or a wire between two pads of one Net, changes nothing.

## What is new

1. **The verdict** (`src/machine`): `Machine::completed()`, from the Faults left and the Copper's `Wiring` (each electrical Net's name and pins, the Unpowered chips, the Dead Rail) against the fault-free Board's, worked out once, when first needed.
2. **The Session's verdict** (`src/session`): kept after each Repair and Reset, with whether the Machine was a Puzzle as loaded, and a count of power-ons.
3. **The acknowledgement's timing** (`src/view/repair_ack`, unit tested): from the verdict, the power and the wall clock, whether the tick shows and the frame the card opens.
4. **The next Puzzle** (`src/machine/machine_list`): `NextPuzzle`.
5. **The bench** (`src/ui`): the tick on the post-it, the card, the bench ending on the next Puzzle.
6. **`bw run`'s line** with `--repair`.
7. **Every Puzzle's test** checks the verdict as loaded and after its Repair; one test checks no Puzzle is completed as loaded.

Untouched: the kernel, Parts, the builder, the `.bwp` format, the Launcher (its lanes, its look), the Machines' files.

## Tests

- Unit: `electrical: …` (`WiringOf`), `machine: completed …` (as loaded, each kind of Repair, a Cut left open, a wire joining two Nets, a wire within a Net, a good chip swapped, the Reset, the working Machine), `session: completed …`, `repair ack: …`, `machine list: next puzzle …`.
- Machine cases: each of the 114 Puzzles' tests checks `completed()` false as loaded and true after its Repair; `repair validation: no Puzzle completed as loaded` over every `.bwp` (the BW-2001's and the BW-80's in release only).
- Screenshots (release, viewed): the Counter's Dark LED with `--repair` at `--after 4s` (the card over the bench, the post-it with its tick); the same at `--after 2s` (no card, no tick yet); the Counter's last Puzzle (no Next button); the BW-2001's Odd E repaired at `--after 4s` (the card over a running Machine).
- Speed: the verdict's cost per Repair on the BW-2001 (the largest Board), measured; the bench's UI time with the card open.
- Sanitizers: debug `machine:`, `session:`'s new cases, `repair ack:`, `machine list:`, the small Machines' Puzzle cases; debug `bw_ui` on the Counter's Dark LED repaired, the card open.
- Web build (the bench draws something new).
- Only the user: the card's words and look, clicking through Next on a few Machines.
- The full release suite.

## Later

- Saved progress: the tick kept across runs and shown on the Launcher's post-its (the user, 2026-11-24).
- "The Machine has shown it" (ARCHITECTURE.md, Repair validation): a second condition for the moment only.
- After a Machine's last Puzzle, an offer of the next Machine.
