# P6j — Pop-out windows

Roadmap step P6j ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): a bench object leaves `bw_ui`'s window for a desktop window of its own, and comes back. Design: [POP_OUT_DESIGN.md](POP_OUT_DESIGN.md). Plan: [POP_OUT_PLAN.md](POP_OUT_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

The bench is the main window's area above the shelf, and the Board fills it. The manuals binder is large (600 × 500 at first, 500 wide at least), and since P6i its links frame and flash parts, Nets and pins on the Board: a player reading the Service Manual wants the Board beside the page, not under it. The same holds for the scope and the analyzer on a busy Board. A desktop has room beside the window (a second display, or a wide one), and nothing in `bw_ui` can use it.

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| Which objects | every bench object can pop out (binder, TV, keyboard, knob, scope, analyzer, meter): one mechanism, the same code drawing it in either place (the row) |
| The control | **only the binder shows one**, at its bottom right: on the blue cover under the Repairs tab, clear of the resize grip, a reasonable size. The others are capable (`--pop` shows them) but have no control yet; whether a generic one comes later is open |
| The renderer | SDL GPU (Metal on macOS) replacing the SDL 2D renderer, with ImGui's multi-viewports (the docking branch, docking off) |
| Where it goes | beside the main window: just right of it, else just left, wherever a display has room; if none has, it stays where it was |
| Put away, out again | as it was left: still in its own window, where the player left it, for the run (as objects already come back where left) |
| Look | still no title bar, its proportions kept; docking stays rejected (P6b) |
| Back in | back at its place on the bench (the row) |

Decided while designing (2026-11-18), for the user to overrule:

- **The control**: 28 px, centred under the tab column, its bottom 28 px above the binder's (above ImGui's resize grip). A cream outline square with an arrow out of it to the upper right (on the bench) or into it from the upper right (in its own window), faint until hovered, as the case's `×`. Tooltip `Own window` / `Back on the bench`.
- **Back on the bench** at its default place (as a shelf double-click puts it), its size kept, shrunk in proportion if larger than the bench.
- **A double-click on the shelf drawing** (`SendHome`) brings a popped-out object back on the bench as well: home is the bench.
- **Independent windows**: a popped-out window is not tied to the main one (ImGui's default, `ConfigViewportsNoDefaultParent`; on macOS the backend never parents windows): it does not move with it, and either can be in front. It is an ordinary top-level window (a taskbar entry on Windows; nothing on macOS).
- **The brief and Debug windows** stay on the bench (the brief is glued to the Board's corner; Debug is the developer's).
- **On the bench, objects stay on the bench**: a window dragged past the main window's edge is clipped there, as today, never made a desktop window by the drag (it is locked to the main window; ImGui's default with viewports on would make it one).
- **Tooltips and popups** (a combo's list, a Board tooltip) that would cross the main window's edge become short-lived desktop windows of their own (ImGui's multi-viewports): they are no longer cut at the edge. A tooltip is shown without taking the keyboard focus (on macOS ImGui's backend activates every window it shows).
- **The keys**: a popped-out keyboard takes the typing when its window has the OS focus (text input follows the focused window).
- **`--pop OBJECT,...`** (`bw_ui`): `manual`, `tv`, `keyboard`, `knob`, `analyzer`, `scope`, `meter` out and popped out at start, placed beside the main window as a click would; refused (usage error) for an object the Machine lacks.
- **`--screenshot`** with popped-out windows: one PNG of the desktop rectangle holding all of `bw_ui`'s windows, each at its place, on a dark grey backdrop (the desktop is not captured), at the main window's pixel scale. With none popped out, the main window's framebuffer as today.

## What is new

1. **The renderer** — SDL GPU through `imgui_impl_sdlgpu3`, ImGui at `v1.93.0-docking` with `ImGuiConfigFlags_ViewportsEnable` (docking off); the TV picture a GPU texture uploaded in a copy pass; the screenshot drawn offscreen per window and composed.
2. **The bench** — each object locked to the main window unless popped out; a popped-out one in its own desktop window (`ImGuiViewportFlags_NoAutoMerge`), placed beside the main window, back at its default place when popped in.
3. **The binder's control** at its bottom right.
4. **`bw_ui`** — `--pop`; the keyboards' text input on the focused window.
5. **Docs** — GLOSSARY (Equipment), the recipes' window rows, ARCHITECTURE.

The kernel, Parts, the builder, devices, Session, Machines and Puzzles are untouched.

## Tests

- **Bench places** (`tests/view/bench_places_test.cpp`): `BesidePlace` puts an object right of the main window with the margin, its top kept; moved up to fit a display's bottom; left when the right has no room; nullopt when neither side fits any display; on a second display right of the first; on a display left of the first, not touching the main window, at that display's right edge. `FitBench` keeps a size that fits, shrinks a larger one in proportion to fit width and height.
- No UI tests exist: the control, the drag and the keys are checked by reading the code, by screenshots and by the user.
- **Screenshots** (release, viewed): the renderer's switch changes nothing seen: Tennis with the binder, Tennis with its TV's tone turned (`--tv bright:60%,contrast:90%`), the BW-80's first Puzzle (its TV's text, keyboard) and the 110 with every instrument, each compared with the same shot before the switch; `--pop manual` on Tennis (the binder beside the window, its control pointing in), `--pop scope,analyzer,meter` on the 110, `--pop tv,keyboard` on the Typewriter, `--pop knob` on Tennis; the binder's control on the bench; a bad `--pop` and `--pop keyboard` on the Counter give the usage error.
- **Release:** the 110 with every instrument, speed 1.00× and UI time as before the switch (about 1 ms), and the frame rate kept, with and without the instruments popped out. `--screenshot` prints the UI time as the frame's building (to `ImGui::Render`; the GPU's waits for each window's vsync would count otherwise) and the frames per second. The full release suite passes.

## Later

- A pop-out control on the other objects, if wanted (one generic button or one per object).
- Several objects popped out at once (`--pop`) placed apart: each goes beside the main window alone, so they overlap at the same edge.
- A popped-out window on a slower display: presented unpaced, the UI keeps 60–80 fps beside a 30 Hz monitor (Metal); more would need each window drawn on its own pace.
- Remembering popped-out windows between runs, with the rest of a session's layout, if P7 keeps one.
