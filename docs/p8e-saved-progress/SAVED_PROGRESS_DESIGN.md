# P8e — Saved progress — Design

How [SAVED_PROGRESS.md](SAVED_PROGRESS.md) is built. Plan: [SAVED_PROGRESS_PLAN.md](SAVED_PROGRESS_PLAN.md). Numbers in parentheses (§3) are the spec's items.

Untouched: the kernel, Parts, Machines and Puzzles, Repair validation (`Machine::completed()`), `RepairAck`. New: `Progress` in the machine layer (no SDL, unit tested), where it is kept in the UI layer (SDL's pref path, `localStorage` through `EM_JS`), `--progress` and `--forget`. Changed: `RunBench` saves at the tick, the Launcher draws the ticks and asks the forget question.

## Files

| File | Change |
|---|---|
| `src/machine/progress.hpp`, `.cpp` (new) | `Progress`, `ProgressKey` (two forms), `ReadProgressFile`, `WriteProgressFile` |
| `src/machine/CMakeLists.txt` | `progress.cpp` |
| `tests/temp_files.hpp` (new) | `WriteFile`, moved out of `machine_list_test.cpp`'s anonymous namespace |
| `tests/machine/progress_test.cpp` (new), `tests/machine/machine_list_test.cpp`, `tests/CMakeLists.txt` | the unit cases; `machine_list_test` uses the shared `WriteFile` |
| `src/view/bench_options.hpp`, `.cpp` | `Options::progress`, `Options::forget`; the `--progress FILE` and `--forget` flags (launcher: true) |
| `tests/view/bench_options_test.cpp` | `--progress`, `--forget` |
| `src/ui/progress_store.hpp`, `.cpp` (new), `src/ui/CMakeLists.txt` | `ProgressStore`: file, `localStorage` or none |
| `src/ui/brief_window.hpp`, `.cpp` | `DrawTick` out of `DrawBrief`'s tick, shared |
| `src/ui/launcher.hpp`, `.cpp` | `setProgress`, the post-its' ticks, the hint, Delete/Backspace, the question |
| `src/ui/main.cpp` | the store made in `main`; saved at the bench's tick and at a forget (`<functional>`) |
| `docs/GLOSSARY.md`, `docs/README_SPEC.md` → `README.md`, `AGENTS.md`, `docs/ARCHITECTURE.md` | docs |

## 1. Progress (§1, §2, §5)

### `src/machine/progress.hpp`

```cpp
namespace bw {

// The Puzzles the player has repaired (P8e), each by its key: its path under machines/,
// "030-dice/puzzles/01-dead-button.bwp" ('/' on every platform).
class Progress {
public:
    bool has(std::string_view key) const;
    bool add(std::string key);  // true when new
    void merge(const Progress& other);  // every key of both
    void clear();
    bool empty() const { return keys_.empty(); }
    const std::set<std::string, std::less<>>& keys() const { return keys_; }

    // "# Bodgewire progress\n", then one key a line, sorted.
    std::string text() const;
    // Each line trimmed (Trim); blank and '#' lines skipped; duplicates once. Never fails.
    static Progress Parse(std::string_view text);

private:
    std::set<std::string, std::less<>> keys_;
};

// A listed Puzzle's key: its Machine's directory name, "puzzles", its file name. No file system
// access: the Launcher calls it per post-it per frame.
std::string ProgressKey(const MachineEntry& machine, const PuzzleEntry& puzzle);
// The key of `file` when it is one of the list's Puzzles (EntryOf: a puzzle, not `random`);
// nullopt for the working Machine, a Vetting Script, or a file not in the list. Once per bench.
std::optional<std::string> ProgressKey(const MachineList& list, const std::filesystem::path& file);

// Missing: empty. Present but not a regular file, or not opened: the error ("cannot read …").
std::expected<Progress, std::string> ReadProgressFile(const std::filesystem::path& path);
// Written to `path` + ".tmp", then renamed over it (a crash leaves the old file). The parent
// directory is not made. The error ("cannot write …").
std::expected<void, std::string> WriteProgressFile(const std::filesystem::path& path,
                                                   const Progress& progress);

}  // namespace bw
```

`ProgressKey(machine, puzzle)`: `machine.dir.filename().generic_string() + "/puzzles/" + puzzle.file.filename().generic_string()`; the `(list, file)` form finds the entry with `EntryOf` (paths with `..` or absolute give the same key) and calls it. `ReadProgressFile`: `std::filesystem::status(path, ec)`: not found → empty; found and not `is_regular_file` → error (on macOS an `ifstream` on a directory opens and reads `""`); then `ReadTextFile` nullopt → error; else `Parse`.

### Tests (`tests/machine/progress_test.cpp`, `[machine]`)

- `progress: text`: two keys added out of order give the header and them sorted; `Parse(text())` round trip; blank lines, `#` lines, spaces around a key and a duplicate key parse to the same set.
- `progress: add, merge and clear`: `add` true then false for the same key; `has`; `merge` the union; `clear` empties.
- `progress: key`: a temporary `machines/` written with `WriteFile` (`tests/temp_files.hpp`): a Puzzle gives `"010-a/puzzles/01-x.bwp"`, reached by a path with `..` in it too, and the entry form gives the same; the netlist, `random.bwr` and a file outside give nullopt.
- `progress: file`: a missing file reads empty; written then read gives the same keys and no `.tmp` left; a directory in the file's place gives an error to read.

## 2. Where it is kept (§2, §4)

### `src/ui/progress_store.hpp`

```cpp
namespace bw {

// Where the player's Progress lives (P8e): a file (--progress, else progress.txt in
// SDL_GetPrefPath("bodgewire", "bw_ui")); on the web always localStorage["bodgewire.progress"];
// or nowhere (--screenshot without --progress: nothing read, nothing written; and after a
// load that failed, so a file that could not be read is never overwritten).
class ProgressStore {
public:
    static ProgressStore For(const Options& options);  // after SDL_Init
    Progress load();  // an error on stderr, then empty, and the store becomes None
    // Re-read, merged with `progress` and `key` added, written; `progress` becomes the merge
    // (another instance's ticks kept). Errors on stderr.
    void add(Progress& progress, std::string key);
    void clear(Progress& progress);  // `progress` emptied and written empty
private:
    enum class Where : std::uint8_t { None, File, Browser };
    Where where_ = Where::None;
    std::filesystem::path file_;
};

}  // namespace bw
```

`For`: under `__EMSCRIPTEN__` always Browser (the page's `?arg=` can pass flags, src/ui/web/shell.html, but a file there would be in MEMFS, lost on reload). Natively: `options.progress` → File at it; else `options.screenshot` → None (`--visit` needs `--screenshot`, ParseOptions); else `SDL_GetPrefPath` (it makes the directory; null → None, the SDL error on stderr; freed with `SDL_free`) plus `progress.txt`.

Browser: two `EM_JS` functions in `progress_store.cpp` under `#ifdef __EMSCRIPTEN__`, with `EM_JS_DEPS(bw_progress, "$stringToNewUTF8,$UTF8ToString")` (the installed emsdk includes no library functions by default: without it the JS throws and the load is silently empty). One returns `localStorage.getItem("bodgewire.progress")` as `stringToNewUTF8` (freed with `free`; 0 when absent or when `localStorage` throws: a JS `try`), one calls `setItem(…, UTF8ToString(text))` in a JS `try`. `main` runs on the browser's main thread (no `PROXY_TO_PTHREAD`), where `localStorage` exists. No C++ `try` near the frame loops (AGENTS.md, web).

## 3. `--progress FILE`, `--forget`

`Options::progress` (`std::optional<std::filesystem::path>`) and `Options::forget` (`bool`), `Flag{"--progress", "FILE", true, …}` and `Flag{"--forget", "", true, …}` after `--make` in `kFlags`. `--forget`: the Launcher opens its question at start (the agent's screenshot, like `--leave`, `--ask`); with a file it is a usage error, as `--make` is without `--select` (check how ParseOptions rejects launcher-only combinations and do the same). Cases in `bench_options_test.cpp`: `--progress /tmp/p.txt` alone and with a file; `--forget` alone; `--progress` without a value is a usage error.

## 4. The bench saves at its tick (§2, §3)

`RunBench` gains a last parameter `const std::function<void()>& repaired` (empty: nothing). In `main`, before the bench: `const auto key = machinesDir ? ProgressKey(launcher.list(), *benchOptions->file) : std::nullopt;` and the callback `[&] { if (key && !progress.has(*key)) store.add(progress, *key); }`. In `RunBench`, after `ack.update(…)`: a `bool wasTicked` kept across frames; on `ack.ticked() && !wasTicked`, `repaired()`. A Random fault and the working Machine have no key (§5); `RepairAck` itself is unchanged, and the brief's `ticked` stays the bench's own (§3: reopened fresh).

`main`: `ProgressStore store = ProgressStore::For(options); Progress progress = store.load();` after `SDL_Init`; `launcher.setProgress(&progress, [&] { store.clear(progress); })`.

## 5. The Launcher (§1, §4)

### `src/ui/brief_window.hpp`

```cpp
// The owner's ballpoint tick (P8c) in a `size` px square at `at`: the brief's, the Launcher's
// post-its' (P8e).
void DrawTick(ImDrawList& list, ImVec2 at, float size);
```

The strokes as `DrawBrief` draws them today (brief_window.cpp): `{at.x + 1, at.y + 0.55·size}` → `{at.x + 0.35·size, at.y + 0.9·size}` → `{at.x + size, at.y + 1}`, `kBallpoint`, `std::max(2.0f, size / 12)` px thick: the brief's tick unchanged (`DrawTick(list, pen, line)`).

### `src/ui/launcher.hpp`, `.cpp`

- `void setProgress(Progress* progress, std::function<void()> forget);` (nullptr: no ticks, no hint). `forget` is called inside `draw` at the question's OK, so a forget is saved at once, even if the tab is closed before any choice.
- `void askForget();` (`--forget`: the question opens at the next draw).
- `DrawPostIt` gains `bool ticked = false`: after the text's clip is popped, `DrawTick(list, {end.x - kNotePad - kTickSize, end.y - kNotePad - kTickSize}, kTickSize)` with `kTickSize = 34`, over the brief. The post-it lambda passes `k > 0 && k != randomAt && progress_ && progress_->has(ProgressKey(machine, machine.puzzles[k - 1]))`: no cache (at most 19 post-its a row, the BW-1's; no file system access).
- The hint: shown while some listed Puzzle is ticked, computed each frame over the list with the entry form (114 string builds, no file system). The text "Delete: forget the ticks" (the UI font, Proggy, has no ⌫; the Mac's backspace key is labelled "delete") in `kSpeedInk` at the title band's right (`origin.x + width - kNoteLeft - textWidth`, `origin.y + kTitleTop + 8`), an `InvisibleButton` over it when `clicksOn`.
- `ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false)` in the keys block (not while `making_`, only while the hint shows), or the hint's click, or `askForget()`, opens `QuestionPopup forget_{"##forget", "Forget every repaired Puzzle?", false}` at the hint's bottom-right, pivot `{1, 0}`. While `ImGui::IsPopupOpen("##forget")`, the arrow, Enter and Space keys are skipped. `forget_.draw()` last in `draw`, outside `PushID("postits")`; `Ok` → `forget_()` callback.

## 6. Docs

- GLOSSARY: **Saved progress** in Puzzles (the keys, where kept, the tick on the Launcher, never taken back by a bench, forgetting); the **Launcher** entry gains the ticks, the hint, Delete and the question (its "no scores, no unlocking, no difficulty" stays true: a tick is none of those); the **Brief** entry says the Launcher's tick stays when the bench's goes; Repair validation's "it goes when a later Repair breaks the Machine again" says the saved one stays.
- README_SPEC.md, the launcher paragraph: one sentence, a repaired Puzzle's post-it keeps its tick on the launcher, across launches. README regenerated (text only: the shots use no progress, so no picture changes).
- AGENTS.md: the screenshot line notes that a shot never reads or writes the player's progress unless `--progress` is given.
- ARCHITECTURE: the P8e row done; P7's "no solved marks" left as history (the P8e row says it is lifted).

## Checks

- Release: `ctest --preset release -j -R "^(progress|bench options|machine list): "` per phase; the full release suite at the end (shared code: the machine library).
- Debug: `ctest --preset debug -j -R "^(progress|bench options|machine list): "` (new C++ logic).
- Screenshots (release, viewed): Dead button repaired with a wire, `--progress /tmp/bw_p8e.txt --screenshot … --after 5s`: brief ticked, the file holds the key; the Launcher `--progress /tmp/bw_p8e.txt --select machines/030-dice/puzzles/01-dead-button.bwp --screenshot`: the ticks and the hint; the same with `--forget`: the question under the hint; a progress file with only an unlisted key: no tick, no hint; the bench reopened on Dead button with the same file: no tick on the brief.
- Web: `cmake --build --preset web` builds; the browser check by the user (§2: a tick kept after a reload).
