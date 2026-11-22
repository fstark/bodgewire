# P7a — Launcher — Design

How [LAUNCHER.md](LAUNCHER.md) is built. Plan: [LAUNCHER_PLAN.md](LAUNCHER_PLAN.md). Numbers in parentheses (§3) are the spec's "What the player sees" items.

The kernel, Parts, the builder, devices, Machines, Puzzles and manuals are untouched. New: one scan in `src/machine`, the launcher in `src/ui`; changed: `bw_ui`'s `main`, the shelf, the power-cycle question.

## Files

| File | Change |
|---|---|
| `src/machine/machine_list.hpp`, `.cpp` (new) | `PuzzleEntry`, `MachineEntry`, `MachineList`, `ListMachines`, `FindMachinesDir`, `MachineIndexOf` |
| `src/machine/CMakeLists.txt` | `machine_list.cpp`; `bw_manual` linked PRIVATE (the manual's title and paragraph: `ParseMarkdown`) |
| `tests/machine/machine_list_test.cpp` (new), `tests/CMakeLists.txt` | `machine list: …` |
| `src/ui/launcher.hpp`, `.cpp` (new) | `Launcher`: the two panes |
| `src/ui/question_popup.hpp`, `.cpp` (`git mv` of `power_cycle_popup.*`) | `QuestionPopup`: the text and the "Don't ask again" box given at construction |
| `src/ui/shelf.hpp`, `.cpp`, `shelf_drawings.hpp`, `.cpp` | `Tray::Leave`, `Item::Leave` and its drawing; `canLeave` in, `leaveClicked` out |
| `src/ui/bench.hpp`, `.cpp` | `ResetBench()` |
| `src/ui/gpu_render.hpp`, `.cpp` | `present(TvView*)`: no TV on the launcher |
| `src/ui/main.cpp` | options without a file, `--machines`, `--visit`, `--leave`; `ImGuiScreen`; `RunWindow` ends with quit or back; `RunLauncher`; `main`'s loop |
| `src/ui/CMakeLists.txt` | `launcher.cpp`, `question_popup.cpp` |
| `src/ui/brief_window.hpp`, `contents_page.hpp` | "for the run" becomes "for the bench" in comments |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_MACHINE.md`, `ADD_PUZZLE.md`, `ADD_INSTRUMENT.md`, `docs/ARCHITECTURE.md` | Launcher, the Shelf's tray, "for the bench"; what the launcher reads; the row |

## 1. The list (spec decisions, §2–3)

### `src/machine/machine_list.hpp`

```cpp
namespace bw {

// What the launcher lists (P7a).
struct PuzzleEntry {
    std::filesystem::path file;  // machines/NNN-name/puzzles/NN-name.bwp
    std::string title, brief;
};
struct MachineEntry {
    std::filesystem::path dir;      // machines/NNN-name
    std::filesystem::path netlist;  // its one .bwn
    std::string name;               // the User Manual's title, else the directory's name
    std::string line;               // the User Manual's first paragraph, plain; "" without one
    std::vector<PuzzleEntry> puzzles;  // in file name order (NN-, easiest first)
};
struct MachineList {
    std::vector<MachineEntry> machines;  // in directory name order (NNN-)
    std::vector<Diagnostic> problems;    // a Puzzle that does not parse, two netlists: left out
};

// Each subdirectory of `machinesDir` with exactly one .bwn is a Machine; others (a .DS_Store, a
// directory without a netlist) are skipped silently, two netlists are a problem. Its Puzzles are
// puzzles/*.bwp read by ReadPuzzleFile (no puzzles/: none); one that fails, or names another
// Machine than its netlist's stem, is a problem, left out. Never throws: a missing or unreadable
// directory lists nothing (the error_code overloads of std::filesystem throughout).
MachineList ListMachines(const std::filesystem::path& machinesDir);
// `from`, then each parent: the first <dir>/machines holding a Machine (a subdirectory with a
// .bwn). nullopt at the root. Never throws.
std::optional<std::filesystem::path> FindMachinesDir(const std::filesystem::path& from);
// The Machine whose directory is MachineDir(file) (a .bwn or a .bwp), compared as
// weakly_canonical paths (no throw on a missing file).
std::optional<std::size_t> MachineIndexOf(const MachineList& list, const std::filesystem::path& file);
// The entry (the netlist or a Puzzle's file) that is `file`, compared as weakly_canonical paths:
// --visit's check and its title.
struct EntryRef {
    std::size_t machine;
    std::optional<std::size_t> puzzle;  // nullopt: the working Machine
};
std::optional<EntryRef> EntryOf(const MachineList& list, const std::filesystem::path& file);

}  // namespace bw
```

The name and line: `ParseMarkdown` of `dir/manuals/user.md` (a missing file: the directory's name, `""`), `Title(doc)` for the name (empty: the directory's name), the first `BlockKind::Paragraph` block's `PlainText(spans)` for the line (`PlainText` drops the code and bold marks). Every User Manual today is `# Name`, `## What it does`, then the paragraph (ADD_MACHINE §2).

`Diagnostic` is `netlist/diagnostic.hpp`'s (`bw_machine` has it through `bw_board`); a Puzzle's are `ReadPuzzleFile`'s, unchanged.

### Tests (`tests/machine/machine_list_test.cpp`, cases `machine list: …`, `[machine]`)

- `a directory`: a temporary tree (`temp_directory_path() / "bw_machine_list_dir"`, removed at the end, as `machine: HasBoard` does; each case its own name, since `ctest -j` runs cases in parallel processes) with `020-b/b.bwn` (no `puzzles/`), `010-a/a.bwn` plus `manuals/user.md` (`# Alpha`, `## What it does`, a paragraph with a `` `D1` `` code span), `010-a/puzzles/02-y.bwp` and `01-x.bwp` (`machine a`, `title`, `brief`), `010-a/puzzles/03-bad.bwp` (an unknown keyword), `010-a/puzzles/04-other.bwp` (`machine b`), `030-c/` without a netlist, `040-d/` with two. Expect: `a` then `b`; `a` named "Alpha", its line with `D1` and no backtick; `b` named "020-b", line `""`, no Puzzles; `a`'s Puzzles `x`, `y`; three problems (the bad Puzzle, the other Machine's, `040-d`). `ListMachines` of a missing directory: empty, no throw.
- `the repository`: `ListMachines(test::RootDir() / "machines")`: 14 Machines, no problems; the first named "Counter" with the Puzzles "Dark LED", "Frozen LED", "LED too fast" (the `.bwp` titles); the BW-2001's name "BW-2001", 16 Puzzles; every Machine a line without a backtick and with no `**`; every Puzzle a title and a brief.
- `find machines`: `FindMachinesDir(test::RootDir() / "machines/010-counter/puzzles")` is `test::RootDir() / "machines"` (canonical); from a temporary directory of its own (`bw_machine_list_find`) nullopt, provided no parent of it has a `machines/` (checked first; the case is skipped with a warning otherwise).
- `machine index`: a `.bwp` and a `.bwn` of the BW-2001 give its index, as relative and absolute paths; `EntryOf` of Odd E gives the BW-2001 and its Puzzle 0, of the netlist no Puzzle; a file elsewhere (`tests/data/bw1_bench.bwn`) and a missing file nullopt.

