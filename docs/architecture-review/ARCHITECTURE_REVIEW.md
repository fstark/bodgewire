# Architecture review — plan of action

An architecture study of the whole of `src/`, done 2026-11-23 when the user considered the game finished, and the refactors it led to. No code changed during the study; this file is the plan to work through. Each item becomes one commit or more; the larger ones (C, D) get a short plan of their own before starting, in the shape of [ROADMAP_STEP.md](../recipes/ROADMAP_STEP.md). Update the status table as items land.

## The user's decisions (2026-11-23)

- "Finished" is no reason not to refactor: a sound base serves any later step. Items are ranked by value, not filtered by whether a roadmap row needs them.
- Machine-specific code is legitimate where it models exact hardware: the BW-2001 keyboard is exactly a PET 2001's and the BW-80's exactly a ZX80's, used by no other Machine (unlike a CPU, or the ASCII keyboard, which are reused). Their key tables, drawings, the ZX80's keyword entry and `KeyMatrixKind {Bw2001, Bw80}` stay. Their Part names should say so (A4).
- Why only the BW-2001 keyboard is in the builder, and not the BW-80's: the PET's keys are bare contacts that join a row Net to a column Net both ways (only the Board's switch network can join Nets); the ZX80's membrane has a diode per row, so a key only pulls a column low, an ordinary Part. The code follows the circuits.
- `bw run` on a netlist simulates the design, `bw_ui` the copper: intended. It tests a circuit before its Board is routed, and `LoadMachine` refuses a Board whose copper doesn't match the netlist (`CheckBoard`), so they are the same circuit once routed.
- The placer's `grid pet`: parked, to be explained again (the roadmap's row "Placer's `grid pet`").

## How it was measured

Pointers to where to read, not verdicts:

- Include edges between libraries against the layer order; ImGui/SDL includes outside `src/ui`.
- Function complexity (lizard 1.26) and methods per class.
- Concept spread: how many files and libraries name one instrument, one device, one Part, one Fault kind.
- Machine names in the general libraries; glossary _Avoid_ words in identifiers; glossary terms the code never uses.
- Then five fresh reviewers read one area each (sim + parts; netlist + board + machine; runner + devices + instruments + session; view + ui windows; the two `main.cpp`, asm, manual, text), against the glossary, the recipes and the open roadmap rows. Their main claims were checked in the code.

Git history was not used to predict future changes: it records the building phase.

