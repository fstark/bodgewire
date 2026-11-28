# P8k — Wrong Clock — Design

How [WRONG_CLOCK.md](WRONG_CLOCK.md) is built. Plan: [WRONG_CLOCK_PLAN.md](WRONG_CLOCK_PLAN.md). Numbers in parentheses (§3) are the spec's items.

A new kind of Fault, by [ADD_PUZZLE.md § A new kind of Fault](../recipes/ADD_PUZZLE.md#a-new-kind-of-fault): a Component Fault like the Switch Fault (`StuckSwitch`), applied in the build after the parts are placed and removed by a swap. The Oscillator gains a period it can be given after construction. Seven Puzzle files with their test cases, by ADD_PUZZLE.md. Untouched: the kernel, the TV, the instruments, the UI, the random draw (`FaultKind`, `kFaultWeights`, `DrawFault`), the Vetting, every netlist and Board, the manuals.

## Files

| File | Change |
|---|---|
| `src/parts/oscillator.hpp`, `.cpp` | `setPeriod(Time)`, `period()` |
| `src/netlist/builder.hpp` | `WrongClock`; `BuildFaults::clocks` |
| `src/netlist/builder.cpp` | `Builder` takes the clocks and applies them after the switches; `#include "parts/oscillator.hpp"` |
| `src/machine/puzzle.hpp` | `ClockDecl`, last alternative of `FaultDecl` |
| `src/machine/puzzle.cpp` | the `fault clock` line; `clock` in the catch-all error; its `FormatFault` |
| `src/machine/machine.hpp` | `FaultSet::clocks`; `resolve(const ClockDecl&)`; the comments on `swap` and `completed` |
| `src/machine/machine.cpp` | `Add`, `resolve`, `swap`, `completed`, `build` |
| `tests/parts/oscillator_test.cpp` | `oscillator: setPeriod` |
| `tests/netlist/builder_test.cpp` | `builder: a Wrong Clock reaches the oscillator` |
| `tests/machine/puzzle_test.cpp` | `puzzle: clock`; the clock line in `puzzle: FormatFault of each kind`; the catch-all text in `puzzle: errors` (lines 96, 97, 114); the count in `puzzle: every Puzzle's faults format back` (121 → 124 in phase 2, → 128 in phase 3) |
| `tests/machine/machine_test.cpp` | `machine: clock fault errors`, `machine: a swap removes a Wrong Clock`; a clock in `machine: completed needs every Component and Joint Fault gone` |
| `tests/machine/machine_list_test.cpp` | `machine list: the repository`: the Counter 3 → 4 with its fourth title (phase 2), the BW-2001 16 → 17 (phase 3) |
| `tests/session/procedural_puzzle_test.cpp` | `Repair`: a `ClockDecl` is swapped (the visitor must cover every alternative) |
| `machines/010-counter/puzzles/03-quick-blink.bwp` (new) | §1 |
| `machines/040-stopwatch/puzzles/08-fast-seconds.bwp` (new) | §2 |
| `machines/045-knob-meter/puzzles/06-three-laps.bwp` (new) | §3 |
| `machines/080-message/puzzles/05-slanted-page.bwp` (new) | §4 |
| `machines/090-typewriter/puzzles/05-giant-letters.bwp` (new) | §5 |
| `machines/130-bw2001/puzzles/15-slanted-banner.bwp` (new) | §6 |
| `machines/140-bw80/puzzles/12-stray-cursor.bwp` (new) | §7 |
| `tests/machines/counter_test.cpp`, `stopwatch_test.cpp`, `knob_test.cpp`, `message_test.cpp`, `typewriter_test.cpp`, `bw2001_puzzles_test.cpp`, `bw80_puzzles_test.cpp` | one case a Puzzle; the name in `<machine>: Puzzles have a title and a brief` (`kPuzzles` where the file has it) |
| `machines/{010-counter,040-stopwatch,045-knob-meter,080-message,090-typewriter,130-bw2001,140-bw80}/docs/*_DESIGN.md` | the Puzzle in the Puzzle list; the count in the Files block |
| `machines/{…same seven…}/docs/{COUNTER,STOPWATCH,KNOB_METER,MESSAGE,TYPEWRITER,BW2001,BW80}.md` | the Puzzle in the Puzzles table; the count where written ("Six Puzzles", "Fifteen Puzzles") |
| `docs/recipes/ADD_PUZZLE.md`, `docs/GLOSSARY.md`, `docs/README_SPEC.md`, `README.md`, `docs/ARCHITECTURE.md` | §4 |

## 1. The Fault kind (§ What is new 1)

### `src/parts/oscillator.hpp`

```cpp
class Oscillator final : public Component {
public:
    Oscillator(std::string reference, Time period);  // Check(period >= 2)
    // Another period, as a Wrong Clock gives it (P8k): before powerOn. Check(period >= 2).
    void setPeriod(Time period);
    Time period() const { return highTime_ + lowTime_; }
    ...
};
```

`setPeriod` splits the period as the constructor does (`highTime_ = period / 2`, `lowTime_ = period - period / 2`). The builder calls it before `finalize()`, so before any `powerOn`.

### `src/netlist/builder.hpp`

```cpp
// Wrong Clock (P8k): the oscillator `reference` runs at `period`, whatever its freq=.
struct WrongClock {
    std::string reference;
    Time period;
};

struct BuildFaults {
    ...
    std::span<const StuckSwitch> switches;
    std::span<const WrongClock> clocks;
};
```

### `src/netlist/builder.cpp`

`Builder`'s constructor takes `std::span<const WrongClock> clocks` after the switches (and `BuildCircuit` passes `faults.clocks`). In `build()`, after the loop over `switches_`:

```cpp
for (const WrongClock& c : clocks_) {
    const auto it = loaded_->components.find(c.reference);
    auto* oscillator =
        it != loaded_->components.end() ? dynamic_cast<Oscillator*>(it->second) : nullptr;
    Check(oscillator != nullptr, "builder: Wrong Clock on a Part that is not an oscillator");
    oscillator->setPeriod(c.period);
}
```

`LoadedPart::marking` still comes from the netlist's `freq=` (the loop above it reads `decl.params`), and the Board draws the design circuit's marking (`BoardScene`), so the can reads its design frequency (user decision). A clock on an unpowered can changes nothing: the can is out of the Circuit either way.

### `src/machine/puzzle.hpp`, `puzzle.cpp`

```cpp
struct ClockDecl {
    std::uint32_t line;
    std::string reference;  // an OSC
    std::string frequency;  // as written: "3MHz", "1.9MHz"
};

using FaultDecl = std::variant<StuckAtDecl, CrackDecl, BridgeDecl, JointDecl, CellDecl, SwitchDecl,
                               ClockDecl>;
```

Last in the variant, so the indices `procedural_puzzle_test.cpp`'s `KindOf` uses stay. The parser's dispatch gains `tokens[1] == "clock"` beside `"switch"`; `clockFault(line, tokens)`: exactly four tokens, else `expected: fault clock REF FREQUENCY`; the frequency through `ParseFrequencyPeriod`, its error reported as is (`bad frequency '3Mhz': expected a number then Hz, kHz or MHz, e.g. 14.31818MHz`). The catch-all error (`expected: fault stuck|crack|bridge|joint|cell|column|switch ...`, `puzzle.cpp:110`) gains `clock`. `FormatFault`: `fault clock {reference} {frequency}`, the text as written, so a file formats back to itself.

### `src/machine/machine.hpp`, `machine.cpp`

- `FaultSet` gains `std::vector<WrongClock> clocks;`.
- `Add(FaultSet&, const WrongClock&)`: in place of an earlier one on the same can, as `StuckSwitch`'s.
- `resolve(const ClockDecl&) -> std::expected<WrongClock, std::string>`: `unknown reference 'X9'`; a Part other than `OSC` (`part->info->name`): `clock U3: oscillators only`; the period from `ParseFrequencyPeriod`, its error returned as `clock X1: ` + the error (`inject` and `check` take a `ClockDecl` the parser never saw); equal to the design can's (`dynamic_cast<const Oscillator*>` on `designCircuit_->components`, `period()`): `clock X1: 1MHz is its own frequency`. No rule on faster cans (spec: chosen without asking).
- `swap(reference)` also erases `faults_.clocks` of that reference; its comment in `machine.hpp` names the Wrong Clock.
- `completed()`: `!faults_.clocks.empty()` joins the Component Faults that must be gone; its comment too.
- `build()` passes `.clocks = faults_.clocks`.

`check`, `inject`, `undoRepairs`, `clearFaults` and `LoadMachine` need nothing (they visit `resolve` or copy `FaultSet`). Not a `FaultKind`: `DrawFault` never makes one (user decision).

### Tests

- `oscillator: setPeriod`: an Oscillator built at 1 MHz, `setPeriod(333'333)`, run: rises 333 333 ps apart, `period()` the new one; `CHECK_THROWS(osc.setPeriod(1))` (`Check` throws `std::logic_error`).
- `builder: a Wrong Clock reaches the oscillator`: an inline netlist with `part X1 OSC freq=1MHz`, `BuildCircuit(…, {.clocks = …})` at 333 333 ps: rises on `X1.OUT` that far apart; `LoadedPart` X1's marking still `1MHz`.
- `puzzle: clock`: `fault clock X1 3MHz` parsed (reference, frequency, line); errors: `fault clock X1` (arity), `fault clock X1 3Mhz` (the frequency error). `puzzle: FormatFault of each kind`: `ClockDecl{0, "X1", "1.9MHz"}` → `fault clock X1 1.9MHz`.
- `machine: clock fault errors` (`LedComputer()`, as the switch case: its X1 is `OSC freq=1MHz`, its U3 a chip): `X9` unknown, `U3` oscillators only, `1MHz` its own frequency, `ClockDecl{0, "X1", "3Mhz"}` the frequency error; `check` gives the same texts.
- `machine: a swap removes a Wrong Clock` (`LedComputer()`): `fault clock X1 3MHz` injected; `swap("U3")` keeps it (not completed); `swap("X1")` removes it (completed); `undoRepairs()` brings it back.
- `machine: completed needs every Component and Joint Fault gone`: a `ClockDecl` among the Faults listed.

## 2. The Puzzles (§1–§7)

By ADD_PUZZLE.md (file, line 1 comment, title, brief, tests, docs). Each file goes in the ladder before the double faults, the later files renumbered (spec: the user's decisions). Each test case:

1. `LoadPuzzle("<name>")`, `CHECK_FALSE(machine.completed())`;
2. the symptom (below) and X1's period: its rises in a short record of `X1.OUT` are the Fault's period apart (3 MHz: 333 333 ps);
3. the tempting wrong swap (below), `CHECK_FALSE(machine.completed())`;
4. `machine.swap("X1")`, `CHECK(machine.completed())`, the fault-free behaviour back through the file's existing helper.

| § | Puzzle | Line | Symptom checked | Wrong swap | Back |
|---|---|---|---|---|---|
| 1 | Counter `quick_blink` | `fault clock X1 3MHz` | `CARRY1` rises 5 333 328 ps apart (16 × 333 333); `C19` changes at least twice in 400 ms (its half-period 174.8 ms; fault-free 524 ms) | `U1` | `CARRY1` 16 µs apart |
| 2 | Stopwatch `fast_seconds` | `fault clock X1 3MHz` | `StartStop` (`stopwatch_test.cpp:159`: Start at 2 ms, Stop at 125.4 ms) reads `0.37` | `U1` (the prescaler; Troubleshooting step 6) | `StartStop` reads `0.12` |
| 3 | Knob Meter `three_laps` | `fault clock X1 3MHz` | `ReadingAt`: 25 % reads 75, 40 % reads 19 (exact on seeds 1–3) | `U2` (the count clock `F10K`) | 25 % reads 25 |
| 4 | Message `slanted_page` | `fault clock X1 1.9MHz` | seeds 1–3: the TV not locked at 300 ms | `U1` (the dot counter; Troubleshooting step 2, `U1.RCO`) | `CheckRepaired(machine)` |
| 5 | Typewriter `giant_letters` | `fault clock X1 2MHz` | seeds 1–3: the TV not locked at 300 ms | `U1` (the dot counter; Troubleshooting steps 1–2) | `CheckShown`, or `Typing(machine, "HELLO\r")` with `codes() == Typed("HELLO")` (`typewriter_test.cpp:439–457`) |
| 6 | BW-2001 `slanted_banner` | `fault clock X1 7.6MHz` | seeds 1–3: the TV not locked at 200 ms, as `no_clock`; release only | `UC9` (the character divider; Troubleshooting step 4) | `Banner(m)`, as `no_clock` |
| 7 | BW-80 `stray_cursor` | `fault clock X1 3.25MHz` | seed 1: the TV not locked after 1 s (the CPU runs at half speed, so the K screen's display file needs about 900 ms: the case's own loop, not `CheckUnlocked`, which expects the display file at 500 ms); release only | `IC18` (Troubleshooting step 1, `IC18.2Q`) | `CheckKScreen` |

`[slow]` where a case simulates more than about 1 s. The BW-2001 and BW-80 cases `SKIP` in debug, as their file's cases do.

Titles and briefs: the spec's drafts, checked against each Puzzle's `bw_ui --screenshot` and the Launcher's post-it size (about 160 characters); the BW-80's written from `bw_ui`.

The Machine docs: in `*_DESIGN.md`, the Puzzle's row in the Puzzle list (file, line, why it shows) and the count in the Files block (Counter 3 → 4, Stopwatch 8 → 9, Knob Meter 7 → 8, Message 6 → 7, Typewriter 6 → 7, BW-2001 16 → 17, BW-80 15 → 16); in `NAME.md`, its row in the Puzzles table and the count where one is written.

## 3. Speed

Each new Puzzle's speed ratio (`bw run FILE --time 2s`, release) in the plan's notes. The 3× three must keep at least 2.5× natively (above real time in the web build, 0.4× native): measured Counter 4.2×, Stopwatch 3.7×, Knob Meter 6.2×. The four TV ones run at about their Machine's fault-free speed or faster (Message 2.7–3.0× against 2.86×, BW-2001 0.40–0.44× against 0.41×: within the noise).

## 4. Docs

- **ADD_PUZZLE.md**:
  - in the Faults list, `fault clock X1 3MHz`: the oscillator can runs at that frequency, its marking unchanged; repaired by swapping the can. Faster only where the Machine keeps at least 2.5× real time natively at the new rate (the Counter, Stopwatch, Knob Meter); slower anywhere (a TV Machine loses its lock: tearing welcome); never drawn at random;
  - in § A new kind of Fault, the steps P8k found missing: `procedural_puzzle_test.cpp`'s `Repair` visitor covers every alternative even of a kind never drawn (else it does not compile); the parser's catch-all error names the new kind (and `puzzle: errors` checks its text);
  - in § Tests, the hard-coded counts a new Puzzle changes: `puzzle: every Puzzle's faults format back` (every `fault` line) and `machine list: the repository` (the Counter's and BW-2001's Puzzles); in § Docs, the Machine's `NAME.md` Puzzles table and the README's per-Machine counts.
- **GLOSSARY.md**, after Switch Fault: **Wrong Clock**: a Fault of an oscillator can: it runs at another frequency than it is marked with. Repaired by a swap. _Avoid_: Slow clock, bad crystal.
- **README_SPEC.md**, then **README.md**: 118 → 125 Puzzles (the sentence and README_SPEC's example at line 41); each Machine's count (README.md 154, 174, 181, 208, 215, 236, 242); a wrong clock in the list of fault kinds met (README.md:146, README_SPEC.md:131). No picture changes.
- **ARCHITECTURE.md**: the P8k row after P8j, planned now (with the docs' commit), done at the end with the date, one line and the three links.

## Checks

- Phase 1: debug `ctest --preset debug -j -R "^(oscillator|builder|puzzle|machine): |^procedural puzzle: [a-z]"` (the second part keeps the per-Machine Vetting cases, named after a directory that starts with a digit, out); release, the full suite (shared code).
- Phase 2: release `ctest --preset release -j -R "counter|stopwatch|knob|machine list|repair validation|puzzle"`; the three speed ratios; screenshots of the three benches, the Counter's with `--hook X1.OUT`.
- Phase 3: release `ctest --preset release -j -R "message|typewriter|bw80|bw2001|machine list|repair validation|puzzle"`; the four speed ratios; screenshots of the four benches, viewed against the spec's frames.
- Phase 4: the full release suite; a Launcher screenshot on a new post-it.
