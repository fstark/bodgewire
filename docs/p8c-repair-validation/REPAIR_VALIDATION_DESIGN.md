# P8c — Repair Validation — Design

How [REPAIR_VALIDATION.md](REPAIR_VALIDATION.md) is built. Plan: [REPAIR_VALIDATION_PLAN.md](REPAIR_VALIDATION_PLAN.md). Numbers in parentheses (§3) are the spec's items.

Untouched: the kernel, Parts, the builder, the `.bwp` parser, the Launcher's lanes and look, the Machines' files. New: the verdict in `src/machine` (`Wiring`, `Machine::completed`, `NextPuzzle`), the Session's copy of it, the acknowledgement's timing in `src/view/repair_ack`, the card in `src/ui/thank_you_card`. Changed: the post-it (its tick), `RunBench` (the card, ending on the next Puzzle), `bw run` (one line), every Puzzle's test.

## Files

| File | Change |
|---|---|
| `src/machine/electrical.hpp`, `.cpp` | `Wiring`, `WiringOf` |
| `src/machine/machine.hpp`, `.cpp` | `golden_` (the fault-free Board's `Wiring`, at load), `completed()` |
| `src/machine/machine_list.hpp`, `.cpp` | `NextPuzzle` |
| `src/session/session.hpp`, `.cpp` | `completed()`, `puzzle()`, `powerOns()` |
| `src/view/repair_ack.hpp`, `.cpp` (new) | `RepairAck`: the tick and the card's moment |
| `src/view/CMakeLists.txt` | `repair_ack.cpp` in `bw_view` |
| `src/ui/brief_window.hpp`, `.cpp` | `DrawBrief(…, bool ticked)`; the index card's colours shared (`kCardPaper`, `kCardRed`, `kCardBlue`, `kCardShadow`) |
| `src/ui/launcher.cpp` | the card colours from `brief_window.hpp` |
| `src/ui/thank_you_card.hpp`, `.cpp` (new) | `ThankYouCard`: the modal card |
| `src/ui/CMakeLists.txt` | `thank_you_card.cpp` |
| `src/ui/main.cpp` | `RunBench`: `RepairAck`, the card, `BenchEnd::Next`; `main`: the next Puzzle's bench |
| `src/cli/main.cpp` | `bw run … --repair`: the completed line |
| `tests/machine/electrical_test.cpp` | `electrical: wiring …` |
| `tests/machine/machine_test.cpp` | `machine: completed …` |
| `tests/machine/machine_list_test.cpp` | `machine list: next puzzle …` |
| `tests/session/session_test.cpp` | `session: completed …` |
| `tests/view/repair_ack_test.cpp` (new) | `repair ack: …` |
| `tests/machines/repair_validation_test.cpp` (new) | `repair validation: …` |
| `tests/machines/*_test.cpp` (16 files, §7) | each Puzzle's case: the verdict as loaded and after its Repair |
| `tests/CMakeLists.txt` | the two new test files |
| `docs/GLOSSARY.md`, `docs/ARCHITECTURE.md`, `docs/GAME_DESIGN.md`, `docs/recipes/ADD_PUZZLE.md`, `docs/README_SPEC.md`, `tools/readme_shots.py`, `README.md`, `docs/readme/repaired.png` | §8 |

## 1. The verdict (§8)

### `src/machine/electrical.hpp`

```cpp
// What a Circuit is built from on the Copper's side (P8c): each electrical Net's name and pins,
// and the chips the Copper leaves Unpowered. Two Boards of one design with equal Wiring build the
// same Circuit, whatever Copper made it (a mend or a Bodge Wire).
struct Wiring {
    // Each Net's name and its pins ("U3.7", "R2.1") sorted; the Nets sorted by name.
    std::vector<std::pair<std::string, std::vector<std::string>>> nets;
    std::vector<std::string> unpowered;  // BoardPower's, design order
    bool deadRail = false;
    bool operator==(const Wiring&) const = default;
};

// ElectricalNetlist (no dry Joints: they are Faults of their own) of `board`, whose islands are
// `islands`, and its `power`.
Wiring WiringOf(const Netlist& design, const LoadedCircuit& designCircuit, const Board& board,
                const Islands& islands, const BoardPower& power);
```

`ElectricalNetlist` gains an overload taking the `Islands` (the current one calls `FindIslands` and forwards): `FindIslands` is nearly all of its cost (BW-2001: 54 of 56 ms in release, the review's measure), and `Machine::rebuildBoard` already works them out.

Net names are kept: the builder reads them (the Nets named `VCC` and `GND` are tied to the supplies, a resistor's pull direction, a capacitor's and a switch's rail, `builder.cpp`), and `ElectricalNetlist` names those two only from the POWER connector's islands. A rail island cut from `J1` but still reaching every chip's power pin keeps its pins and its Powered chips, but becomes `VCC~2`, untied: a different Circuit, which only the name shows. With equal partitions the names are equal (connection order, then the POWER presets), so a wire between two pads of one Net still changes nothing. Passive pins are in (a resistor bridged across is left out by `ElectricalNetlist`, so its pins vanish: not the Golden Machine). Power pins are not in `ElectricalNetlist`'s Nets: `unpowered` and `deadRail` carry them (a cracked VCC stub leaves the Nets alone but one chip Unpowered).

### `src/machine/machine.hpp`

```cpp
    // Repair Validation (P8c): the Machine builds the Golden Machine's Circuit. No stuck pin,
    // cell, column, switch Fault or dry Joint left, and the current Copper's Wiring is the
    // fault-free Board's. Cracks, Solder Bridges, Cuts and Bodge Wires count only through it.
    bool completed() const;
private:
    Islands islands_;  // of board_, kept by rebuildBoard (it worked them out for power_ already)
    // The fault-free Board's Wiring, worked out at the first completed() that needs it (a
    // working Machine, or a Puzzle with a Component Fault left, never does).
    mutable std::optional<Wiring> golden_;
```

`completed()`:

1. a stuck pin, cell, column, switch Fault or dry Joint left: false;
2. no Crack, Solder Bridge, Cut or Bodge Wire (`board_` is `pristine_`): true, nothing worked out;
3. else `WiringOf(design_, *designCircuit_, board_, islands_, power_) == golden()`, `golden()` filling `golden_` from `pristine_` once (`FindIslands(pristine_)`, `FindPower`).

`completed()` is called after a Repair, never per frame. Its cost is one `ElectricalNetlist` without `FindIslands` (a few ms on the BW-2001), plus one `FindIslands` of the pristine Board the first time. Measured in phase 1 (a `[.perf]` case, `repair validation: verdict time`), expected under one `FindIslands`. `golden_` is filled on the UI thread (or the Launcher's loading thread, the Session's construction): one Machine is never shared between threads.

The ARCHITECTURE.md rule "A new kind of Fault must act through what `build()` reads" holds: a Fault kind added to `FaultSet` must be added to `completed()`'s list too (ADD_PUZZLE.md, A new kind of Fault, gains the line).

### Tests (`tests/machine/electrical_test.cpp`, `machine_test.cpp`, `[machine]`)

- `electrical: wiring of the fault-free Board`: the Test Pattern's `WiringOf` on the pristine Board twice is equal; nothing Unpowered; one entry per design Net with a non-Power Pin, under the design's name.
- `electrical: wiring sees a rail renamed`: the `pull-up cut from VCC…` Board (R1.1 on `VCC~2`) differs from the pristine's.
- `electrical: wiring sees a crack and a dead VCC stub`: the H0 crack (`H0Crack`) gives a different `nets`; a cracked VCC stub (`power: cracked VCC stub…`'s Board) the same `nets`, a different `unpowered`.
- `machine: completed`, on the Test Pattern Puzzles (cheap, no simulation), one `SECTION` each:
  - the working Machine (`LoadNetlistMachine`) is completed;
  - `broken_trace` not as loaded; completed after the mend, and after a Bodge Wire `U1.14`–`U13.6` instead; not after a Cut elsewhere on a good Trace left open, completed again once mended;
  - `stuck_gate` not as loaded; a good chip swapped first changes nothing; the faulty one's swap completes it;
  - `line_bridge` (a Solder Bridge) completed by `removeBridge`;
  - `unpowered_chip`, `dead_rail` completed by their Repairs (their tests' Repairs);
  - after a completing Repair, a wire joining two Nets (`U1.14` to `U2.1`, two different Nets) makes it not completed; a wire between two pads of one Net does not;
  - `undoRepairs` brings it back to not completed.
- `machine: completed needs every Component and Joint Fault gone`: `InlinePuzzle` with a dry Joint, a Stuck Cell and a switch Fault (LED Computer, Dice): not completed until `resolder` and the swaps.

## 2. The Session's verdict (§2, §6)

### `src/session/session.hpp`

```cpp
    // Repair Validation (P8c): the Machine as repaired now builds the Golden Machine's Circuit;
    // worked out at construction, after each Repair Done and each Reset. False without a Machine.
    bool completed() const { return completed_; }
    // Not completed as loaded: a Puzzle (the working Machine, or no Machine, is not).
    bool puzzle() const { return puzzle_; }
    // powerOn calls so far: a new run for the acknowledgement's count.
    std::uint64_t powerOns() const { return powerOns_; }
private:
    bool completed_ = false;
    bool puzzle_ = false;
    std::uint64_t powerOns_ = 0;
```

The constructor sets `completed_ = machine_ && machine_->completed()`, `puzzle_ = machine_ && !completed_` (free for a working Machine: `completed()`'s step 2). `repair` (when Done) and `undoRepairs` set `completed_` again. `powerOn` increments `powerOns_`. The move constructor is defaulted: the new members move with it.

### Tests (`tests/session/session_test.cpp`, `[session]`)

- `session: completed follows the Repairs`: `PuzzleSession("broken_trace")`: `puzzle()`, not `completed()`; powered off, a wire repair: `completed()`; `undoRepairs`: not; `powerOns()` counts each `powerOn`.
- `session: the working Machine is no Puzzle`: a Session on `LoadNetlistMachine` of the Counter: `completed()`, not `puzzle()`.

## 3. The acknowledgement's timing (§2, §4–§6)

### `src/view/repair_ack.hpp` (new)

```cpp
// When the bench acknowledges a completed Puzzle (P8c): the post-it's tick, and the frame the
// Thank-you Card opens (once per bench). Wall clock, from a power-on: the player sees the Machine
// run before being told.
class RepairAck {
public:
    static constexpr double kRunSeconds = 3;

    explicit RepairAck(bool puzzle);  // Session::puzzle(): false never acknowledges
    // Each frame: Session::completed(), powered(), powerOns(), and the wall clock in s.
    void update(bool completed, bool powered, std::uint64_t powerOns, double now);
    bool ticked() const { return ticked_; }
    // True once per bench: on the first call after the first tick.
    bool takeCard();

private:
    bool puzzle_;
    std::uint64_t powerOns_ = 0;
    std::optional<double> since_;  // the current run's power-on; nullopt while off
    bool ticked_ = false;
    bool cardDue_ = false;
    bool cardGiven_ = false;
};
```

`update`: nothing without a Puzzle. A new `powerOns` value starts the count (`since_ = now`); `!powered` clears it. `!completed` takes the tick off at once. Completed with `now - *since_ >= kRunSeconds` ticks it, and the first tick of the bench makes the card due. The power-on count, not the power edge, starts a run: the bench's power cycle (off, Repair, on at the next frame) may never show `update` an unpowered frame.

### Tests (`tests/view/repair_ack_test.cpp`, cases `repair ack: …`, `[view]`)

- `no tick before 3 s, then the tick and the card once`;
- `the working Machine never`: `RepairAck(false)`, completed, powered for 10 s;
- `not completed, never`;
- `a power-on restarts the count`: a new `powerOns` at 2.5 s, completed: no tick at 4 s, a tick at 5.5 s;
- `power off before 3 s: no tick`;
- `the tick follows the verdict, the card does not come back`: ticked, then not completed (tick off), then completed with a new run: ticked again after 3 s, `takeCard()` false;
- `the tick stays with the power off`.

## 4. The next Puzzle (§3)

### `src/machine/machine_list.hpp`

```cpp
// The Puzzle after `file` among its Machine's, as listed (P8c's card); nullptr for the Machine's
// last Puzzle, its working Machine or a file not in the list.
const PuzzleEntry* NextPuzzle(const MachineList& list, const std::filesystem::path& file);
```

Through `EntryOf`.

### Tests (`tests/machine/machine_list_test.cpp`)

- `machine list: next puzzle`: in the repository's list, the Counter's `01-dark-led.bwp` → `02-frozen-led.bwp`; `03-too-fast.bwp` → nullptr (not the Chaser's first); `counter.bwn` → nullptr; an unknown file → nullptr.

## 5. The bench (§1–§7)

### `src/ui/brief_window.hpp`, `.cpp`

`DrawBrief(bool& open, const OpenedMachine& loaded, std::optional<ImVec2> corner, BriefNote& note, bool ticked)`: with `ticked`, under the brief, a spacing, then a tick drawn as two strokes (2 px, `kBallpoint` `IM_COL32(35, 60, 150, 255)`, the height of a line) and "Works again, thank you!" in the same blue beside it. The window's auto-resize makes the note grow.

The index card's colours move here from `launcher.cpp` as `inline constexpr ImU32 kCardPaper, kCardRed, kCardBlue, kCardShadow`: the Launcher's "Start the Machine" and the Thank-you Card share them.

### `src/ui/thank_you_card.hpp` (new)

```cpp
enum class CardAnswer : std::uint8_t { None, Board, Next, Machines };

// The owner's index card thanking the player for a completed Puzzle (P8c §2–§3): a modal in the
// main window's middle, the bench lightly dimmed; it waits for a button.
class ThankYouCard {
public:
    void open();  // from the next draw
    // Each frame, last (above every window). `title`: the Puzzle's; `next`: the next Puzzle's
    // title, nullptr for none (no Next button); `canLeave`: Back to the Machines offered.
    CardAnswer draw(const std::string& title, const std::string* next, bool canLeave);

private:
    bool opening_ = false;
};
```

`draw`: `ImGui::OpenPopup("##thank you")` when opening; `ImGuiCol_ModalWindowDimBg` pushed at `IM_COL32(0, 0, 0, 60)`; the window placed at the main viewport's centre (`pivot {0.5, 0.5}`, `ImGuiCond_Always`), width 360 px, `BeginPopupModal(id, nullptr, NoTitleBar | NoMove | NoResize | AlwaysAutoResize | NoSavedSettings)`, its background transparent and the card drawn on the window's draw list at its rectangle (shadow, `kCardPaper`, a `kCardRed` rule under the heading, `kCardBlue` lines under the text, as `DrawPostIt`'s card in `launcher.cpp`). Contents: "Repaired!" at 1.6× the font size in `kNoteInk`, the line "<title>: it works again. Thank you!" wrapped, then the buttons on one row: "Back to the Board", "Next: <next>" (with `next`), "Back to the Machines" (with `canLeave`). A click closes the popup (`CloseCurrentPopup`) and returns its answer. Esc and clicks outside do not close a modal without `p_open` (ImGui 1.93: `NavUpdateCancelRequest` skips modals; `ClosePopupsOverWindow` stops at the top modal), so the card waits; if ImGui closes it anyway, the answer is `Board`. The project's first `BeginPopupModal`. Viewports are on: a popped-out window (P6j) is dimmed with the style's `ModalWindowDimBg` (ImGui's default, not the pushed one) and blocked while the card is up; accepted.

`open()` while another popup is open (a power-cycle, leave or Reset question) would close it (`OpenPopupEx` closes popups at its level), so `RunBench` defers the card: it opens at the first frame no popup is open (`!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)`).

### `src/ui/main.cpp`

- `enum class BenchEnd { Quit, Back, Next }`; `BenchResult` gains `std::optional<std::filesystem::path> next` (with `Next`).
- `RunBench(…, const MachineList* list)` in place of `bool canLeave`: `canLeave = list != nullptr`; `const PuzzleEntry* next = list ? NextPuzzle(*list, *options.file) : nullptr` once at start.
- In the bench: `RepairAck ack(session.puzzle())`; each frame, after the power-on of `powerOnNext` and before the windows, `ack.update(session.completed(), session.powered(), session.powerOns(), seconds)` (`std::chrono::steady_clock`, seconds since the bench started); `cardDue |= ack.takeCard()`, and `if (cardDue && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) { card.open(); cardDue = false; }`; `DrawBrief(…, ack.ticked())`. `card.draw(loaded.title, next ? &next->title : nullptr, canLeave)` after the other popups (last, above every window); `Next` ends the bench with `BenchEnd::Next` and the file; `Machines` sets `leaving` (no leave question); `Board` nothing.
- `main`: `RunBench(…, machinesDir ? &launcher.list() : nullptr)`; a result `Next` keeps the file in `std::optional<std::filesystem::path> next`, and the loop's next turn takes it before the Launcher: no launcher refresh, `Load(file)` (on failure, the Launcher with its error line, as today), a fresh `Options{}` with `file` and `machines` only (no `visitFor`, no screenshot). `left` becomes that file after its bench (`left = *benchOptions->file`, as today), so Back afterwards selects it.
- `--visit` benches and `--screenshot` are unchanged: a screenshot at `--after 4s` of a repaired Puzzle shows the card.

### Checks (no unit test reaches `src/ui`): screenshots (§Checks).

## 6. `bw run`'s line

`src/cli/main.cpp`, `LoadPuzzle`: after the `--repair` list is applied, with a non-empty list, `std::println(stderr, "bw: Puzzle {}completed", machine->completed() ? "" : "not ")`.

## 7. Every Puzzle's test (spec, Tests)

In each Puzzle's `TEST_CASE` (ADD_PUZZLE.md, Tests): `CHECK_FALSE(machine.completed())` right after `LoadPuzzle`, `CHECK(machine.completed())` right after the Repair(s), before the fault-free behaviour is checked. Through a Session: `session.machine()->completed()`. A shared helper also called with a wrong Repair on purpose (`board_repair_test.cpp`'s `Repaired`, `shorts_power_test.cpp`'s `CheckRepairs`, the `faults:` cases' wrong Machines) takes the expected verdict as a parameter, or checks only at its right-Repair call sites: a wrong Repair is checked not completed. A Puzzle whose case repairs nothing gets its Repair (the one the Machine design's Puzzle list names). The 114 Puzzles' cases are in `counter`, `chaser`, `dice`, `stopwatch`, `knob`, `test_pattern_faults`, `board_repair`, `shorts_power` (the Test Pattern's `sync_bridge`, `line_bridge`, `unpowered_chip`, `dead_rail`), `ball`, `tennis`, `message`, `typewriter`, `led_computer`, `bw1_puzzles`, `bw2001_puzzles`, `bw80_puzzles` `_test.cpp` (16 files).

A Puzzle that turns out completed as loaded (a crack on a Trace with a second path, say) is a Puzzle bug: reported to the user, not worked around.

### `tests/machines/repair_validation_test.cpp` (new, `[machines]`)

- `repair validation: no Puzzle completed as loaded`: every `.bwp` of `ListMachines(test::RootDir() / "machines")` (no problems), `LoadPuzzleFile` with `INFO(file)`, `CHECK_FALSE(completed())`. In the debug build the BW-2001's and BW-80's are skipped (`#ifndef NDEBUG`, by the Machine's netlist stem, as `bw2001_puzzles_test.cpp` skips its cases), keeping the case short there: debug covers the other 83.
- `repair validation: every working Machine is completed`: each Machine's `LoadNetlistMachine`.
- `repair validation: verdict time` (`[.perf]`): the BW-2001's `odd_e` swapped, `completed()` timed over 20 calls, the mean printed.

## 8. Docs

- GLOSSARY: Repair Validation (built in P8c: the tick and the card, 3 s after a power-on); Brief (the tick); a new **Thank-you Card** entry; Golden Machine (the Circuit a completed Puzzle builds).
- ARCHITECTURE: Repair validation's first line ("Built in P8c, 2026-11-24"), the wall-clock 3 s in "When it shows"; the P8c row done, one line with links.
- GAME_DESIGN: the Loop ends "→ the game acknowledges it".
- ADD_PUZZLE.md: Tests, steps 1 and 2 check `completed()`; A new kind of Fault: add it to `Machine::completed()`'s list.
- README_SPEC §6: a sixth step `repaired` (3 s after the power-on, the owner's card thanks you); `tools/readme_shots.py` gains the shot (`mended`'s flags at `--after 4s`); that shot taken alone (`python3 -I tools/readme_shots.py repaired`) and the README regenerated.

## Checks

- Phase fences: release `ctest -R "^(electrical|power|machine|machine list|session|repair ack|repair validation): "` and the Puzzle cases (`-R "^(counter|chaser|dice|stopwatch|knob|faults|board repair|shorts power|ball|tennis|message|typewriter|led_computer|bw1|bw2001|bw80): "`). Debug: the unit cases (`electrical|power|machine|machine list|repair ack`, `session: (completed|the working)`, never `session:` whole: its BW-2001 and BW-80 cases) and the small Machines' Puzzle cases only (`counter|chaser|dice|stopwatch|knob|faults|board repair|shorts power|repair validation`): tennis, ball, BW-1, typewriter and the large Machines cost about an hour of debug CPU.
- Screenshots (release, viewed):
  - `bw_ui machines/010-counter/puzzles/01-dark-led.bwp --repair mend:F.Cu:192.6:121.96 --screenshot /tmp/p8c-card.png --after 4s`: the card ("Next: Frozen LED"), the post-it with its tick beside it;
  - the same at `--after 2s`: no card, no tick;
  - the same with `--repair wire:U5.QD:D1.1` (a Bodge Wire across the crack) at `--after 4s`: the card;
  - `machines/010-counter/puzzles/03-too-fast.bwp --repair swap:U1 --after 4s`: no Next button;
  - `machines/010-counter/counter.bwn --after 4s`: no card;
  - `machines/130-bw2001/puzzles/01-odd-e.bwp --repair swap:UA2 --after 5s`: the card over the running BW-2001; its speed ratio and UI ms against the same without `--repair`.
- Debug `bw_ui` on the Dark LED repaired (`--after 4s`, the card open): no sanitizer report. The card's Next and Back to the Machines are clicked by the user only; what follows them (a fresh bench after a bench, the Launcher after a bench) is the path `--visit` already runs under the sanitizers.
- Web build (`cmake --preset web && cmake --build --preset web`).
- `bw run machines/010-counter/puzzles/01-dark-led.bwp --repair mend:F.Cu:192.6:121.96` prints `bw: Puzzle completed`; with `cut:` elsewhere instead, `not completed`.
- The full release suite at the end.
