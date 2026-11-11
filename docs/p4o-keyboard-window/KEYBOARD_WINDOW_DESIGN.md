# P4o Keyboard window — Detailed Design

Implements [KEYBOARD_WINDOW.md](KEYBOARD_WINDOW.md): control codes through the Session, the keyboard layout, the keyboard window and its typing by focus. No new Part, no simulator change.

Recipes: [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md), rows "Session" and "UI" and the capture bullet. Those rows are what changes here. This design gives the rest.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Files

```
src/view/keyboard_layout.hpp, .cpp      KeyCap table, codes, CapFor (no ImGui, no SDL)
src/ui/keyboard_window.hpp, .cpp        KeyboardWindow: drawing, mouse, flashes, focus
src/ui/keycap_font.cpp.in               template for the embedded Roboto Medium bytes
tests/view/keyboard_layout_test.cpp
```

Changed: `src/session/session.{hpp,cpp}`, `src/ui/keyboard_capture.{hpp,cpp}`, `src/ui/side_panel.{hpp,cpp}`, `src/ui/toolbar.cpp`, `src/ui/main.cpp`, `src/ui/CMakeLists.txt`, `src/view/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/session/session_test.cpp`, `datasheets/KEYBOARD.md`, docs (§7).

## 2. Codes through the Session

[session.hpp](../../src/session/session.hpp), beside `type`:

```cpp
// Sends a 7-bit `code` the keyboard can send (0x00-0x5F: control codes from CTRL, ESC,
// RETURN, BACK SPACE, and the upper-case set) to KEYBOARD `reference`. False as type().
bool typeCode(std::string_view reference, std::uint8_t code);
// As typeCode, the character through KeyboardCode (text, --type).
bool type(std::string_view reference, char32_t key);
inline constexpr std::uint8_t kKeyboardLastCode = 0x5F;  // in keyboard.hpp, beside the others
```

`type` becomes `const auto code = KeyboardCode(key); return code && typeCode(reference, *code);`. `typeCode` is today's body of `type` with the test `code > kKeyboardLastCode → false` replacing the `KeyboardCode` test. `typed_` keeps raw codes, as now. `KeyboardCode` and `ParseTyping` do not change. `bw_ui`'s `--type` loop ([main.cpp](../../src/ui/main.cpp), "typedKeys") calls `typeCode` with the codes it already holds.

`Keyboard::type(std::uint8_t)` already takes any byte: no Part change.

[KEYBOARD.md](../../datasheets/KEYBOARD.md) Function, second paragraph: upper case only, as now; Return sends 13, Back space 8, Esc 27; CTRL with a key of codes 64–95 (`@`, the letters, `[ \ ] ^ _`) sends the code minus 64 (0–31), CTRL with any other key sends the key's own code. `manual_test` covers its pins only: unchanged.

## 3. Keyboard layout (`src/view/keyboard_layout`)

```cpp
enum class CapKind : std::uint8_t { Character, Shift, Ctrl, Special };  // Special: sends one code

struct KeyCap {
    float x, y, width;          // key-widths (u); y is the row (0-4); height 1u (space bar too)
    std::string_view label;     // the plain legend ("1", "Q", "RETURN"), drawn as is
    std::string_view shifted;   // the shifted legend ("!", "@"), "" when none
    CapKind kind;
    std::uint8_t code;          // Character, Special: the plain code; Shift, Ctrl: 0
};

inline constexpr float kKeyboardWidth = 14.0f;  // u
inline constexpr int kKeyboardRows = 5;

std::span<const KeyCap> KeyCaps();  // in reading order

// What `cap` sends with SHIFT and CTRL as given; nullopt for SHIFT, CTRL, and `0` shifted.
std::optional<std::uint8_t> CapCode(const KeyCap& cap, bool shift, bool ctrl);

struct CapPress {
    std::size_t cap;   // index in KeyCaps()
    bool shift, ctrl;  // modifiers it needs
};
// The cap (and modifiers) that types `code` (0x00-0x5F); nullopt above that. For a letter,
// the plain cap; for a control code with two ways (0x0D is RETURN and CTRL+M), the
// dedicated key; for 0x00-0x1F otherwise, CTRL with the cap of code + 0x40.
std::optional<CapPress> CapFor(std::uint8_t code);
```