## 2. The loop (spec decisions, §4, §10)

### `main`

```text
options ← ParseOptions
first   ← options.file ? Load(file, roms) + PrepareBench(options) : none   // errors exit before the window, as today
machinesDir ← options.machines
            ?: (file ? parent of MachineDir(file) if FindMachinesDir(MachineDir(file)) is it : none)
            ?: FindMachinesDir(current_path()) ?: FindMachinesDir(SDL_GetBasePath())
--machines DIR listing no Machine: "bw_ui: no Machines in DIR", usage exit
no file and no machinesDir: "bw_ui: no machines/ directory found", usage exit
--visit: each file EntryOf the list, else "bw_ui: 'F' is not a Machine or Puzzle of DIR", usage exit
--leave without machinesDir: usage exit ("nowhere to go back to")
SDL_Init, window, GpuRender                                        // once
loop:
    if first:   result ← bench on *first (options)        ; first.reset()
    else:       choice ← next --visit file, or RunLauncher(...)  (quit → break)
                loaded.emplace(Load(choice, nullopt))     (no session → launcher's error line,
                                                           and with --visit exit code kExitErrors; continue)
                result ← bench on *loaded (BenchOptions(choice, visit))
                loaded.reset()                            // the Session ends before the next Load
    result Quit → break; Back → the launcher selects MachineIndexOf(file) (nullopt: kept)
exit code: the last non-zero one
```

