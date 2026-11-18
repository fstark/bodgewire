# P6j — Pop-out windows — Implementation Plan

Spec: [POP_OUT.md](POP_OUT.md). Design: [POP_OUT_DESIGN.md](POP_OUT_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Spike (branch `p6j-spike`): SDL GPU, viewports, the TV picture, `--pop`, the composed screenshot | |
| 2 | Beside the main window, back on the bench, the binder's control, keys on the focused window | |
| 3 | Glossary, recipes, ARCHITECTURE | |

## Phase 1 — Spike: SDL GPU, viewports, `--pop`, screenshot

On a branch `p6j-spike` from `main`. The row's spike: the backend switched, the scope popped out, the TV picture drawn, the speed ratio unchanged. If the gate fails (speed under 1.00× on the 110, a UI time well above today's, a window ImGui cannot pop out on macOS), stop and report to the user with the numbers; the branch stays unmerged.

1. Before switching: on `main`, the 110 with every instrument (`--group`, `--hook`, `--meter`), Tennis with `--manual user` and with `--tv bright:60%,contrast:90%`, the BW-80's first Puzzle: speed ratio and UI ms recorded in the notes, screenshots kept for comparison.
2. `src/ui/CMakeLists.txt`: `GIT_TAG v1.93.0-docking`; `imgui_impl_sdlgpu3.cpp`.
3. `gpu_render.{hpp,cpp}` as designed; `tv_view` on the GPU.
4. `bench.{hpp,cpp}`: `PoppedOut`, `PopOutAtStart`, `OnBench`, `PlaceNext`'s viewport choice (the pending position at the default place: no `BesidePlace` yet); `brief_window.cpp` `OnBench`.
5. `main.cpp`: the renderer, viewports on, `OnBench` for the main window, `--pop`, its refusals, the usage.
6. Screenshots (release), each viewed: Tennis `--manual user`, Tennis `--tv bright:60%,contrast:90%` (the tone on the GPU texture), the BW-80's first Puzzle (the TV's text) and the 110 with every instrument, each compared with step 1's; `--pop scope` on the 110 with a hook (the composed PNG: the scope in its own window at its default place); `--pop tv` on Tennis (the picture in a popped window). `--pop keyboard` on the Counter: usage error, exit 2; `--pop nonsense`: usage.

**Fence:** release build; the full release suite passes (the UI build is shared); the 110 with every instrument at 1.00×, UI ms near step 1's, the frame rate kept, with and without `--pop scope,analyzer,meter`; screenshots viewed. clang-format on the changed C++ files. Then `main` fast-forwarded to the branch.
Commit: `ui: SDL GPU and ImGui multi-viewports: the TV picture a GPU texture, --pop puts an object in its own desktop window, the screenshot composes them (P6j phase 1, spike)`.

## Phase 2 — Beside, back, the binder's control, keys

1. `bench_places`: `Area`, `BesidePlace`, `FitBench`; the spec's cases in `bench_places_test.cpp`.
2. `bench.cpp`: `PopOut`, `PopIn`, `BesidePlace` from `platform_io.Monitors`' work areas and the main viewport, `SendHome` popping in, the `FitBench` size.
3. `manual_window.cpp`: the control.
4. `keyboard_capture`, `matrix_capture`: `sync(focused)`; `main.cpp` passes `SDL_GetKeyboardFocus()`.
5. Screenshots (release), each viewed: Tennis `--manual user` (the control on the bench, under the Repairs tab, clear of the grip); `--pop manual` on Tennis (beside the window if the display has room, else where it was: which, in the notes; the control pointing in); `--pop tv,keyboard` on the Typewriter; `--pop knob` on Tennis; `--pop scope,analyzer,meter` on the 110. The click, the drag, the keys on a popped keyboard and a double-click home: by reading the code, then the user.

**Fence:** release build; the full release suite passes; debug build and `ctest --preset debug -j -R "bench places"`; the 110 with every instrument popped out at 1.00×; screenshots viewed. clang-format on the changed C++ files.
Commit: `ui: a popped-out object goes beside the main window and back to its place on the bench; the binder's pop-out control; text input on the focused window (P6j phase 2)`.

## Phase 3 — Glossary, recipes, ARCHITECTURE

1. GLOSSARY **Equipment**; ADD_INSTRUMENT Gotchas and Window row, ADD_CONNECTOR_DEVICE UI layer (design, Docs).
2. ARCHITECTURE: P6j done (2026-11-18) with the links; the renderer named.
3. This plan cut down to the phase table and the notes.

**Fence:** docs only, no build.
Commit: `docs: P6j done, pop-out windows`.

## Implementation notes
- Docs review (before the docs commit), fixed: the composed screenshot's order (main first, then by `LastFocusedStampCount`); `PrepareDrawData` per viewport in the screenshot (one shared buffer pair); the spec's screenshots narrowed to what the plan takes; the UI time taken before `present` (its vsync wait) and the frame rate printed; the screenshot after `present` (this frame's TV upload in); the TV uploaded with the main window minimized; tooltips and popups crossing the main window's edge become desktop windows (spec), a tooltip shown without the focus; `ConfigViewportsNoDefaultParent` is ImGui's default; the taskbar left as ImGui's default; the shutdown order; `cycle` on the texture upload; no debug run in phase 1 (no logic it reaches); ADD_INSTRUMENT's Window row named in the design.