**Shift** (`CapCode`): for a Character cap with a non-empty `shifted`, the code XOR 0x10 (bit-paired: `1` 0x31 → `!` 0x21, `K` 0x4B → `[` 0x5B, `P` 0x50 → `@` 0x40). For a letter without `shifted` and for Space, the plain code. For `0` (shifted empty, not a letter), nullopt. Special caps ignore SHIFT. **Ctrl**: if the (shifted or plain) code is 0x40–0x5F, `code & 0x1F`; otherwise unchanged. CTRL with a Special cap: unchanged.

**The table** (`x` from the left, rows top to bottom; a cap 1u wide unless given):

| Row | Caps |
|---|---|
| 0 | from x 1.0: `1 !` `2 "` `3 #` `4 $` `5 %` `6 &` `7 '` `8 (` `9 )` `0` `: *` `- =` (ends 13.0) |
| 1 | `ESC` 1.5 (0x1B); from 1.5: `Q W E R T Y U I` `O _` `P @`; `BACK SPACE` 2.5 at 11.5 (0x08) |
| 2 | `CTRL` 1.75; from 1.75: `A S D F G H J` `K [` `L \` `; +`; `RETURN` 2.25 at 11.75 (0x0D) |
| 3 | `SHIFT` 2.25; from 2.25: `Z X C V B` `N ^` `M ]` `, <` `. >` `/ ?`; `SHIFT` 1.75 at 12.25 |
| 4 | Space bar 8.0 at 3.0 (0x20, no legend) |

Rows 1–3 end at 14.0, row 0 is centred with 1u left free at each end, as on the Teletype layouts. The positions are the starting point. Phase 3 may adjust widths once seen, keeping the tests' rules (no overlap, within 14u).

**`CapFor`**: built once (a `std::array<std::optional<CapPress>, 0x60>` filled by trying every cap with each of the four modifier combinations, preferring no modifier, then shift alone, then CTRL alone, then both). A Special cap wins over CTRL for its code because no-modifier comes first. Space (0x20) → the bar.

`keyboard_layout.cpp` in `src/view/CMakeLists.txt` (sorted). `bw_view` already links what it needs (nothing new).

## 4. Keyboard window (`src/ui/keyboard_window`)

```cpp
// The Machine's ASCII keyboard as a floating window of drawn caps. Typing while it is focused
// (KeyboardCapture); clicks type caps; --type keys flash.
class KeyboardWindow {
public:
    explicit KeyboardWindow(ImFont* legends);  // the embedded Roboto Medium (§5)