`Loaded` holds a `Session`, which is movable but not assignable (`session.hpp`: a move constructor and a destructor); the loop keeps `std::optional<Loaded>` for both the first bench and the launcher's, `reset()` after each bench.

`PrepareBench` is today's block in `main` between `Load` and `SDL_Init` (probe, contents, clips, groups, hooks, scope, meter, repairs, checks of `--ask`, `--mark`, `--tv`, `--pop`, knobs, typing), moved unchanged into a function returning an exit code or nothing.

The second "where back goes" rule takes the `machines/` above the file's Machine only when it is one (`FindMachinesDir(MachineDir(file))` gives `MachineDir(file).parent_path()`); a netlist in `tests/data/` falls back to the search. The whole search runs before `SDL_Init` (`SDL_GetBasePath` needs none). From the build tree the executable's search always finds the repository's `machines/`, so the grayed arrow (spec §11) is reached only with a `bw_ui` copied elsewhere: checked by reading, not by a screenshot.

The bench's title: `loaded.title`, today's (a Puzzle's title, else `machine->name()`, the netlist's stem: "counter"); a bench opened from the launcher on a working Machine takes the list's name instead ("Counter").

### `ImGuiScreen`: ImGui made per screen

Today's ImGui set-up in `main` (`CreateContext`, `IniFilename`, viewports, `ImGui_ImplSDL3_InitForSDLGPU`, `KeepFocusFromTooltips`, `ImGui_ImplSDLGPU3_Init`, the three fonts) moves into a struct in `main.cpp` whose constructor does it and whose destructor does today's tear-down (`SDL_WaitForGPUIdle`, `ImGui_ImplSDL3_Shutdown`, `ImGui_ImplSDLGPU3_Shutdown`, `DestroyContext`). One lives for the launcher, one for each bench (`std::optional<ImGuiScreen>` emplaced in the loop). The backends' shutdown destroys the popped-out windows (`ImGui::DestroyPlatformWindows`). `KeepFocusFromTooltips` already re-reads `Platform_ShowWindow` at each call.

Why not one context kept: ImGui remembers every window by name for the run (place, size, collapsed), and `PlaceNext`'s `FirstUseEver` would put a second bench's TV where the first's was left (spec: a fresh bench). Clearing that through `imgui_internal.h` would reach into ImGui's internals; a new context is a few milliseconds (the font atlas).

### `ResetBench()` (`bench.hpp`)

`bench.cpp`'s namespace-scope state (`popped_`, `popAtStart_`, `at_`, `size_`, `benchAt_`) back to `{}`. Called by `RunWindow` first. The other mutable globals of `src/ui` need no reset: `KeepFocusFromTooltips`' function pointer is re-set per screen; `bench.cpp`'s `aspect_` is set before every read; `contents_page.cpp`'s `static const bool pound` (whether the UI font has `£`) holds for every context, the fonts being the same.

What was "for the run" is now "for the bench": the power-cycle question's "Don't ask again", the brief's place, each chip's Contents settings, a popped-out window's place. The comments and GLOSSARY lines saying "for the run" change (§6).

### `RunWindow`

Returns `struct BenchResult { BenchEnd end; int exitCode; }`, `enum class BenchEnd { Quit, Back }`. It gains a `bool canLeave` (a `machines/` known) passed to the shelf. How it ends:

- the window closed or SDL's quit: Quit;
- `--screenshot` (a command-line bench): shot `--after`, Quit, as today;
- a visit (`Options::visitFor`, a duration, set by the loop for a `--visit` file): after it, the speed line printed as a screenshot prints it, no shot, Back;
- the Leave tray (§4): Back.

On leaving, before returning: `SDL_StopTextInput` on the main window (the keyboard capture may have started it); the `TvView` and every window object end with the scope. The `Session` is the loop's (`loaded.reset()` after `RunWindow` returns), so its runner thread is joined before the next `Load`.

A bench opened from the launcher gets `BenchOptions(file, visit)`: an `Options` with only `file`, `visitFor` (the `--after` of a `--visit` run, else none) and `machines`; every other field at its default, as a bare `bw_ui FILE`.

### `GpuRender::present(TvView*)`

`tv->upload(commands)` only when not null; the launcher passes `nullptr`. `RunWindow` passes `&view`.

### Window title

