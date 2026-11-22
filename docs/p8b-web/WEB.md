# P8b — Web port

Roadmap step P8b (ARCHITECTURE.md#roadmap): `bw_ui` built by Emscripten into a page that runs every Machine in the browser, published on GitHub Pages from `main` (https://fstark.github.io/bodgewire/). Done as a spike on the `web` branch, without a design or plan: this file says what was built.

## Why

A demo anyone can open, with nothing to install: the launcher, every Machine and every Puzzle.

## User decisions

The user, 2026-11-22:

- A web version first, run at whatever speed it gets (even 10 % of the desktop's): a demo for fast machines, no optimisation work.
- All the Machines in it.
- The pop-out windows may go.
- Worked on its own branch, the fixes that suit the desktop build too on `main`.
- The roadmap row when it merges (2026-11-23: P8b, P8 being the ports).

Chosen without asking:

- Emscripten 7.0.1.
- SDL_Renderer on WebGL. SDL GPU has no browser backend.
- Asyncify, so the frame loops keep their shape.
- pthreads for the Runner's threads, made possible on Pages by a vendored coi-serviceworker.
- The Machines and datasheets packed into the page.

## What the player sees

1. The launcher, filling the browser window and following its size.
2. Every Machine and Puzzle as on the desktop: Board, instruments, TV, keyboards, manuals.
3. No pop-out windows: everything stays in the page.
4. A command line from the address (`?arg=/machines/070-tennis/tennis.bwn`, one `arg` a word).
5. The first visit reloads itself once (the service worker installing).

## What is new

Only `src/ui/` and the build change; the simulator, Parts, Machines, Runner and view are compiled unchanged.

1. **`GpuRender` behind one interface.**
   - [gpu_render.hpp](../../src/ui/gpu_render.hpp) gains `initImGui`, `newFrame` and `shutdownImGui`, moved out of `main.cpp`.
   - [gpu_render.cpp](../../src/ui/gpu_render.cpp) is the desktop's (SDL GPU, multi-viewports); [gpu_render_web.cpp](../../src/ui/gpu_render_web.cpp) is the web's (SDL_Renderer). CMake picks one, with its ImGui backend.
   - The web's `present` awaits `requestAnimationFrame` (`EM_ASYNC_JS`): Asyncify unwinds the frame loop there and resumes it on the next frame.
   - `SDL_HINT_EMSCRIPTEN_ASYNCIFY` is `0`, so SDL never yields by itself, through indirect calls `ASYNCIFY_IGNORE_INDIRECT` leaves uninstrumented.
   - The web's `screenshot` fails.
2. **`TvView`** takes the `GpuRender`. Its texture is an `SDL_Texture` on the web and a GPU texture with a transfer buffer on the desktop, under `#ifdef __EMSCRIPTEN__`.
3. **The window** in [main.cpp](../../src/ui/main.cpp):
   - `SDL_WINDOW_FILL_DOCUMENT` on the web, so the canvas follows the page.
   - `SDL_WINDOW_OPENGL` from the start. Without it the GLES2 renderer recreates the window, and SDL's fill-document page juggling throws.
4. **The build:**
   - **The `web` preset** in [CMakePresets.json](../../CMakePresets.json): `-pthread -fwasm-exceptions`, no tests.
   - **The `if(EMSCRIPTEN)` block** in [src/ui/CMakeLists.txt](../../src/ui/CMakeLists.txt):
     - Asyncify, a pool of 12 threads, memory growth and an 8 MB stack;
     - `machines/` and `datasheets/` preloaded, without docs, history or KiCad project files;
     - the page from [web/shell.html](../../src/ui/web/shell.html);
     - [web/coi-serviceworker.js](../../src/ui/web/coi-serviceworker.js) copied next to it. It is v0.1.7, MIT. It adds COOP/COEP, which `SharedArrayBuffer` needs and Pages does not send.
5. **Publishing:** [.github/workflows/web.yml](../../.github/workflows/web.yml) builds on every push to `main` and deploys to Pages.
6. **On `main` first:** two warnings of newer clangs kept out of `-Werror`, `-Wmissing-designated-field-initializers` and `__COUNTER__`.

The rule this brings: Asyncify cannot unwind through a live `try` block under wasm exceptions (the link warns "not compatible"). No `try` encloses a frame loop today, and none may.

## Tests

- **No ctest:** the web build has none. The desktop's full release suite passes after the merge.
- **Chrome, through Playwright** (2026-11-22):
  - the launcher, the Counter, tennis with its TV;
  - the BW-2001 booting to BASIC's READY;
  - the canvas at the window's size, after a resize.
- **The user** (2026-11-22–23): Firefox, typing, an iPad (no keyboard or modifiers there), the canvas filling Firefox's window.
- **Speed:** wasm at about 0.42× native (0.31–0.51× across the Machines), before Asyncify's and WebGL's costs. Slow, but it runs.
- **Output:** `bw_ui.wasm` 3.8 MB, `bw_ui.data` 7.5 MB.

## Later

- **`TvView` with no backend in it.** `GpuRender` would own the TV texture and `present` upload `TvView`'s pixels. That ends the `#ifdef`s and the `device()` and `renderer()` getters.
- **Speed:**
  - JSPI in place of Asyncify, once Safari has it;
  - or the frame loops as `emscripten_set_main_loop` callbacks.
- **Touch:** an on-screen keyboard and modifiers on tablets.
- **Safari:** untested.
- **CI:** emsdk cached in the workflow.