    // `capture.on` follows the window's focus; F2 handled here when not typing.
    void draw(bool& open, Session& session, KeyboardCapture& capture);
    // A code sent other than by this window (--type): its cap and modifiers down 150 ms
    // (until the next flash).
    void flash(std::uint8_t code);

private:
    ImFont* legends_;
    bool shiftLatched_ = false, ctrlLatched_ = false;   // mouse: until the next cap
    std::optional<CapPress> flash_;
    std::chrono::steady_clock::time_point flashUntil_;
    bool focusNext_ = false;                             // F2 while not typing
};
```

### 4.1 Window, focus, F2

- `capture.reference` empty: nothing drawn, F2 ignored.
- First, before any early return: F2 when not typing, as P4n's side panel did (`!io.WantTextInput && IsKeyPressed(ImGuiKey_F2, false)`): `open = true`, `focusNext_ = true`. F2 while typing never reaches ImGui (capture takes it): `KeyboardCapture::handle` sets `capture.stop = true` (§6), handled after `Begin` below.
- Whenever `draw` returns without drawing the caps (`open` false, `Begin` false): `capture.on = false`, `capture.stop = false`.
- `SetNextWindowSize({kInitialWidth, kInitialWidth * aspect + ImGui::GetFrameHeight()}, FirstUseEver)` with `kInitialWidth = 720`, the aspect from the case (§4.2) and the title bar's height added. `SetNextWindowPos` at the bottom centre of the Board area (`WorkPos.x + WorkSize.x * kBoardShare / 2 - width / 2`, `WorkPos.y + WorkSize.y - height - 16`), FirstUseEver. `kBoardShare` moves from main.cpp to `toolbar.hpp` beside `ToolButton` so both can use it. `SetNextWindowSizeConstraints` keeps the case's aspect: a callback setting height = width × aspect + title bar height (passed through `UserData`), min width 360. `focusNext_` → `SetNextWindowFocus()`.
- `Begin("Keyboard", &open, NoSavedSettings | NoScrollbar | NoScrollWithMouse | NoCollapse | NoFocusOnAppearing)`. `NoFocusOnAppearing` matters: ImGui focuses every new window when it first appears, which would start typing at launch and take every shortcut. `NoCollapse`: a collapsed window would hide the caps while keeping focus. Focus then comes only from a click or `focusNext_` → `SetNextWindowFocus()`, which works with that flag.
- After `Begin`: `capture.stop` → `SetWindowFocus(nullptr)`, `capture.on = false`, `capture.stop = false`. Otherwise `capture.on = IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)`. A click anywhere else takes focus (ImGui), so typing stops on the next frame. Clicking a cap focuses the window (`ButtonBehavior` focuses on press).
- `WindowPadding` pushed to 0 and `WindowBg` to `kSurround` (darker than the case), so the rounded case drawn inside shows its corners and edge.

### 4.2 Geometry and drawing

`u = min(available width / (kKeyboardWidth + 2 × kCaseMargin), available height / (kKeyboardRows + 2 × kCaseMargin + 0.45))`, so a rounding mismatch never clips, with `kCaseMargin = 0.5u` on every side and a label strip of `0.45u` above row 0. Client height `= u × (kKeyboardRows + 2 × kCaseMargin + 0.45)`. The aspect for §4.1 comes from these. A cap at (x, y, w) occupies `[origin + (x + kGap/2) u, origin + (y + kGap/2) u]` of size `(w − kGap) u × (1 − kGap) u`, `kGap = 0.08`.

Colours (ImU32 constants at the top of the file):

| Name | RGB | Use |
|---|---|---|
| `kSurround` | 8 8 9 | window background around the case |
| `kCase` | 18 18 19 | case fill |
| `kCaseEdge` | 46 46 48 | thin top-edge highlight (1 px line along the top inside the corner radius) |
| `kTypingEdge` | 255 176 40 | 2 px case outline while typing |
| `kEngraving` | 95 95 98 | `ASCII KEYBOARD · J1` |
| `kSkirt` / `kSkirtSpecial` | 22 22 24 / 52 53 56 | cap body |
| `kFaceTop`, `kFaceBottom` | 58 58 62 → 40 40 43 | cap face gradient (Character) |
| `kSpecialTop`, `kSpecialBottom` | 92 93 97 → 70 71 75 | Shift, Ctrl, Special faces |
| `kHighlight` | 255 255 255, alpha 28 | 1 px line along the top of the face |
| `kShadow` | 0 0 0, alpha 90 | under each raised cap |
| `kLegend` | 240 236 226 | legends |

Drawing one cap, rect `r`, `down` whether pressed:

1. Not down: shadow, three rounded rects offset `0.04u` down growing by 1 px each, alpha `kShadow / 3`.
2. `down`: `r` moved down `0.10 × height`.
3. Skirt: `AddRectFilled(r, skirt, rounding 0.12u)`.
4. Face: `r` inset `0.10u` left/right, `0.05u` top, `0.17u` bottom (seen slightly from the front), rounding `0.10u`. Filled `AddRectFilled` then `ImGui::ShadeVertsLinearColorGradientKeepAlpha` (imgui_internal.h) over the vertices just added, top colour to bottom colour, so the gradient keeps the rounded corners. Down: both colours scaled by 0.85.
5. Highlight: `AddLine` along the face's top, inset by the rounding.
6. Legends, in `legends_` pushed at a size from `u` rounded to whole pixels (`std::round`): ImGui 1.93 bakes glyphs per size, and a continuous size while resizing would bake a new one every frame:
   - Character with `shifted`, not a letter (`1 !`): two lines centred, shifted above, each `0.30u`.
   - Letter with `shifted` (`K [`): the letter `0.38u` centred low, the symbol `0.24u` centred above.
   - Plain letter or `0`: `0.38u` centred.
   - Shift, Ctrl, Special: label `0.20u`, left-aligned `0.12u` from the face's left, at the face's bottom less `0.08u`; `BACK SPACE` wraps to two lines (`BACK` / `SPACE`).
   - Space bar: none.

The label strip: `ASCII KEYBOARD · <reference>` at `0.22u`, `kEngraving`, left-aligned with row 1's left edge. The case: `AddRectFilled` the client rect with `kCase`, rounding `0.25u`, the `kCaseEdge` line, and the `kTypingEdge` outline while `capture.on`.

### 4.3 Pressed state

A cap is drawn down when any of these holds:

- its `ImGui::InvisibleButton` (one per cap, `PushID(index)`, over the cap rect) is active (the mouse holds it);
- it is in `capture.held()` (§6): a PC key held typed it;
- it is the `flash_` cap and `now < flashUntil_`;
- for a SHIFT cap: `shiftLatched_`, or the PC Shift is held (`capture.shiftHeld()`), or a held or flashed cap needed shift (`CapPress::shift`); likewise CTRL with `ctrlLatched_`, `capture.ctrlHeld()`, `CapPress::ctrl`. Both SHIFT caps go down together.

Mouse: `IsItemActivated()` on a Character or Special cap → `CapCode(cap, shiftLatched_, ctrlLatched_)`, if any → `session.typeCode(capture.reference, code)` (refusals ignored, as for keys). The latches clear when that cap is released (`IsItemDeactivated`), so SHIFT stays down with the key it modified. On SHIFT or CTRL → toggle its latch. The click also focuses the window (ImGui), so typing starts. That is wanted: the mouse is on the keyboard.

`flash(code)`: `flash_ = CapFor(code)`, `flashUntil_ = now + 150 ms`.

## 5. Legend font

`src/ui/CMakeLists.txt`: at configure time, `file(READ "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" hex HEX)`, `string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")`, `configure_file(keycap_font.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/keycap_font.cpp @ONLY)`, added to `bw_ui`'s sources. The template:

```cpp
// Generated from keycap_font.cpp.in: Roboto Medium (Apache License 2.0), from Dear ImGui's misc/fonts.
#include <cstddef>
namespace bw {
extern const unsigned char kKeycapFont[] = {@bytes@};
extern const std::size_t kKeycapFontSize = sizeof(kKeycapFont);
}  // namespace bw
```

Declared in `keyboard_window.hpp`. In main.cpp after `ImGui::CreateContext()`, the default font is added first so it stays the UI font. It is chosen as ImGui's lazy `AddFontDefault` in `NewFrame` would choose it (13 px × framebuffer scale ≥ 15 → vector, else bitmap). Called before the first frame, `AddFontDefault` itself would assume Retina on macOS. So: `SDL_GetWindowPixelDensity(window) * 13.0f >= 15.0f ? AddFontDefaultVector() : AddFontDefaultBitmap()`. Then `ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;` and `AddFontFromMemoryTTF(const_cast<unsigned char*>(kKeycapFont), static_cast<int>(kKeycapFontSize), 0.0f, &cfg)`. The `const_cast` is safe: ImGui does not write to the data when it does not own it. The `ImFont*` goes to `RunWindow`, then to the `KeyboardWindow` constructor. ImGui 1.93 fonts are sized at draw time (`PushFont(font, size)`), so one font serves every cap size.

## 6. Keyboard capture

[keyboard_capture.hpp](../../src/ui/keyboard_capture.hpp):

```cpp
// Typing: while on (the keyboard window focused), the keyboard and text events of SDL go to one
// KEYBOARD Part instead of ImGui, so no tool or knob key acts. F2 asks to stop (stop).
struct KeyboardCapture {
    bool on = false;
    bool stop = false;      // F2 pressed while typing; the window clears it
    std::string reference;

    bool handle(const SDL_Event& event, Session& session);
    void sync(SDL_Window* window);

    // The caps of the PC keys held now (each with the modifiers it needed); PC Shift and Ctrl.
    std::vector<CapPress> held() const;
    bool shiftHeld() const;  // SDL_GetModState(): also a modifier pressed before typing began
    bool ctrlHeld() const;

private:
    bool started_ = false;
    std::optional<SDL_Scancode> lastDown_;               // the key whose text is due
    std::map<SDL_Scancode, CapPress> held_;               // PC key → the cap it typed
    void send(Session& session, SDL_Scancode scancode, std::uint8_t code);
};
```

`send`: `session.typeCode(reference, code)` (refusals ignored); `if (auto press = CapFor(code)) held_[scancode] = *press`.

`handle`:

| Event | While on | Taken |
|---|---|---|
| `KEY_DOWN` `SDLK_F2` | `stop = true` | yes |
| `KEY_DOWN` Shift / Ctrl (L or R) | none (read through `SDL_GetModState`) | yes |
| `KEY_DOWN` `SDLK_RETURN`, `SDLK_KP_ENTER` | `send(…, 0x0D)` | yes |
| `KEY_DOWN` `SDLK_BACKSPACE` | `send(…, 0x08)` | yes |
| `KEY_DOWN` `SDLK_ESCAPE` | `send(…, 0x1B)` | yes |
| `KEY_DOWN` with `SDL_KMOD_CTRL` | `k = SDL_GetKeyFromScancode(scancode, event.key.mod & SDL_KMOD_SHIFT, false)` (`event.key.key` is the unshifted key: Ctrl+Shift+2 must give `@` on a US layout); if `k < 0x80`, upper-cased: 0x40–0x5F → `send(…, k & 0x1F)`, 0x20–0x3F → `send(…, k)` (CTRL changes nothing there), else nothing. SDL sends no text for control characters, so nothing types twice | yes |
| `KEY_DOWN` other | `lastDown_ = scancode` | yes |
| `TEXT_INPUT` | each byte 0x20–0x7E → `KeyboardCode` → `send(…, lastDown_ or SDL_SCANCODE_UNKNOWN, code)` | yes |
| `TEXT_EDITING` | none | yes |
| `KEY_UP` | (also while off) `held_.erase(scancode)` | no (passed on) |

Key repeats send again, as in P4n. When `on` turns false (checked in `sync`), `held_` clears, because its key-ups may go elsewhere. `shiftHeld`/`ctrlHeld` answer false while not on. `held()` returns the values of `held_`.

The `SDL_SCANCODE_UNKNOWN` fallback (text with no key-down seen, e.g. an input method) flashes nothing: `send` skips the `held_` entry for it.

## 7. Side panel, toolbar, main

- [side_panel.cpp](../../src/ui/side_panel.cpp): `DrawKeyboard` removed; `SidePanel::draw` loses the `KeyboardCapture&` parameter, and [side_panel.hpp](../../src/ui/side_panel.hpp) its include and the "Keyboard" in its comment. `Session::type`'s comment names `typeCode`. In the Debug section: `Keys sent (J1): …`, with `SentText` extended: 0x0D `[CR]`, 0x08 `[BS]`, 0x1B `[ESC]`, other codes below 0x20 `^` + (code + 0x40). It is drawn only when the Session has a keyboard (`session.keyboards()` not empty; the first).
- [toolbar.cpp](../../src/ui/toolbar.cpp): banner `Typing on %s · F2 to stop`.
- [main.cpp](../../src/ui/main.cpp): `KeyboardWindow keyboardWindow(legendFont)`, `bool keyboardOpen = !capture.reference.empty()`; `keyboardWindow.draw(keyboardOpen, session, capture)` after the other floating windows and **outside** their `if (session.machine())` block (the bench has no Board, so no Machine); `capture.sync(window)` moves after it, and `toolbar.typing` is set there. The `--type` loop calls `session.typeCode` and `keyboardWindow.flash(code)` on success.

## 8. Docs

- [GLOSSARY.md](../GLOSSARY.md) **Typing**: the `bw_ui` mode in which every key the player presses goes to the Machine's keyboard: on while the keyboard window has focus, F2 toggles it.
- [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md): row UI: a device with many controls gets its own floating window (`keyboard_window.cpp`) instead of a side-panel section; the capture bullet: on by window focus, F2 toggles, the keys held mapped to what they typed (not where they sit), key-ups always seen; reference commits for P4o.
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P4o row (done, links); P4n's row says "Typing (F2, Esc)": history, left as is.
- [KEYBOARD.md](../../datasheets/KEYBOARD.md) (§2).

## 9. Testing

| File | Checks |
|---|---|
| `tests/view/keyboard_layout_test.cpp` `[view]` | for each code 0x20–0x5F, exactly one Character cap types it with SHIFT off or on (letters count once, by the plain cap); `0` shifted → nullopt; digits and punctuation with `shifted` give code ^ 0x10, `A` shifted → 0x41, `K` shifted → 0x5B, `P` shifted → 0x40; RETURN, BACK SPACE, ESC → 0x0D, 0x08, 0x1B, also with SHIFT; CTRL+`A` 0x01, CTRL+SHIFT+`P` 0x00, CTRL+SHIFT+`K` 0x1B, CTRL+`1` 0x31; the union of every cap × four modifier pairs is exactly 0x00–0x5F; `CapFor` for every code in 0x00–0x5F gives a cap whose `CapCode` with its modifiers is that code, `CapFor(0x0D)` RETURN without CTRL, `CapFor('!')` the `1` cap with shift, `CapFor(0x60)` nullopt; no two caps overlap; each cap within `0..kKeyboardWidth` and rows `0..kKeyboardRows`; two SHIFT caps, one CTRL |
| `tests/session/session_test.cpp` `[session]` | `session: typing control codes`: on the bench, `typeCode` true for 0x00, 0x01, 0x1B, 0x5F, false for 0x60, 0x7F and with the power off; `typed` holds them; after `waitIdle`, one of the 16 RAM words holds 0x07 typed between `A` and `B` (searched as in `session: typing`, since U4 starts at a random count) |

The window itself is checked by screenshots (release, plan phase 4) and by the user.