`SDL_SetWindowTitle`: "Bodgewire" on the launcher, "Bodgewire — " + `loaded.title` on a bench (today's string).

## 3. The launcher (§1–6)

### `src/ui/launcher.hpp`

```cpp
// The launcher (P7a): the Machines on the left, the selected one's entries on the right. Plain
// ImGui; its look is P7b's.
class Launcher {
public:
    void setList(MachineList list);           // read anew each time; the selection clamped
    void select(std::size_t machine);         // the Machine just left (clamped)
    void setError(std::string text);          // "Could not load …: see the terminal", red
    // Each frame, inside the full-window ImGui window: the file clicked (a .bwn or a .bwp).
    std::optional<std::filesystem::path> draw();
private:
    MachineList list_;
    std::size_t selected_ = 0;
    std::string error_;
};
```

The left pane: a child window 220 px wide, a `Selectable` per Machine's `name`, the selected one highlighted. The right pane: the rest; the name larger (`PushFont(nullptr, style.FontSizeBase * 1.5f)`, as `manual_view.cpp` sizes its titles), the `line` wrapped (`TextWrapped`), a separator, then the entries. Each entry is a `Selectable` as tall as its text (`ImGuiSelectableFlags_AllowOverlap`, its height from `CalcTextSize` with the pane's wrap width), the title over it and the brief under the title in a dimmer colour (`ImGuiCol_TextDisabled`); the working Machine's entry is "The working Machine (no fault)" with no brief. Every text shown (names, lines, titles, briefs) goes through `Displayable` (`manual_view.hpp`), as the manuals' do: the LED Chaser's and the Dice's lines hold an en dash the UI font lacks. A click returns the entry's file. The error line, if any, sits at the top of the right pane, cleared when a Machine is selected. The problems of `ListMachines` go to stderr each time it is read (as a manual's).

### `RunLauncher` (`main.cpp`)

The launcher's frame loop, smaller than `RunWindow`'s: `SDL_PollEvent` into ImGui (quit and the main window's close quit), `NewFrame`, a full-window window ("Launcher", the bench's flags), `launcher.draw()`, `Render`, `gpu.present(nullptr)`. Returns the file clicked, or nothing to quit. `--screenshot` without a file: the screenshot is taken `--after` (default 1 s) after the launcher shows, then it quits as a bench does.

### `--visit FILE,...`

No file, with `--screenshot` (a usage error otherwise). Each file must be one of the list's entries (`EntryOf`; a usage error otherwise, checked before the window). The loop opens each in turn as if clicked, without showing the launcher between; its bench runs `--after` (`Options::visitFor`), then ends as Back (no question). After the last, the launcher shows, selects the last file's Machine and is shot `--after` later. So `--visit machines/130-bw2001/puzzles/01-odd-e.bwp --screenshot x.png` shows the BW-2001's entries. The visited benches print their speed line as a screenshot would, but are not shot. A visited file that fails to load makes the run exit `kExitErrors` (after the launcher's screenshot).

