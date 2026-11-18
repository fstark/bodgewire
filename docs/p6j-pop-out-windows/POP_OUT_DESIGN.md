# P6j — Pop-out windows — Design

Spec: [POP_OUT.md](POP_OUT.md). Plan: [POP_OUT_PLAN.md](POP_OUT_PLAN.md). Recipes: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md) Window row and Gotchas (`PlaceNext`, `SendHome`), [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) UI layer (a case window: `PlaceNext`, `KeepAspect`, `CaseClose`). Every bench object already goes through `PlaceNext` before its `Begin` (`src/ui/bench.cpp`), so the viewport choice is made there once; no window's drawing changes except the binder's control.

## Files

| File | Change |
|---|---|
| `src/ui/CMakeLists.txt` | ImGui `v1.93.0-docking`; `imgui_impl_sdlgpu3.cpp` instead of `imgui_impl_sdlrenderer3.cpp`; `gpu_render.cpp` |
| `src/ui/gpu_render.{hpp,cpp}` (new) | the GPU device, a frame drawn and presented, the screenshot (below) |
| `src/ui/tv_view.{hpp,cpp}` | an `SDL_GPUTexture`, uploaded in a copy pass (below) |
| `src/view/bench_places.{hpp,cpp}`, `tests/view/bench_places_test.cpp` | `Area`, `BesidePlace`, `FitBench` (below) |
| `src/ui/bench.{hpp,cpp}` | popped-out state; `PlaceNext` picks the viewport; `PopOut`, `PopIn`, `OnBench` (below) |
| `src/ui/manual_window.cpp` | the control (below) |
| `src/ui/brief_window.cpp` | `OnBench()` before its anchored `SetNextWindowPos` |
| `src/ui/keyboard_capture.{hpp,cpp}`, `src/ui/matrix_capture.{hpp,cpp}` | `sync` follows the focused window (below) |
| `src/ui/main.cpp` | the renderer, the main window on the bench, `--pop`, `sync` with the focused window |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_INSTRUMENT.md`, `docs/recipes/ADD_CONNECTOR_DEVICE.md`, `docs/ARCHITECTURE.md` | below |

## The renderer (`src/ui/gpu_render.hpp`)

From `examples/example_sdl3_sdlgpu3/main.cpp` of the docking tag: the device for SPIR-V, DXIL, MSL and metallib shaders (Metal on macOS), the main window's swapchain taken with `SDL_WaitAndAcquireGPUSwapchainTexture` (it waits for the vsync). ImGui's backends own the secondary windows (`ImGui::UpdatePlatformWindows`, `RenderPlatformWindowsDefault`: their swapchains, present mode VSYNC, taken with the non-waiting `SDL_AcquireGPUSwapchainTexture`, so a window whose swapchain is not ready skips a frame); `GpuRender` owns the device and the main window's frame. All viewports share one vertex and index buffer pair in `imgui_impl_sdlgpu3` (`MainWindowFrameData`): `PrepareDrawData` before each `RenderDrawData`, always.

```cpp
// SDL GPU for every window bw_ui draws (P6j): the main one and the popped-out objects'.
class GpuRender {
public:
    explicit GpuRender(SDL_Window* main);  // the device claims `main`, VSYNC; error() if not
    ~GpuRender();                          // waits for the GPU, releases the device
    const char* error() const;             // null when ready
    SDL_GPUDevice* device() const;
    SDL_GPUTextureFormat format() const;   // the main swapchain's, ImGui's ColorTargetFormat
    // After ImGui::Render: one command buffer, the TV's copy pass (tv.upload, also when the main
    // window is minimized: a popped-out TV still shows it), then, if the main swapchain gave a
    // texture and DisplaySize is not 0, ImGui's buffers (PrepareDrawData) and the main window
    // cleared black and drawn; submitted; then the other windows (UpdatePlatformWindows,
    // RenderPlatformWindowsDefault). Waits for the windows' vsync.
    void present(TvView& tv);
    // After present (so this frame's TV upload is in, and UpdatePlatformWindows, which may run
    // once a frame, has made the windows): per visible viewport, in its own command buffer,
    // PrepareDrawData, a render pass into an offscreen COLOR_TARGET texture of DisplaySize ×
    // FramebufferScale (the swapchain's format, so ImGui's pipeline fits), a download, a fence
    // waited for. Composed: with only the main viewport, its framebuffer as is; else the desktop
    // rectangle around every viewport at the main viewport's FramebufferScale on a (40, 40, 40)
    // backdrop, each scaled to that scale (SDL_BlitSurfaceScaled), back to front: the main
    // window first, then the others by LastFocusedStampCount (ImGui's fallback z-order,
    // IsViewportAbove), stable. SDL_WaitForGPUIdle first and after. False with SDL's error.
    bool screenshot(const std::filesystem::path& path);
};
```

`main()`: `ImGui_ImplSDL3_InitForSDLGPU(window)`, `ImGui_ImplSDLGPU3_Init` with `{device, format, SAMPLECOUNT_1}`; `io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable` (no `DockingEnable`); `ConfigViewportsNoDefaultParent` (true), `ConfigViewportsNoDecoration` (true, no title bar) and `ConfigViewportsNoTaskBarIcon` (false) stay ImGui's defaults. The frame: `ImGui_ImplSDLGPU3_NewFrame` for `ImGui_ImplSDLRenderer3_NewFrame`; the UI time taken after `ImGui::Render` (before `present`, whose vsync waits it would count), a frame counted; then `present`; then the screenshot (when due), which prints the frames per second beside the UI time. Shutdown: `RunWindow` returns (its `TvView` releases its texture and buffer), `SDL_WaitForGPUIdle`, `ImGui_ImplSDL3_Shutdown`, `ImGui_ImplSDLGPU3_Shutdown` (the platform windows destroyed), `DestroyContext`, then `GpuRender` (the main window released from the device, the device destroyed), then `SDL_DestroyWindow`, as the example.

Tooltips: `KeepFocusFromTooltips()` after the SDL3 backend's init wraps `platform_io.Platform_ShowWindow`: a viewport with `TopMost | NoFocusOnAppearing` (ImGui's tooltips, `WindowSyncOwnedViewport`) is shown with `SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN` "0", every other through the backend (on macOS it sets "1" for all: an unactivated popup would appear under the main window). The download's pixel format: `B8G8R8A8_UNORM` → `SDL_PIXELFORMAT_BGRA32`, `R8G8B8A8_UNORM` → `RGBA32`, any other refused. `SDL_SetRenderVSync` goes (the swapchain's present mode).

## The TV picture (`src/ui/tv_view.hpp`)

`TvView(SDL_GPUDevice*)`: an `R8G8B8A8_UNORM` `SAMPLER` texture `kTvColumns × kTvRows` and an upload transfer buffer of its bytes. `show` and `setTone` fill `pixels_` and set `dirty_` (no GPU call); `upload(SDL_GPUCommandBuffer*)` when dirty maps the transfer buffer (`cycle = true`), copies, and records `SDL_UploadToGPUTexture` (`cycle = true`: earlier frames in flight still sample the texture) in its own copy pass, before ImGui's. `texture()` is the `SDL_GPUTexture*` as `ImTextureID` (`imgui_impl_sdlgpu3`'s identifier since 2025-08-08). ImGui's default sampler is linear, as the SDL texture's scale mode was.

## Bench places (`src/view/bench_places.hpp`)

ImGui-free, tested with the rest of the file.

```cpp
// A rectangle in desktop logical px (ImGui's coordinates with viewports on), top left.
struct Area {
    float x = 0, y = 0, width = 0, height = 0;
};
// Where an object popped out of the main window goes (P6j): right of `main` with kBenchMargin
// between, else left of it, on the first display (each its work area) that holds the whole
// object there; its top where it was, moved to fit that display's height. nullopt when neither
// side fits any display (it stays where it was).
std::optional<Area> BesidePlace(Area object, Area main, std::span<const Area> displays);
// A size back on the bench: as is when it fits `bench`, else shrunk in proportion to fit.
Extent FitBench(Extent size, Extent bench);
```

## The bench (`src/ui/bench.hpp`)

```cpp
// P6j: true while `object` is in its own desktop window, for the run.
bool PoppedOut(BenchObject object);
// From inside its window: to its own desktop window, BesidePlace'd from the window's rect, the
// main viewport's and platform_io.Monitors' work areas (else where it is), from the next frame.
void PopOut(BenchObject object);
// --pop: the same before the object first comes out, BesidePlace'd from its default place and
// first size (both known to its first PlaceNext).
void PopOutAtStart(BenchObject object);
// Back on the bench at its default place, FitBench'd to the bench, from the next frame.
void PopIn(BenchObject object);
// Before Begin of a window that never leaves the bench (the main one, the brief).
void OnBench();
```

`PlaceNext`, before what it does today:

- not popped out: `OnBench()`, i.e. `SetNextWindowViewport(GetMainViewport()->ID)`: locked to the main window, clipped at its edge when dragged past it;
- popped out: `SetNextWindowClass` with `ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge` (ImGui's `GetWindowAlwaysWantOwnViewport`: its own viewport even over the main window); a pending `BesidePlace` position `SetNextWindowPos(…, ImGuiCond_Always)` once.

The default-place branch is unchanged, so a popped-in object goes home through `home_` (as `SendHome`) with a pending `FitBench` size `SetNextWindowSize(…, ImGuiCond_Always)` once. `SendHome` of a popped-out object pops it in. `BenchArea` keeps the main viewport's `WorkPos`: with viewports on, desktop coordinates, so the default places follow the main window wherever it is (ImGui moves the main viewport's windows with it).

`PopOut` and `PopIn` record a request applied by the next `PlaceNext` (the window is between `Begin` and `End`). The popped-out flags live beside `home_`, indexed by `BenchObject`.

## The binder's control (`src/ui/manual_window.cpp`)

After the dividers' tabs, before `CaseClose`: `PopControl(at, w, h)`.

- 28 px square (`kPopSize`), centred on the tab column (`pageRight + kTabSetBack` to `at.x + w - kCoverEdge - 2`), its bottom at `at.y + h - 28` (`kPopLift`): above the resize grip.
- An `InvisibleButton`; a click calls `PopOut(BenchObject::Manual)` or `PopIn`.
- Drawn in the paper's cream (`kPaper`), alpha 150 faint, 255 hovered: a rounded square outline (the lower-left two-thirds of it, open at the top right) and an arrow on the diagonal, its head at the top right (out) or at the square's centre (in). Tooltip `Own window` / `Back on the bench`.

## Keys (`src/ui/keyboard_capture.hpp`, `matrix_capture.hpp`)

SDL3's text input is per window, and only the focused one gets `SDL_EVENT_TEXT_INPUT`. `sync(SDL_Window* focused)`: `main.cpp` passes `SDL_GetKeyboardFocus()`, else the main window. While `on`, text input is started on `focused`, and stopped on the window it was started on when that changes (`started_` becomes that `SDL_Window*`); off, stopped there. Key events need nothing: every window's arrive through `SDL_PollEvent`, and the captures do not look at `windowID`. The quit test keeps the main window's ID (a popped-out window has no close button).

## `main.cpp`

- `RunWindow(SDL_Window*, GpuRender&, …)`; `TvView view(gpu.device())`.
- The main window `"Bodgewire"`: `OnBench()` before its `SetNextWindowPos`.
- `--pop LIST`: `ParseBenchObject` (`manual`, `tv`, `keyboard`, `knob`, `analyzer`, `scope`, `meter`); in `main()` refused for an object the Machine lacks (`bw_ui: this Machine has no {}`, usage, exit 2: TV `session.tv()`, keyboard either kind, knob `session.knobs()`, instruments `session.machine()`); in `RunWindow` each brought out (`shelf.manualOut`, `tvOut`, `keyboardOut`, `knobOut`, `focus.bringOut`) and `PopOutAtStart` before the first frame.
- The usage line gains `[--pop manual|tv|keyboard|knob|analyzer|scope|meter,...]`.

## Docs

- GLOSSARY, **Equipment**: an object can leave for its own desktop window, beside the main one, and come back to its default place; the binder by the control at its bottom right; the others with `--pop` (P6j).
- ADD_INSTRUMENT Window row and Gotchas, ADD_CONNECTOR_DEVICE UI layer: `PlaceNext` also locks a window to the bench or gives it its own desktop window; a new window must go through it (or `OnBench`), or a drag past the main window's edge makes it a desktop window.
- ARCHITECTURE: P6j done with the date and links; the UI's renderer named SDL GPU wherever the SDL renderer is.
