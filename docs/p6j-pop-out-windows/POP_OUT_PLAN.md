# P6j — Pop-out windows — Implementation Plan

Spec: [POP_OUT.md](POP_OUT.md). Design: [POP_OUT_DESIGN.md](POP_OUT_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Spike (branch `p6j-spike`): SDL GPU, viewports, the TV picture, `--pop`, the composed screenshot | 8275ad2 |
| 2 | Beside the main window, back on the bench, the binder's control, keys on the focused window | this commit |
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
- Phase 1, before (`main` at 7e16040, release): the 110 with every instrument (`--group U1.A0:8000,U1.D0 --sample 100ns --hook U1.RW,U1.A0,U1.A1,U1.D0 --timebase 200ns --delay 1us --probe U1.RW --meter D1.A --focus analyzer`) 1.00×, UI 0.5–0.6 ms; Tennis 1.00×, 1.6–1.9 ms; the BW-80's first Puzzle 0.20–0.21× (its usual speed), 1.2 ms.
- Phase 1, after: the UI time first read 8–12 ms: `present` holds the main swapchain's vsync wait, which `SDL_RenderPresent` did after the old measurement; and with a window popped out, Metal's drawable acquire inside `RenderPlatformWindowsDefault` blocks too. So the UI time is now taken after `ImGui::Render` (the frame built) and `--screenshot` prints the frame rate as well (the design followed). The 110 with every instrument: 1.00×, UI 0.2 ms, 119 fps (a 120 Hz display); the same with `--pop scope,analyzer,meter` 1.00×, 0.2 ms, 117–119 fps; Tennis with `--pop tv,knob,manual` 1.00×, 0.9 ms, 119 fps; the BW-80 0.22×. Screenshots viewed against the befores: Tennis with the binder, Tennis with the tone (the black level raised, as before), the BW-80's TV text and keyboard, the 110: the same. `--pop scope,analyzer,meter` on the 110: four viewports (logged once while spiking: main 1280×800 and three of the instruments' sizes at their default places), the composed PNG the main window's rectangle with the three drawn over it, the meter in front. `--pop tv` on Tennis: two viewports. `--pop keyboard` on the Counter: `this Machine has no keyboard`, usage, exit 2; `--pop nonsense`: usage. Full release suite 1451 passed (91 s). The spike gate holds: `main` fast-forwarded.
- Not seen by the agent (it cannot drive the mouse or watch the screen live): the popped windows as desktop windows (borderless, drag), a tooltip's window not taking the focus. User check.
- Phase 2: `SendHome` takes the window's name (the shelf has it) for a popped-out object's size. The agent's desk: the laptop's display (0, 33, 1512 × 949 work area, 120 Hz) and a 3008 × 1692 one up and to the left at 30 Hz (−3008, −1114); the main window 1280 × 800 at (116, 107). No room right of it nor strictly left (−500 straddles both displays), so `BesidePlace` gained: on a display further away, at its edge nearest the main window (a test with these numbers). `--pop manual` on Tennis: the binder at that display's right edge, the whole Board seen in the main window, its control pointing in; the control on the bench under the Repairs tab, clear of the grip. `--pop tv,keyboard` on the Typewriter: both on the other display, the picture drawn there. `--pop knob` on Tennis, `--pop scope,analyzer,meter` on the 110: out on the other display.
- Phase 2, the frame rate: with a window on the 30 Hz display the whole UI ran at 30 fps (its swapchain's VSYNC: Metal's drawable acquire blocks). The popped-out windows now present unpaced (`GpuRender::unpacedMode`: MAILBOX if the main window supports it, else IMMEDIATE, IMMEDIATE on Metal); the main window's VSYNC paces the loop. With windows on the 30 Hz display: 60–80 fps (Metal's few drawables still hold it under 120), speed 1.00×, UI 0.2–0.7 ms. Recorded in the design.
- Phase 2, seen: objects popped out together (`--pop`) are placed each alone, so they land at the same edge, overlapping (the scope over the analyzer, the keyboard over the TV). Only the binder has a control, so a player pops one at a time; left for later (spec).
- Phase 2 checks: full release suite 1458 passed (80 s); debug build, `-R "bench places"` 11 passed. The click, a popped window's drag, the keys on a popped keyboard and a double-click home: by reading the code. User check.