Its limits as a check: a visited bench takes no bench flags, so nothing is popped out and no popped-out window is closed by it (left to the user's check); and ASan on Apple Silicon detects no leaks, so a clean run says nothing about a leaked context or GPU object.

## 4. The way back (§7–11)

### The shelf

`shelf_drawings.hpp`: `Item::Leave`, first in the enum (the enum is the drawing's switch and the shelf's `PushID`; its order nothing else). `DrawItem` for it: an arrow pointing left (a filled triangle and a shaft, a light wood colour, alpha `look.alpha`), centred in its cell. The comments of `shelf.hpp` and `shelf_drawings.hpp` listing the trays left to right gain the Leave tray.

`shelf.cpp`: `Tray::Leave` before `Tray::Controls`; `kItems` gains `{Item::Leave, "Back to the Machines", Tray::Leave, 1}` first (15 items). The layout code needs no change: a new tray at the left is one more group. `Item::Leave` gets its own case in both switches of `Shelf::draw`: the look (`inert = !canLeave`, `why` "No Machines directory") and the click (`leaveClicked = true`), never reaching the `default:` that dereferences the item's `Owner`.

```cpp
// shelf.hpp, Shelf:
bool canLeave = false;      // a machines/ known: the Leave tray's arrow is live (P7a)
bool leaveClicked = false;  // set by draw() on a click, reset by its caller
```

### The question

`PowerCyclePopup` becomes `QuestionPopup` (`git mv` to `question_popup.*`): its constructor takes the ImGui id, the text and whether the "Don't ask again" box is offered; `open(at, pivot)` gains ImGui's pivot (default `{0, 0}`: the top-left at `at`, as today); the rest unchanged. `main.cpp` has two: the power cycle's (as today) and the leave one:

```cpp
QuestionPopup leaveQuestion("##leave", "Back to the Machines? The repairs and pencil marks on "
                            "this Board are lost.", false);
```

In `RunWindow`, after `shelf.draw`: on `leaveClicked`, work is `!session.log().empty() || !marks.marks().empty()`; no work: return Back at the end of this frame (after `Render` and `present`, so ImGui's frame is closed); work: `leaveQuestion.open(shelf.leaveAt, {0, 1})`, its bottom-left just above the Leave tray (`Shelf::leaveAt`, set by `draw`), so it opens upwards over the bench and leaves the shelf seen (the arrow is at most 56 px above the window's bottom: opened downwards it would leave the main window, and ImGui would give it a desktop window of its own, which macOS shows under the main one). Its `Ok`: Back at the end of the frame; `Cancel`: nothing. Drawn last with the power-cycle question.

`--leave` (with a file): the question open at start, at the same place, so the screenshot shows where a click puts it (no check of work: it is for the screenshot). With `--ask` a usage error (one popup at a time); without a `machines/` known a usage error.

## 5. Options (spec decisions)

`Options::file` becomes `std::optional<std::filesystem::path>`; `ParseOptions` takes `argv[1]` as the file when it does not start with `--` (no other argument is ever the file), and no longer refuses `argc < 2`. New: `machines` (`--machines DIR`), `visit` (`--visit FILE,...`, `SplitCommas`; and `Options::visitFor`, the duration a visited bench runs, set only by the loop), `leave` (`--leave`, no value: tested before the `i + 1 >= argc` check, beside `--debug`, whose comment "the one flag without a value" changes). `ParseOptions` keeps a `benchFlag` bool, set by every flag but `--machines`, `--screenshot`, `--after`, `--visit` (comparing `Options` with its defaults cannot tell: `--run 1x` is the default). Usage errors: `benchFlag` without a file; `--visit` with a file; `--visit` without `--screenshot`; `--leave` with `--ask`. The usage line starts `usage: bw_ui [file.bwn|file.bwp] [--machines DIR] [--visit FILE,...]`, `--leave` beside `--ask`.

`RunWindow`'s `options.file` uses (the Contents defaults, `MachineDir(options.file)`) read `*options.file`: a bench always has one.

## 6. Docs

- GLOSSARY: **Launcher** (the screen `bw_ui` opens with no file: the Machines and their entries, P7a); the Shelf's entry: the Leave tray at the left end; "for the run" becomes "for the bench" in the Brief, Contents and Equipment entries.
- Comments saying "for the run" of what is now per bench: `bench.hpp` (`PlaceNext`, `PoppedOut`), `brief_window.hpp`, `contents_page.hpp`, `question_popup.hpp`; `docs/recipes/ADD_INSTRUMENT.md` likewise.
- ADD_MACHINE: the launcher lists a Machine by its directory (`NNN-` the order), names it by the User Manual's `#` title and shows its first paragraph: write the paragraph as the Machine's welcome (it already is: ADD_MACHINE §2).
- ADD_PUZZLE: the launcher lists Puzzles by file name (`NN-`, easiest first) with their `title` and `brief`; a Puzzle whose `machine` is not its directory's netlist is left out and reported.
- ARCHITECTURE: P7a done with the date and links; its clause "the SDL window, GPU and ImGui kept" becomes "the SDL window and GPU kept, ImGui made anew per screen".

## Checks

- The `machine list: …` cases (release and debug: C++ in `src/machine`).
- Screenshots (release): the launcher at start; `--visit` of the BW-2001's Odd E (16 entries after the working Machine); the bench's new tray on a command-line bench (`bw_ui machines/010-counter/puzzles/01-dark-led.bwp --screenshot`: a launcher-opened bench is the same code, and `--visit` shoots only the launcher); `--leave --repair swap:U5` on the Counter's Dark LED (the question above the arrow).
- Debug (ASan, UBSan): `build/debug/src/ui/bw_ui --visit machines/010-counter/puzzles/01-dark-led.bwp,machines/050-test-pattern/test_pattern.bwn,machines/090-typewriter/typewriter.bwn --screenshot /tmp/x.png --after 1s` exits 0 with no sanitizer report (its limits: §3).
- The full release suite (the shared code: `bw_machine` gains a file, the bench's globals).
