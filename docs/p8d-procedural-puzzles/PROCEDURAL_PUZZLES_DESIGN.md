# P8d — Procedural Puzzles — Design

How [PROCEDURAL_PUZZLES.md](PROCEDURAL_PUZZLES.md) is built. Plan: [PROCEDURAL_PUZZLES_PLAN.md](PROCEDURAL_PUZZLES_PLAN.md). Numbers in parentheses (§3) are the spec's items under What the player sees; "block 4" is an item of its What is new; D4 is this design's section 4.

Untouched: the kernel, Parts, the builder, the `.bwp` format, the Authored Puzzles and their tests, `Machine::completed()`. New: a Vetting Script per Machine (`puzzles/random.bwr`), a Fault drawn at random, the Vetting, the loop that makes a Puzzle, a `bw` command, a post-it in the Launcher and a page in the binder. Changed: the inputs `bw run` plays move into a library and a knob can be set at a time; `Machine` learns to check a Fault without taking it and to drop its Faults; the Launcher, the bench and the card learn an entry that is not an Authored Puzzle.

Words: the player reads "Random fault" (the post-it's title); the code and the docs say Procedural Puzzle (GLOSSARY) for the Puzzle made, Vetting for the check, Vetting Script for a Machine's `random.bwr`. "Capture" alone is the Logic Analyzer's (GLOSSARY): here it is always a "capture time", and what is taken there is a `Shown`.

One choice shapes the UI's part: **a Procedural Puzzle's identity is the path of its Machine's Vetting Script**. Everything that names a bench today is a path (`Launcher::entryFile`, `LauncherResult::file`, `BenchResult::next`, `Options::file`, `EntryOf`, `MachineDir`): with the script's path as the entry, that plumbing stays, `bw_ui machines/010-counter/puzzles/random.bwr` opens one from the terminal, and "Another random fault" is the card's Next on the same path.

## Files

| File | Change |
|---|---|
| `src/machine/knob_setting.{hpp,cpp}` | `KnobSetting::at`; `ParseKnob` takes `J2:87%:400ms` |
| `src/machine/input_edges.{hpp,cpp}` (new) | the inputs `bw run` plays, out of `src/cli/main.cpp`: `InputEdge`, `InputEdges`, `RunTo` |
| `src/cli/main.cpp` | `Run` uses `InputEdges` and `RunTo`; `kUsage` (`--knob REF:N%[:TIME]`, `bw random`); the `random` command |
| `src/ui/bench_setup.cpp` | a timed `--knob`, `--manual giveup` and `--show-fault` off a Random fault's bench refused |
| `src/machine/puzzle.{hpp,cpp}` | `QuotedValue`, `IsMachineName` exported; `FormatFault` |
| `src/machine/vetting_script.{hpp,cpp}` (new) | `VettingScript`, `ParseVettingScript`, `ReadVettingScriptFile` |
| `machines/*/puzzles/random.bwr` (new, 14) | each Machine's inputs, capture times and task (D2) |
| `src/machine/machine.{hpp,cpp}` | `Machine::check`, `Machine::clearFaults`; `MachineDir` of a `.bwr` |
| `src/machine/machine_list.{hpp,cpp}` | `MachineEntry::random`, `EntryRef::random`, `EntryOf` |
| `src/machine/fault_draw.{hpp,cpp}` (new) | `FaultKind`, `kFaultWeights`, `DrawKind`, `DrawFault` |
| `src/session/vetting.{hpp,cpp}` (new) | `Shown`, `Watched`, `RunScript`, `Shows`, `Vetting` |
| `src/session/procedural_puzzle.{hpp,cpp}` (new) | `MakeProceduralPuzzle` |
| `src/session/puzzle_maker.{hpp,cpp}` (new) | `PuzzleMaker`: the making on its own thread |
| `src/machine/CMakeLists.txt`, `src/session/CMakeLists.txt` | the new sources |
| `src/view/launcher_lanes.{hpp,cpp}` | one more post-it on a Machine with a Vetting Script |
| `src/view/binder.{hpp,cpp}` | `Divider::GiveUp`, `TabTops` |
| `src/ui/manual_window.{hpp,cpp}` | the "Give up" tab and page, the tabs laid out by `TabTops` |
| `src/ui/launcher.{hpp,cpp}` | the post-it, the making and its lines, `LauncherChoice`, `select` on a `.bwr` |
| `src/ui/loaded.{hpp,cpp}` | `Load` on a `.bwr` with its Fault; `OpenedMachine::fault` |
| `src/ui/main.cpp` | the choice carried to `Load`; a `.bwr` made from the terminal; the card's Next |
| `src/ui/thank_you_card.{hpp,cpp}` | the Next button's label given whole |
| `src/view/bench_options.{hpp,cpp}` | `--seed`, `--make`, `--show-fault`, `--manual giveup` |
| `tests/CMakeLists.txt`, `tests/bw_random.cmake` (new) | the new test files; `bw_random`; one ctest a Machine (D5) |
| `tests/…` | below, a section at a time |
| `docs/…`, `AGENTS.md`, `README.md`, `tools/readme_shots.py` | D9 |

New struct fields get `= {}` where an aggregate is built without them (`MachineEntry::random`, `OpenedMachine::fault`, `KnobSetting::at`): the build has `-Wmissing-field-initializers` and `-Werror`.

## 1. A knob set at a time, the inputs in a library (block 2)

### `src/machine/knob_setting.hpp`

```cpp
struct KnobSetting {
    std::string reference;  // "J2"
    double turn;            // 0 (left stop) .. 1 (right stop)
    Time at = 0;            // simulated ps; 0: from power-on
};
// "J2:40%", "J2:12.5%" (0 to 100 %), or with a time "J2:87%:400ms"; error text otherwise.
std::expected<KnobSetting, std::string> ParseKnob(std::string_view text);
```

The error becomes `bad knob '…' (expected REF:N% or REF:N%:TIME, 0 to 100)`; `tests/machine/knob_setting_test.cpp` pins the old text and follows. `ParseDuration` refuses 0, so a time is never 0 by accident. `Paddle::setKnob` is read when a pulse starts: a turn needs no wake. `ParseKnob`'s other callers (`src/view/bench_options.cpp`, `--knob`) need no change.

### `src/machine/input_edges.hpp` (bw_machine: the parsers are there; `Simulator` and the lookups `SwitchOf`, `KeyMatrixOf`, `KeyboardOf`, `PaddleOf` come through its public links)

```cpp
// One scripted input at its simulated time: a switch pressed or released, a key typed, a knob turned.
struct InputEdge {
    Time at;
    ComponentId wake;
    std::function<void()> apply;
};

// The edges of presses, typing and timed knobs on `loaded`, in time order. Knobs with no time are
// set at once. Error: "'SW9' is not a switch", "'J9' is not a keyboard", "cannot type the text on
// 'J3', a keyboard", "'J9' is not a paddle controller" (bw run's texts).
std::expected<std::vector<InputEdge>, std::string>
InputEdges(LoadedCircuit& loaded, std::span<const ButtonPress> presses,
           std::span<const KnobSetting> knobs, std::span<const Typing> typing);

// Runs `sim` to `end`, a millisecond at a time and stopping at each edge from `next` on to apply
// it (then wake). `slice` after each stop (devices fed, recordings trimmed): false stops the run.
// False: stopped by `slice`. Edges at `end` or later are left for the next call.
bool RunTo(Simulator& sim, std::span<const InputEdge> edges, std::size_t& next, Time end,
           const std::function<bool()>& slice);
```

This is `Run`'s code moved (`src/cli/main.cpp`: the `InputEdge` struct, the three loops and the stepping loop), the knob loop made an edge when `at > 0`. `Run` keeps its `sim.runFor` path when it has no TV, lamp, probe or edge, and its `slice` always says true.

`src/ui/bench_setup.cpp`, `ApplyBenchOptions`: a `--knob` whose `at` is not 0 is refused, "--knob: a time is for bw run". The bench's own scripted inputs (`src/ui/bench_input.cpp`) are left as they are: they follow frames, not simulated time.

### Tests (`tests/machine/knob_setting_test.cpp`, `tests/machine/input_edges_test.cpp` (new), `[machine]`)

`knob setting: a time`, `knob setting: a bad time`; `input edges: a press is two edges`, `input edges: edges in time order`, `input edges: a timed knob turns the paddle then` (the Knob meter, 42 % then 87 %: its digits read 42, then 86), `input edges: an untimed knob is set at once`, `input edges: text on a key matrix is one edge` (the BW-80's Circuit built, not run), `input edges: an unknown reference`, `run to: stops at each edge and at the end`, `run to: stopped by its slice`. `tests/run_leds.cmake` unchanged: `bw run` prints what it did.

## 2. The Vetting Script (block 1)

### `machines/NNN-name/puzzles/random.bwr`

```
# Random faults on the Dice: how one is vetted, and what its Brief asks.
machine dice
task "Hold ROLL, then let go."
press SW1:300ms:800ms
capture 500ms
capture 700ms
```

Lines, in any order (one `machine` line, as in a `.bwp`):

- `machine NAME`: the netlist's name, as a `.bwp` (`IsMachineName`);
- `task "…"`: at most one, the Brief's "do this" (§7), read by `QuotedValue` as a `.bwp`'s `brief`;
- `press REF:DOWN:UP`, `knob REF:N%[:TIME]`: one a line, the string `ParsePress` and `ParseKnob` take;
- `type REF:START:TEXT`: the rest of the line, so the text keeps its spaces; `\n` is NEW LINE or RETURN (`ParseTyping`);
- `capture TIME`: one or more, each later than the one before.

`#` starts a comment, but not on a `task` or a `type` line (their text may hold one), as a `.bwp` does for `title` and `brief`. `QuotedValue` and `IsMachineName` are private to `puzzle.cpp` today: declared in `puzzle.hpp`, used by both parsers.

The fourteen files (the spec's What the samples show, with the presses as sampled; a `#` first line in each, left out here):

| Machine | Lines after `machine` |
|---|---|
| `counter` | `capture` 250ms, 500ms, 750ms, 1250ms |
| `chaser` | `capture` 600ms, 666ms, 732ms, 798ms, 864ms, 930ms, 996ms, 1062ms |
| `dice` | `task "Hold ROLL, then let go."`; `press SW1:300ms:800ms`; `capture` 500ms, 700ms |
| `stopwatch` | `task "Try START, STOP and RESET."`; `press` SW2:50ms:100ms, SW3:150ms:250ms, SW1:300ms:400ms, SW2:1000ms:1100ms; `capture` 280ms, 600ms, 900ms, 1300ms |
| `knob` | `task "Turn the knob from end to end."`; `knob` J2:42%, J2:87%:400ms, J2:5%:800ms; `capture` 300ms, 700ms, 1100ms |
| `test_pattern` | `capture 400ms` |
| `ball` | `capture` 600ms, 900ms, 1200ms |
| `tennis` | `task "Press START and turn both knobs."`; `knob` J3:40%, J4:60%, J3:85%:1100ms, J4:15%:1100ms; `press SW1:200ms:300ms`; `capture` 1000ms, 1500ms |
| `message` | `capture 300ms` |
| `typewriter` | `task "Press CLEAR and type a few words."`; `press SW1:500ms:700ms`; `type J3:1s:HELLO, WORLD`; `capture 2500ms` |
| `led_computer` | `task "Watch the light, then press RESET."`; `press SW1:1000ms:1100ms`; `capture` 200ms, 400ms, 600ms, 800ms, 1300ms, 1500ms, 1700ms |
| `bw1` | `task "Pick an address with AD, change its byte with DA, go on with +."`; `press` SW17:100ms:150ms, SW1:200ms:250ms, SW4:300ms:350ms, SW1:400ms:450ms, SW1:500ms:550ms, SW18:600ms:650ms, SW11:700ms:750ms, SW10:800ms:850ms, SW19:900ms:950ms; `capture` 80ms, 580ms, 880ms, 1000ms |
| `bw2001` | `capture 1400ms` |
| `bw80` | `task "Type PRINT 42 and NEW LINE."`; `type J3:350ms:PRINT 42\n`; `capture` 300ms, 1200ms |

### `src/machine/vetting_script.hpp`

```cpp
// A Machine's puzzles/random.bwr: how a Procedural Puzzle is vetted on it and what its Brief asks.
struct VettingScript {
    std::string file;
    std::string machine;  // "dice"
    std::string task;     // "" when none
    std::vector<ButtonPress> presses;
    std::vector<KnobSetting> knobs;
    std::vector<Typing> typing;
    std::vector<Time> captures;  // capture times, ascending, at least one
};

// All errors, in line order.
std::expected<VettingScript, std::vector<Diagnostic>> ParseVettingScript(std::string_view text,
                                                                        std::string file);
std::expected<VettingScript, std::vector<Diagnostic>>
ReadVettingScriptFile(const std::filesystem::path& path);
```

Errors by line: an unknown directive, a missing or second `machine`, a second `task`, `ParsePress`'s, `ParseKnob`'s and `ParseTyping`'s texts, "no capture line", "captures must come in time order". Only the file named `random.bwr` is a Machine's script. References are checked on the Machine by `InputEdges` (D4), and for every Machine by a test here.

### `src/machine/machine.cpp`, `src/machine/machine_list.{hpp,cpp}`

- `MachineDir`: a `.bwr` as a `.bwp`, its directory's parent.
- `MachineEntry` gains `std::optional<std::filesystem::path> random = {};  // puzzles/random.bwr, read without error`. `ListMachines` reads it with `ReadVettingScriptFile`. A Machine without the file has no Random fault, and that is no problem (`machine list: a directory` keeps its three). One that fails to parse, or names another Machine, is a problem (on stderr, as a Puzzle's) and the Machine has no Random fault.
- `EntryRef` gains `bool random = false;  // the Machine's Vetting Script`. `EntryOf` matches a Machine's `random`. `NextPuzzle` is unchanged (it finds nothing for a `.bwr`).

### Tests (`tests/machine/vetting_script_test.cpp` (new), `tests/machine/machine_list_test.cpp`, `tests/machine/puzzle_test.cpp`, `[machine]`)

`vetting script: every line`, `vetting script: errors by line`, `vetting script: captures in order`, `vetting script: a type line keeps its spaces and its #`; `vetting script: every Machine's inputs exist` (each `random.bwr`: its Machine's netlist built, `InputEdges` on it); `machine list: the Vetting Script`, `machine list: a random.bwr naming another Machine`, `machine list: the repository` (every Machine has one); `machine dir: of a random.bwr`.

## 3. Drawing a Fault (block 3)

### `src/machine/puzzle.hpp`

```cpp
// The `fault …` line of a Fault, as ParsePuzzle reads it back: "fault stuck U5.QD 0", "fault crack
// R6.2 D5.2 51%" (two decimals at most), "fault cell U2.D3 ram $3F 0".
std::string FormatFault(const FaultDecl& fault);
```

### `src/machine/machine.hpp`

```cpp
// Whether inject would take this Fault, the Machine left as it is; the error is inject's.
std::expected<void, std::string> check(const FaultDecl& fault) const;
// Every Fault and every Repair gone: the fault-free Machine again (the Vetting's next candidate).
void clearFaults();
```

`check` visits the private `resolve` overloads; `clearFaults` empties `faults_`, `loaded_`, `cuts_` and `wires_`, then `rebuildBoard()`.

### `src/machine/fault_draw.hpp`

```cpp
enum class FaultKind : std::uint8_t { Stuck, Crack, Bridge, Joint, Column, Cell, Switch };

// Shares of the kinds, in FaultKind's order: 35, 32, 12, 10, 6, 3, 2 (the spec's start).
inline constexpr std::array<int, 7> kFaultWeights{35, 32, 12, 10, 6, 3, 2};

// A kind by weight among those the Machine has something for (a memory chip for Column and Cell,
// a switch for Switch); nullopt for a Machine with no chip at all.
std::optional<FaultKind> DrawKind(const Machine& machine, Rng& rng);

// A random Fault of `kind` the Machine accepts (Machine::check): refused draws are drawn again,
// kDrawTries (200) at most; nullopt after that, or when the Machine has nothing of that kind.
std::optional<FaultDecl> DrawFault(const Machine& machine, FaultKind kind, Rng& rng);
```

Each draw takes an object evenly among the Machine's, from `Machine::designCircuit()`:

- **Stuck, Joint**: one pin among all pins of all chips (`LoadedPart::info->kind == PartKind::Chip`, `Component::pins()`), by number; Stuck also a value. `check` refuses a Power or `NoConnect` pin, and they are drawn again, so every signal pin is as likely. A signal pin on no Net is accepted: the Vetting finds that it shows nothing.
- **Bridge**: a chip pin `k` among those with a pin `k + 1`, and that pin; the pair across the package is refused ("pins are not neighbours").
- **Crack**: a `Connection` among all of them (a pad on a Net), another pad of the same Net, and a place from 0 to 100 % in hundredths. `placeCrack` refuses the pairs with nothing crackable between them.
- **Column, Cell**: a Component that is a `MemoryCells`, one of its data pins (`dataBit`), one of its `memories()` when it has several, a value; Cell also a word under `wordCount`.
- **Switch**: a Part of `PartKind::Switch` (a BUTTON, a KEY, a SLIDE), closed or open.

`Rng` is `src/sim/rng.hpp`'s (the same on every platform); a number under `n` is `rng.next() % n` (the bias is far under one part in a billion at these sizes). The same Machine, kind and `Rng` state give the same Fault.

Against the samples (`tools/fault_sample.py`): every pin is as likely here, where the tool drew a chip first; the oscillator and the power-on reset are chips here (`PartKind::Chip`), and a SLIDE is a switch; the tool drew none of them. D8 measures again.

### Tests (`tests/machine/puzzle_test.cpp`, `machine_test.cpp`, `tests/machine/fault_draw_test.cpp` (new), `[machine]`)

`puzzle: FormatFault of each kind`, `puzzle: every Puzzle's faults format back` (the 114 `.bwp`: `ParsePuzzle` of `FormatFault` gives the same Fault); `machine: check takes nothing`, `machine: clearFaults`; `fault draw: each kind on a Machine that has it` (each drawn Fault is accepted by `inject`), `fault draw: a kind with nothing to act on` (no memory on the Counter, no switch), `fault draw: the same seed, the same Fault`, `fault draw: the kinds' shares` (10 000 draws of `DrawKind`, within two points), `fault draw: every signal pin can be drawn` (the Counter, 2 000 stuck draws).

## 4. Vetting (block 4)

### `src/session/vetting.hpp` (bw_session: it needs `Machine`, `TvInput`, `LedInput`, `FindTvNets`, `FindLedNets`; `bw`, `bw_ui` and the tests all link it)

```cpp
// What a Machine shows at one capture time: its TV's frame, or its lamps.
struct Shown {
    bool locked = false;       // the TV has a picture
    std::vector<float> cells;  // TvFrame::cells; empty on a Machine vetted on its lamps
    std::vector<float> lamps;  // each watched lamp's brightness; empty on a Machine with a TV
};

// The outputs a Machine is vetted on, decided once on its fault-free build.
struct Watched {
    bool tv = false;                                         // FindTvNets found one: the TV alone
    std::vector<std::pair<std::string, std::size_t>> lamps;  // else each lamp: reference, lamp
};
Watched WatchedOf(const LoadedCircuit& golden);

inline constexpr int kShownCells = 100;      // TV cells that differ: fewer do not show
inline constexpr float kCellMargin = 0.25f;  // a cell differs when off by more
inline constexpr float kLampMargin = 0.25f;  // of the brightest golden lamp: a lamp differs when off by more

// The golden picture lost, kShownCells cells off by more than kCellMargin, or a lamp off by more
// than kLampMargin of `brightest`.
bool Shows(const Shown& golden, const Shown& mine, float brightest);

// `loaded` run from power-on (seed 1) with the script's inputs, what it shows taken at each
// capture time. With `golden`, stops at the first that Shows and returns those so far. Stopped by
// `stop`: nullopt. Error: InputEdges'.
std::expected<std::optional<std::vector<Shown>>, std::string>
RunScript(LoadedCircuit& loaded, const VettingScript& script, const Watched& watched,
          const std::vector<Shown>* golden, float brightest, std::stop_token stop);

// A Machine and what it shows fault-free: whether a Fault shows on it.
class Vetting {
public:
    // `machine` fault-free; runs it once. nullopt: stopped. Error: the script's (an unknown
    // switch), a Machine with no TV and no lamp, or a Golden Machine with no picture at a
    // capture time.
    static std::expected<std::optional<Vetting>, std::string>
    Make(Machine machine, VettingScript script, std::stop_token stop);
    const Machine& machine() const { return machine_; }
    // nullopt: stopped. The Machine is fault-free again afterwards.
    std::optional<bool> shows(const FaultDecl& fault, std::stop_token stop);
};
```

- One run a try, a `Simulator` stepped by `RunTo` (D1) to each capture time in turn; its `slice` feeds the device, trims the recordings and returns `!stop.stop_requested()`: a stop is seen within a millisecond of simulated time.
- `Watched` is the fault-free build's: its TV when it has one (the BW-2001's one LED is then not watched), else its lamps. A faulty build is read by the same list: a watched lamp `FindLedNets` no longer finds reads 0; a TV whose Nets `FindTvNets` no longer finds is a lost picture.
- A TV's `Shown` copies `Tv::completedFrame().cells` and `Tv::isLocked()`. The cells are fractions (a column's coverage), hence `kCellMargin`; the samples compared the BMP's bytes exactly, so D8 measures again.
- `brightest` is the largest lamp value over the golden's (1 when none is lit): the BW-1's multiplexed segments reach 0.17.
- `shows`: `inject` (a Fault `check` took), `build()`, `RunScript` against the golden, `clearFaults()`. A build that fails counts as not showing. So does a `std::exception` out of the build or the run (a `Check` in the builder or the kernel on a Fault nobody tried by hand): caught there, the Machine cleared. That `try` is around one simulation, on the maker's thread or `bw`'s, never around a frame loop (the web build's rule).
- The first capture time that shows ends the try: on the BW-80 a Fault that loses the picture is known at 300 ms, before the typed line.

### Tests (`tests/session/vetting_test.cpp` (new), `[session]`, the long ones `[slow]`)

`vetting: the Counter's golden` (dark, lit, lit, dark), `vetting: the Dark LED's crack shows`, `vetting: a stuck unconnected output does not`, `vetting: the Machine is fault-free again`, `vetting: a lost picture shows` (the test pattern, its clock stuck), `vetting: one cell off does not` (`Shows` on hand-made `Shown`s: 99 cells, 100 cells, a cell within the margin), `vetting: lamps against the brightest` (hand-made: 0.17 against 0, 0.9 against 1), `vetting: a watched lamp the build lost reads dark`, `vetting: stops at the first capture time that shows`, `vetting: stopped` (in `Make` and in `shows`), `vetting: an unknown switch in the script`.

## 5. Making a Puzzle (blocks 5 and 8)

### `src/session/procedural_puzzle.hpp`

```cpp
struct MadePuzzle {
    FaultDecl fault;
    int tries;  // Faults vetted, this one included
};

inline constexpr int kTriesAKind = 5;
inline constexpr int kKindPicks = 60;  // then "no Fault shows on this Machine"

// Whether a Fault shows; nullopt: stopped. Vetting::shows, or a test's own.
using ShowsFn = std::function<std::optional<bool>(const FaultDecl&)>;

// The framework's loop: a kind by weight, a Fault of it, vetted; another of the same kind while
// it does not show, kTriesAKind at most; then a kind again. nullopt: stopped. Error: after
// kKindPicks kinds.
std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzle(const Machine& machine, const ShowsFn& shows, Rng& rng);
std::expected<std::optional<MadePuzzle>, std::string>
MakeProceduralPuzzle(Vetting& vetting, Rng& rng, std::stop_token stop);
```

A kind whose `DrawFault` gives nothing counts as a pick. The second form is the first on `vetting.machine()` and `vetting.shows`.

### `src/cli/main.cpp`: `bw random <machines/NNN-name/puzzles/random.bwr> [--seed 1]`

Loads the Machine (`LoadNetlistMachine` of the netlist in `MachineDir(file)`), makes one with `Rng(seed)` and prints `fault …` (`FormatFault`), `tries N`, `golden_s S` (the Golden Machine's run) and `seconds S` (the whole making); exit 1 with the error. `kUsage` gains its line.

### Tests (`tests/session/procedural_puzzle_test.cpp` (new), `tests/bw_random.cmake` (new), `tests/CMakeLists.txt`)

- With a `ShowsFn` of the test's own, no simulation: `procedural puzzle: five of a kind at most, then another` (one that never shows and records what it is asked: no run of one kind longer than five, the error after `kKindPicks` picks), `procedural puzzle: the first that shows is made` (its `tries`), `procedural puzzle: stopped`, `procedural puzzle: the same seed, the same Faults asked`.
- On the Counter: `procedural puzzle: it shows and is not completed`.
- One ctest a Machine, as the BW-2001's conformance set has one a session: a hidden case `procedural puzzle: a Machine`, a section a Machine's directory, and in `tests/CMakeLists.txt` a `foreach` over `machines/*/puzzles/random.bwr` adding `procedural puzzle: 010-counter` and so on (`bw_tests "procedural puzzle: a Machine" -c 010-counter`, `SKIP_RETURN_CODE 4`). Each: one `Vetting`, then seeds 1 to 3: a Puzzle is made, `completed()` is false, and true after the Repair its kind calls for (`swap`, `mend` at the crack, `removeBridge`, `resolder`). In debug: seed 1 only, and only the Machines whose try is about a tenth of a second in release (010, 020, 030, 040, 045, 050, 080); the others `SKIP`. The BW-2001 and the BW-80: release only, seed 1 only.
- ctest `bw_random`: the Counter, the same line twice for one seed; the line under a `machine counter` line loads as a Puzzle (`bw run`).

Time: in release about 50 s summed over the fourteen ctests, 12 s at most for one (the BW-2001), run in parallel; in debug about a minute each for the seven.

## 6. The bench on a Random fault (§7 to §11, block 7)

### `src/ui/loaded.hpp`

```cpp
struct OpenedMachine {
    …
    std::string fault = {};  // a Random fault's `fault …` line; "" otherwise
};
// A .bwr opens its Machine with `fault` injected (a Random fault's bench).
OpenedMachine Load(const std::filesystem::path& file, const std::optional<std::filesystem::path>& roms,
                   const FaultDecl* fault = nullptr);
```

On a `.bwr`: `ReadVettingScriptFile`, `LoadNetlistMachine` of the Machine's netlist, `inject(*fault)` before the Session is made (`Session::puzzle()` is read at its construction: the tick and the card need it); the title is "Random fault", the brief the fixed sentence and the script's `task` after a blank (§7); `fault` is `FormatFault`'s line. `Load`'s other callers (the Launcher's live picture, `main()`) pass no Fault.

### `src/ui/main.cpp`

- A `.bwr` that reaches `Load` with no Fault made (`bw_ui FILE.bwr`, `--visit FILE.bwr`) has one made first, on the spot, the window not yet drawn: `MakeProceduralPuzzleFor(file, options.seed.value_or(1), roms)` (`src/session/procedural_puzzle`: the file read, its Machine loaded, `Vetting::Make`, `MakeProceduralPuzzle`). An error is the load's error.
- The title of an entry is overwritten today when `EntryOf` finds no Puzzle (the working Machine's name): not for `EntryRef::random`.
- `RunBench` reads `MachineDir(*options.file)` for `roms/contents.txt`: right for a `.bwr` with D2's `MachineDir`.
- Reset is `Machine::undoRepairs`, which keeps an injected Fault: nothing to do.
- No Next on the card yet: `NextPuzzle` finds nothing for a `.bwr`. "Another random fault" comes with the Launcher's making (D7).

### `src/view/binder.{hpp,cpp}`, `src/ui/manual_window.{hpp,cpp}`

- `enum class Divider { User, Service, Datasheets, Repairs, Contents, GiveUp };` `kTabs` gains `{Divider::GiveUp, "Give up", …}`; `PageId` and the page's `switch`, the two over `Divider`, gain their case.
- `std::vector<float> TabTops(std::size_t count, float top, float bottom, float tabHeight, float step);` in `binder.hpp`: the top of each of `count` visible tabs, `step` apart from `top`, closer (overlapping at the worst) when the last would end past `bottom`. `ManualWindows::tabs` places the visible tabs by it, where it used `kTabs`' index: no gap for a hidden Contents, and six tabs fit the smallest binder (292 px, reached by a drag: six need 324).
- `ManualWindows::draw(…, const std::string* fault)`: the `fault` line on a Random fault's bench, else `nullptr`; the tab shows only then. Its one caller is `RunBench`.
- The page: "Give up?", one sentence, the button "Show the fault"; pressed (`faultShown_`, kept for the bench), the line as a selectable row: a click calls `ImGui::SetClipboardText` and says "copied" beside it.
- `Binder` needs no state: the tab is there or not by `fault`.

### `src/view/bench_options.{hpp,cpp}`, `src/ui/bench_setup.cpp`

- `Options::seed` (`std::optional<std::uint64_t>`, `--seed N`): the seed of a `.bwr`'s Fault. `ParseOptions` refuses it unless the file is a `.bwr` or `--make` is given (D7).
- `BenchOptions::showFault` (`--show-fault`: the page as after its button); `giveup` in `kDividers` for `--manual`. `ParseOptions` refuses both when the file is not a `.bwr` (it knows the file; `ApplyBenchOptions` does not).
- `tests/view/bench_options_test.cpp` pins the usage's start and end: it follows.

### Tests (`tests/view/binder_test.cpp`, `tests/view/bench_options_test.cpp`; screenshots in Checks)

`binder: tabs a step apart`, `binder: six tabs in the smallest binder` (the last ends inside it), `binder: one tab`; `bench options: --seed`, `bench options: --seed without a .bwr`, `bench options: --show-fault`, `bench options: --manual giveup`.

## 7. The Launcher's post-it and the making (§1 to §6, §8, §9, block 6)

### `src/view/launcher_lanes.{hpp,cpp}`

`setPuzzles(std::vector<std::size_t> counts, std::vector<bool> random = {})`: a Machine's post-its are the card, its Puzzles, and one more when `random` says so (none when it is empty: the tests' three calls stand). `bool random() const`: the selected post-it is that one; `puzzle()` is `nullopt` on it.

### `src/session/puzzle_maker.{hpp,cpp}` (bw_session, so it is tested; threads as the Runner's)

```cpp
// Makes a Procedural Puzzle off its caller's thread.
class PuzzleMaker {
public:
    void start(std::filesystem::path file, std::uint64_t seed);  // a random.bwr; stops one under way
    void stop();                                                  // asks, does not wait
    bool making() const;
    // The Fault once made, or the error; nullopt while making, and after stop().
    std::optional<std::expected<FaultDecl, std::string>> take();
};
```

Its `std::jthread` runs `MakeProceduralPuzzleFor` (the file read, the Machine loaded, the `Vetting` and the Puzzle made), its `stop_token` passed down; an exception out of the load is caught there and is the error. A stopped making is kept until its thread has ended, never waited for; the destructor stops and joins them all: at most a millisecond of simulated time, or a Machine's load.

### `src/ui/launcher.{hpp,cpp}`

- `struct LauncherChoice { std::filesystem::path file; std::optional<FaultDecl> fault; };` `Launcher::draw` returns `std::optional<LauncherChoice>`; `LauncherResult` in `main.cpp` carries it and `main()` gives the Fault to `Load`.
- `setList` feeds `lanes_` each Machine's `random.has_value()`. `entryFile()` is the Machine's `random` on its last post-it. `select(file)` selects that post-it for `EntryRef::random` (the bench left, `--select`).
- The live picture's file (`Live::file`) is the Machine's netlist there, so it is the working Machine, the same run as the card's (§2).
- The post-it: `DrawPostIt` gains a `sign` drawn in place of the brief, "?" at about four times the title's size, centred; its title "Random fault". It sits after the last Puzzle, `kRandomGap` further than the row's own gap. The row's gap and scroll count Puzzles today (`titles.size() - 1`, `lanes_.puzzle()`): they count the row's post-its instead.
- The "?" over the live picture (§2): drawn on the picture's rectangle at a low alpha while the post-it is selected, behind a constant (`kPictureQuestion`) until the user has seen both screenshots.
- Opening it (§3): `maker_.start(file, seed)`, the seed `std::random_device`'s, or `--seed`'s with `--make`. While making: keys and clicks do nothing but Esc; the wood is dimmed and a dark plate in its middle shows the line, changed every `kLineTime` (1 s), in the list's order from a random start, with "Making a random fault. Esc stops." under it. Esc calls `maker_.stop()` (§6).
- The lines end in three dots, typed as dots: the UI font has no ellipsis (`Displayable` maps dashes and arrows only).
- The choice is returned when the Fault is there and `kLineSeconds` have passed since the start (a line is always read). An error (no Fault shows, a bad file) shows in the band until a key.
- `Launcher::make()` starts it for the selected post-it: `main()` calls it for the card's "Another random fault", and for `--make`.
- `Launcher::stop()` stops the maker with the rest.

### `src/ui/main.cpp`, `src/ui/thank_you_card.{hpp,cpp}`

- `ThankYouCard::draw(title, const std::string* nextLabel, canLeave)` takes the Next button's whole label: "Next: " and the title for an Authored Puzzle, "Another random fault" on a `.bwr` when the Launcher is there (`list`), as Back to the Machines needs it. `following` is then the same file.
- `BenchEnd::Next` on a `.bwr`: back to the Launcher, that post-it selected (`left`), `launcher.make()`.
- With `--screenshot`, the Launcher never opens a bench: the shot is taken at `--after`, a making still shown or its Fault held. Today `RunLauncher` returns a choice before it looks at `--after`, and the bench then has no `--screenshot`.

### `src/view/bench_options.{hpp,cpp}`

`Options::make` (`--make`, with `--select` of a `.bwr` only: the making started at once; for the screenshot of a line).

### Tests (`tests/view/launcher_lanes_test.cpp`, `tests/session/puzzle_maker_test.cpp` (new); screenshots in Checks)

`launcher lanes: a Random fault is the last post-it`, `launcher lanes: none without the file`, `launcher lanes: the selection kept across setPuzzles`; `puzzle maker: makes one` (the Counter), `puzzle maker: the same seed, the same Fault`, `puzzle maker: stopped` (nothing to take), `puzzle maker: started again while making`, `puzzle maker: a bad file is its error`; `bench options: --make`, `bench options: --make without --select`.

## 8. The fourteen files, measured again

`bw random` on each Machine for seeds 1 to 20: the tries, the kinds made, the golden's seconds and the whole making's go into the plan's notes, beside the samples' numbers (the draws and the cell margin differ from the tool's, D3 and D4). A Machine whose golden has no picture at a capture time, or that makes none in `kKindPicks`, has its file corrected there; the knobs' later positions on the Knob meter and the Tennis are new.

The wait, estimated from the samples (the Golden Machine's run, then about one try in two or three shows): under a second on ten Machines, one to two on the Bouncing ball and the Tennis, about 4 s on the Typewriter, about 10 s on the BW-2001 and 9 s on the BW-80, of which the golden is 4 and 5. The user, 2026-11-24: not a problem at this stage, nothing is built for it (later, if it feels long: something to please the player, and `PuzzleMaker` keeping its `Vetting` for the next making of the same file).

## 9. Docs

- `GLOSSARY.md`: Procedural Puzzle (the "Random fault" post-it, made when opened); Vetting (the run, the capture times, what shows; "or is trivial" gone); a new Vetting Script entry (the `random.bwr`); the Launcher's entry (the post-it, the lines); Equipment's binder (the "Give up" divider); the Thank-you Card's (Another random fault); Knob (set at a time in a script); Capture gains "not a Vetting's capture time".
- `recipes/ADD_MACHINE.md`: the `puzzles/random.bwr` step (inputs, capture times on a stable picture, task; `bw random` to try it). `recipes/ADD_PUZZLE.md`, A new kind of Fault: `FormatFault`, `FaultKind`, its draw.
- `AGENTS.md`: Shared helpers gain `InputEdges` and `RunTo`, `FormatFault`, `QuotedValue`, `Rng`; Build and test's sentence on `bw run` gains `bw random`.
- `ARCHITECTURE.md`: the row done, one line; Faults: a pointer to the Vetting.
- `README_SPEC.md`, `tools/readme_shots.py`, `README.md` and its pictures: the Launcher's shot shows the post-it; one sentence in the Puzzles' section; regenerated.
- The spec: its Tests and Open brought up to what was built and measured.

## Checks

- Release: the full suite after each phase (each touches shared code).
- Debug (ASan, UBSan), never the BW-2001's or BW-80's cases: `ctest --preset debug -j -R "knob setting|input edges|run to|vetting script|machine list|machine dir|fault draw|puzzle: |machine: (check|clearFaults)|vetting:|procedural puzzle: (five|the first|stopped|the same|it shows|010|020|030|040|045|050|080)|puzzle maker|launcher lanes|bench options|binder|run_leds|bw_random"`, each phase the part it has; debug `bw_ui machines/010-counter/puzzles/random.bwr --seed 1 --screenshot` and `bw_ui --select machines/010-counter/puzzles/random.bwr --make --screenshot … --after 0.5s`.
- Screenshots (release, viewed):
  - `bw_ui machines/010-counter/puzzles/random.bwr --seed 1`: the bench, its Brief;
  - the same with `--manual giveup`, then with `--show-fault`;
  - `bw_ui machines/080-message/puzzles/random.bwr --seed 1 --contents U14 --manual giveup`, the binder at `--binder-height`'s smallest: six tabs;
  - the Counter's with the `--repair` its Fault calls for at `--after 4s`: the card, "Another random fault";
  - `bw_ui machines/130-bw2001/puzzles/random.bwr --seed 1`;
  - `bw_ui --select machines/010-counter/puzzles/random.bwr`: the post-it, the working Counter live, with `kPictureQuestion` on and off;
  - the same with `--make --after 0.5s`: a line.
- Speed: `bw random`'s seconds on every Machine (D8); the Launcher's UI ms with the post-it.
- Web build after phases 5 and 6 (it links, its data holds the scripts).
- Only the user, reported at the end, no fence waits for them: the lines, the Briefs' words, the "?" over the picture; a few Random faults played, with the card's "Another random fault" and Esc during a making; on the web, the page loaded (the post-it in the Launcher) and a Random fault made on the Counter, the BW-2001 and the BW-80, timed.