Numbers at the time: `ui` 12,980 lines in 65 files, `parts` 12,199 in 113. Largest functions: `RunWindow` (ui/main.cpp, 707 lines, complexity 244), `BoardView::draw` (490, 158), `AnalyzerWindow::draw` (496, 130), `ParseOptions` in ui/main.cpp (252, 121), `Z80Core::run` (721, 139: the instruction set's size). Most methods: `Session` 83, `BoardView` 46, `Z80Core` 45. An instrument is named in 14–19 files over 5–6 libraries; the TV in 22.

## Healthy: keep

- The layer order holds everywhere; no ImGui or SDL outside `ui`.
- The kernel (about 1,950 lines) is small and coherent; every Part reads and drives only through `EvalContext`, the single entry point that makes generated code or two threads possible.
- CPU and VIA cores are separate from their Pins and use no simulator types.
- `view` is pure, headless and tested (22 test files); the UI does not leak Faults.
- `Machine`, the parsers (`.bwn`, `.bwp`, `.place`, Repairs, S-expressions), the KiCad reader/writer/copper split, `asm` (one design for both CPUs), `manual` and `text` are the size their job needs.
- Runner, `Consumer`, Dispatcher, `SpscRing`, `TripleBuffer`, the pure instrument engines (`ProbeState`, `ScopeState`, `AnalyzerState`) and devices (`Tv`, `LedGlow`).

## Plan

### A. Small fixes for real inconsistencies (one commit each)

**A1. One `Machine::apply(RepairAction)`.** `cli/main.cpp` `Repair()` (an if-chain of `get_if` ending in an unchecked `std::get<UnbridgeRepair>`) and `Session::repair` (`std::visit`) both apply Repairs, and already differ: the CLI warns "no crack near" on a mend, the Session counts every mend as done. A new Repair kind would crash the CLI at run time. Both call `Machine::apply`. About 30 lines, 3 files, low risk.

**A2. One `FindTvNets`.** The VIDEO/MONITOR connector lookup exists three times: `session.cpp` (`HasTv` and `makeLive`, whose comment says "the two must agree") and `cli/main.cpp`. About 30 lines, low risk.

**A3. One channel setup for scope and analyzer.** `runner/scope_consumer.cpp` and `runner/analyzer_consumer.cpp` each have `pending_`, the mutex, `generation_`, `adoptNets` with masks, `maskOf` and the watch/unwatch diff; the copies drifted (`netsGeneration_` set before the lock in one, inside it in the other). One `ChannelNets<N, Mask>` helper. About −70 lines, low risk; check under the debug sanitizers.

**A4. Keyboard Parts named after their Machines; Parts found by kind.**
- `MATRIXKB` → `BW2001KB`, `MEMBRANEKB` → `BW80KB` (catalogue, netlists, manuals, tests; about 40 files). `KEYBOARD` (the ASCII keyboard, reused) keeps its name.
- Session finds Parts by name strings (`"PADDLE"`, `"KEYBOARD"`, `"MATRIXKB" || "MEMBRANEKB"`, `session.cpp:637-710`) where kind finders exist (`KeyMatrixOf`, `PartKind`); `"SLIDE"` is tested by name in `ui/main.cpp`, `ui/board_tags.cpp` and `parts/catalogue.cpp` (let `Switch` say whether it latches).
- The builder (`builder.cpp:501-515`) checks the name and fetches `Bw2001Keys()` itself; ask the keyboard Part for its contacts and their two Pins instead (a second bare-contact keyboard, a PET 4032's, would then need no builder change).
- Low risk.

**A5. Test fixtures out of `sim/primitives.hpp`.** `Gate`, `TriStateBuffer`, `DFlipFlop`, `ClockSource`, `Stimulus` are used only by tests and benches; 25 Parts include the header only for `Delays`. `Delays` gets its own header, the fixtures go to the tests. Also: `Z80STUB` (bench only) in the shipped catalogue. Low risk. Done: `sim/delays.hpp` (`Delays`, `DelayFor`), `sim/gate_function.hpp` (`GateKind`, `GateFunction`), the fixtures in `sim/fixtures.*` as library `bw_sim_fixtures` (tests and bench only); `Z80STUB` kept, since the bench netlists load it through `bw run`.

**A6. Docs that describe what isn't there.**
- ARCHITECTURE's `m.injectFault(...)` example (no such API until C10) and "overridable per instance" delays (not built until D19): mark as intent.
- ADD_INSTRUMENT.md: `kMaxWatched` 96 → 128, Clips 32 → 64.
- ADD_CONNECTOR_DEVICE.md: a new key matrix needs more than "the kind's key table" (until A4).
- Glossary: Sample, Consumer, Slice; the stale `consumer.hpp:12` comment ("instruments later").

### B. Names (one mechanical commit)

- **B7. "Bus" with one meaning** (the glossary's group of Nets; the roadmap's "Bus as one entity" needs it): `PinBus` → `PinGroup`, `isPlainBus` → `isSharedNet`, `Z80Bus`/`Mos6502Bus` → `Z80Cycle`/`Mos6502Cycle`.
- **B8. Cut and Broken Trace.** One `Crack` type holds both the Fault and the Repair (`machine.hpp`: `std::vector<Crack> cracks_; std::vector<Crack> cuts_;`, `FindCrackTrace` used by `cut()`), and `CutMark` draws both. "crack" is not a glossary word: add it as the file word for a Broken Trace, or rename the geometry `TraceGap`. Also `Board::bodgeWires` holds Solder Bridges too.
- **B9. Others.**
  - `Focus::key` (named after keys P6o removed; means "out with the focus").
  - `Loaded` → e.g. `OpenedBench`; `RunWindow` → `RunBench`.
  - The runner's `Marker`/`MarkerKind` clash with the glossary's Marker.
  - `BoardView::chip()` returns any Component.
  - Two unrelated `PlacedPart` types (`builder.cpp`, `placer.hpp`).
  - `Options` reused as each bench's settings.
  - Layer names parsed in three places (`LayerName` in `machine/puzzle.hpp`, `repair.cpp`, `kicad_board.cpp`).
  - `ParseMillimetres` (board/kicad_board.hpp) missing from AGENTS.md's shared helpers.

### C. Structure (several commits each, a short plan first)

**C10. Faults as one type, added by `Machine::inject`.** Six Decl structs and vectors in `Puzzle` (`machine/puzzle.hpp`), six member vectors plus a private copy `Faults loaded_` in `Machine`, `StuckPin`/`StuckCell`/`StuckSwitch` as positional spans of `BuildCircuit`. `LoadMachine` (complexity 49) is mostly six inline validation loops; `LoadNetlistMachine` builds a fake Puzzle. A new Fault kind touches about 5 files and 6 places in machine.cpp.
- Fix: `FaultDecl` as a variant and one list in `Puzzle`; `Machine::inject(const FaultDecl&)` holding today's loop bodies; `LoadMachine` = load the clean Machine, then inject; one `FaultSet` for current and loaded (undo = assignment); `BuildCircuit(netlist, BuildFaults)`.
- Diagnostics are grouped by kind today: sort them by line so the tests on messages still pass.
- Opens procedural Faults (a clean Machine reused, Faults injected) and intermittent ones (one place to add activation).
- About 250 lines moved, 6 files; low–medium risk.

**C11. Split `ui/main.cpp`.** `RunWindow` (605–1354) does about ten jobs:
- builds 14 bench objects and applies about 25 start presets;
- routes keys (Machine, ImGui, Tab, backtick);
- handles every tool's click (`switch (focus.owner())`: game logic);
- runs the power-cycle state machine;
- copies Session state into BoardView tags each frame;
- keeps the button model (latched, slid, held, scripted);
- replays `--type`, sets the window order;
- times frames and ends on screenshot or visit.

About 40 % of the file is test and screenshot options (usage, `Options`, `ParseOptions`, `PrepareBench`, 47 `options.` uses in RunWindow), all in an anonymous namespace.
- Fix: options into their own file as a `BenchPreset` with a table-driven parser, applied in one place; `RunWindow` → a `Bench` class whose members are today's locals; tool clicks and the button model to `view` next to `Focus` (`BoardClick` has no ImGui); tag copying into `board_tags.cpp`.
- `--binder-height` parses "N%" by hand (250 % allowed, so `ParsePercent` needs a range).
- `roms/contents.txt` is read inside RunWindow and ignores `--roms` (roadmap row "BW-2001 debug ROMs' contents"): read it into `Loaded` from the ROM directory actually used.
- About 1,300 lines moved; medium risk; check with `tools/readme_shots.py` screenshots.
- Done 2026-11-23 in two phases: the options in `view/bench_options` (one table, tested); out of `RunBench` into their own files: the flags' checks (`ui/bench_setup`, `ApplyBenchOptions`), the tool clicks (`ui/tool_click`, `ToolClick`, `RepairPlan`, `WireDraft`), the button model and `--type` (`ui/bench_input`), the instruments' tags (`board_tags`). `main.cpp` 1,820 → 1,015 lines, `RunBench` 707 lines / complexity 244 → 501 / 157. Left for D23: a `Bench` class from `RunBench`'s locals, with the one frame loop it needs. The tool clicks stay in `ui` (they act on the Session; `view` does not depend on it).

**C12. One identity per bench object.** `Owner` (view/focus.hpp), `BenchObject` (view/bench_places.hpp) and `Item` (ui/shelf_drawings.hpp) overlap (`Item` also has the power and run switches), mapped by `OwnerOf`/`WindowOf`/`ObjectOf` (shelf.cpp) and the `--pop` switch in main.cpp. Out/away is stored twice: `Focus::out_` (instruments) and public `Shelf` bools (`tvOut`, `keyboardOut`, `manualOut`, `knobOut`, `briefOut`). Three ways of being a window (`bool draw(bool&, Session&)` + `BeginInstrument`; `void draw` + `PlaceNext`/`KeepAspect`/`CaseClose`; free `DrawBrief`/`DrawDebug`). Window names scattered.
- Fix: one table in `view` (object → owner, shelf item, window name, kind); out/away beside `Focus`, which keeps only the owner; one window shape.
- About 300 lines, 8 files; medium risk; extend focus_test and machine_slot_test.
- Done 2026-11-23, narrowed: the three enums name three things (who acts on a pin click, a window on the bench, a button on the shelf) and overlap only on the instruments, through two small mappings (`ObjectOf` now in `view/focus`, `WindowOf`); merging them would blur that. What was duplicated is out or away: now all in `Focus` (`out(BenchObject)`, a reference a closing window writes), the shelf's five bools and the bench's `debugOut` gone. The three ways of being a window are left as they are (the instruments' `draw` returns a click for the focus; the cases and the brief need none).

**C13. Split `Session`.** 83 methods in groups: lifecycle 10, repairs 4, probe 4, analyzer 19, scope 10, meter 10, LEDs 1, input devices 18. Its real job, keeping what the player set and re-applying it after each rebuild, is spread over `makeLive` (every consumer by hand), `powerOn` (knobs), four release loops in `powerOff`, `touchLive`/`configureLive`.
- Fix: one object per group (analyzer, scope, meter, probe, input devices), each with `consumers()`, `rebind(loaded, runner)`, `release()`; Session keeps lifecycle and Repairs and hands out the groups (`session.analyzer().clip(…)`). Analyzer first.
- Run control through Session too: pause, resume and status go through `session.runner()` today (shelf.cpp, main.cpp, launcher.cpp), and `tv()` hands out the `TvConsumer`.
- About 800 lines moved into 5 file pairs; about 960 call sites renamed in 14 UI files and the tests; low risk to behaviour, high churn.
- Done 2026-11-23 in three phases: `InputDevices` (`session.inputs()`), `AnalyzerSetup` and `ScopeSetup` (`analyzer()`, `scope()`), `ProbeSetup` and `MeterSetup` (`probe()`, `meter()`), each bound to each build in one `bindBuild()`; `session/leads` holds `PadLead`, `PadLeadOf` and `SessionBuild` (the Machine, the build, powered: on the heap with the Machine, now a `unique_ptr`, so the setups survive a Session's move). Session: 1,069 → 361 lines, 83 → 35 methods (lifecycle, Repairs, LEDs, the groups). Run control through Session (`pause`, `resume`, `status`); `runner()` kept for the tests' stepping. About 500 call sites renamed.

**C14. Input as shared gestures** (prepares [mobile](../mobile/MOBILE.md), whose idea 3 assumes it). Input is read raw from ImGui in about 10 files:
- the scope and analyzer canvases repeat wheel, pan, click-or-drag and right-click-clears (scope_window.cpp, analyzer_window.cpp);
- the TV knobs copy the paddle knob's drag and wheel constants ("(the paddle's)", tv_window.cpp, knob_window.cpp);
- the "second click" is Right, Shift, Alt or Cmd depending on the place (board_view.cpp ×4, analyzer, scope, matrix keyboard).

Fix: a time-canvas helper returning intents for both windows, one `DragKnob` widget, one `SecondaryClick()`. Touch then lands in 3 helpers and `BoardView::input`. About 250 lines, low risk.

Done 2026-11-23: `ui/gestures`: `TimeCanvas::read` (hover, zoom notches, wheel pan, fit, clear marker, press, drag, click as marker, release) for both windows, `ReadKnob` (drag up, wheel, double-click) for the TV's controls and the paddle controllers. No `SecondaryClick`: the modifiers mean different things (Shift-click a whole bus, Alt the layer, a right-drag pans, a right-click latches or clears, Cmd frames), so there is no one "second click" to share; mobile will map each. Needs a check by hand (drag, wheel, double-click, right-click on both canvases and the knobs): screenshots cannot drive them.

**C15. `BoardView` and `Shelf` logic into `view`.**
- The hovered part's wiring is recomputed each frame inside `BoardView::draw` (pads × pads): precompute it in `BoardScene`, tested.
- `focus()` bounds → `BoardScene::boundsOf`.
- `route()` (the Focus rule, reading `io` itself) → `RouteLink(owner, target, modifiers)` in view/focus.
- The per-frame setters (`setClips`, `setHooks`, `setLeads`, …) → one `BoardOverlay` per frame (the launcher's still picture can reuse it).
- `draw` split into copper, devices, overlays, flip corner.
- `Shelf::draw` (281 lines): tray layout → a tested `ShelfLayout` in view; clicks return actions applied in one place; the meter's power switch repeats the shelf's power logic.
- About 400 lines moved, low risk.
- Done 2026-11-23: `BoardScene::wiring` (per-Net pad counts and the rails found once with the scene, not pads × pads each frame) and `boundsOf`/`smallestPad`, `BoardTarget` to `view/board_scene`; `RouteLink` in `view/focus`; `LayOutShelf` in `view/shelf_layout`; each tested. Not done, judged cosmetic: one `BoardOverlay` in place of the per-frame setters, `draw` split into parts, shelf clicks as returned actions.

**C16. The catalogue holds each Part's pins and parameters.** Pin names exist only once a Component is built: `manual.cpp` `PartPinNames` builds a throwaway one with made-up parameters and special cases (6530, OSC, POR, PADDLE); a new Part needing parameters silently gets no pin check. Each factory parses its own parameters (`catalogue.cpp`, about 300 near-identical lines). `DurationFromText` lives in `paddle.hpp`. Fix: a static pin list and declared parameters on `PartInfo`, one parser. About −200 lines, low risk. Done 2026-11-23: `PartInfo::sample` (the parameters that build an example) and `PartPinNames` in the catalogue, the manual's special cases by name gone; `TakeParams` (each parameter known and given once) in the seven factories that take some; `ParseDuration` and `ParseFrequencyPeriod` in `sim/time.hpp`, in place of `DurationFromText` (paddle.hpp), `PeriodFromFrequency` (oscillator.hpp) and their netlist wrappers. A parameter given twice is now refused everywhere (some Parts took the last). Not done: a static pin list on `PartInfo` (the pins come from the Component; `sample` is enough to list them).

**C17. One way to feed signal samples.** The headless instruments (`LogicProbe`, `Multimeter`, `Oscilloscope::update`, `LogicAnalyzer::update`, `TvInput::update`; used only by the CLI and tests) each merge Samples by time their own way, beside the threaded Dispatcher and Consumers the UI uses; the VIDEO_N inversion is written twice (tv_input.cpp, tv_consumer.cpp). Fix: run the Dispatcher on one thread (`FeedInline`), the CLI and tests drive the Consumers, the wrappers go. About −160 lines, about 12 test files; medium risk. Done 2026-11-23, narrowed: what was duplicated is in one place, the wrappers stay as the synchronous API that `bw run` and the tests step (16 files use `TvInput`; the instruments' Consumers need a Runner to be configured). `Recorder::inOrder` merges several Nets' Samples by time (it replaces `TvInput`'s k-way pick and the Oscilloscope's and Logic Analyzer's own sorts), and `Tv::feed(input, …)` maps the connector's inputs (the separate drives' inverted video once, for `TvInput` and `TvConsumer`).

**C18. Pin references.**
- Four shapes: `PinRef` as written (number or name); `PinRef` holding a number string (`dry_`, `WireEnds`, `padOf`); `(reference, PinIndex)` (`StuckPin`, `Connection`, `pinNet_`); `Pad::number`.
- `Builder::resolvePin` nearly copies `FindPin`; `std::stoul(pin) - 1` in machine.cpp and session.cpp.
- Fix: `resolvePin` calls `FindPin`; one resolved `PinAt{reference, PinIndex}` after loading, `PinRef` only for text as written.
- About 100 lines; low–medium risk.

### D. Foundations (kernel, threads, frame loop)

**D19. Delays per Component.** ARCHITECTURE promises delays "overridable per instance (slow or wrong part, timing fault)"; 32 Parts use global `delays::k…`, and `gate_package.cpp`/`rom.cpp` bake them into template parameters. Fix: a delay scale per Component applied in `EvalContext` (`driveChanged`, `wakeAfter`), set by the builder from a Fault. About 40 lines; traces unchanged at scale 1. Parts timing from `lastChange` (ram2114, rom) only scale approximately.

**D20. One random stream per Component.** One `Rng` per Simulator (`simulator.cpp`), 85 draw sites in Parts: any new draw shifts all later ones, so a flaky Fault stops reproducing after an unrelated change, and two threads would not replay. Fix: seed each Component's stream from (seed, ComponentId). About 30 lines; every seeded outcome changes once (tests pinning values updated).

**D21. Input devices through a Runner command.** Outputs all go through the Runner's rings; each input has its own mailbox written from the UI thread (`Switch::pressed_`, `Paddle::knob_` atomics, `Keyboard`'s own queue, `PressQueue`), plus `runner->wake(id, PressJitter())` at 5 Session sites. Fix: `Runner::post(function<void(Simulator&)>, jitter)` applied at the slice boundary; Parts back to plain fields; `Keyboard` reuses the generic queue. Medium risk: knob reads move to slice edges (1 ms).

**D22. Network Pins unified.** `PinKind::Switched` and `PinKind::Joined` differ only in strength; the kernel has fixed slots (`external_`/`external2_`, `networkPin`/`secondNetworkPin`) and refuses two network Pins of one kind on a Net; `SwitchContact` wakes its network through a raw pointer (the only cross-Component wake). Intermittent contacts would grow this. Fix: one `PinKind::Network` with its strength in `PinSpec`, an `External` per network Pin. About 80 lines; hot path, measure speed before and after. Related, cosmetic: `PinSpec`'s three positional bools → `WakeOn {Any, Rise, Fall, Never}`.

**D23. One frame loop.** `RunLauncher` and `RunWindow` each have their own poll/render/present/screenshot loop (the speed line printed twice); `main()` destroys and rebuilds `ImGuiScreen` and the fonts between them; `launcher.stop()` drops the live Session and the bench loads the Machine again. P7c needs both screens in one frame and the Session handed over. Fix: one loop over screens (launcher, bench, transition), `Loaded` handed from launcher to bench. Medium–high risk: unsure whether a bench can be fresh without a new ImGui context. Suits the web rule (no `try` around a frame loop).

Not in this plan: the kernel's multi-Pin `EvalContext` and Bus as one entity are roadmap rows of their own.

## Order and status

A and B first (cheap; each commit runs the full release suite; later work reads better on clean names), then C10–C12 (the most-edited code), the rest of C, D last (hot-path speed, seeded values, ImGui contexts).

| Item | Status |
|---|---|
| A1 `Machine::apply` | done 2026-11-23 |
| A2 `FindTvNets` | done 2026-11-23 |
| A3 scope/analyzer channel setup | done 2026-11-23 |
| A4 keyboard Part names, Parts by kind | done 2026-11-23 |
| A5 test fixtures out of primitives | done 2026-11-23 |
| A6 docs | done 2026-11-23 |
| B7–B9 names | done 2026-11-23 |
| C10 Faults as one type | done 2026-11-23 |
| C11 split ui/main.cpp | done 2026-11-23 (the Bench class with D23) |
| C12 bench object identity | done 2026-11-23 (narrowed: out or away in one place) |
| C13 split Session | done 2026-11-23 |
| C14 shared gestures | done 2026-11-23 (to try by hand) |
| C15 BoardView/Shelf logic to view | done 2026-11-23 (the rules; not the cosmetic splits) |
| C16 catalogue metadata | done 2026-11-23 |
| C17 one sample feed | done 2026-11-23 (narrowed: one merge, one TV input map) |
| C18 pin references | todo |
| D19 delays per Component | todo |
| D20 random stream per Component | todo |
| D21 inputs through the Runner | todo |
| D22 network Pins | todo |
| D23 one frame loop | todo |
