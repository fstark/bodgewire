#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <imgui.h>

#include "session/session.hpp"
#include "ui/matrix_capture.hpp"
#include "view/matrix_keyboard_layout.hpp"

namespace bw {

// A key matrix keyboard as a floating window of drawn keys: the BW-2001's chiclets, as the PET
// 2001's (P5q), or the BW-80's membrane, as the ZX80's (P5x), by the capture's kind. Typing while
// it is focused (MatrixCapture), F2 toggles; a click holds a key while the mouse is down, a
// right-click latches it (SHIFT for a sequence) until clicked again.
class MatrixKeyboardWindow {
public:
    explicit MatrixKeyboardWindow(ImFont* legends) : legends_(legends) {}

    // `capture.on` follows the window's focus. Nothing drawn without a MATRIXKB.
    void draw(bool& open, Session& session, MatrixCapture& capture);

private:
    void drawKeys(Session& session, const MatrixCapture& capture);
    void drawMembrane(Session& session, const MatrixCapture& capture);
    // The cap loop both looks share: the buttons, the latch, the Session's held keys.
    template <class Draw>
    void drawCaps(Session& session, const MatrixCapture& capture, std::span<const MatrixCap> caps,
                  std::size_t shift, ImVec2 keys, float u, Draw&& draw);

    ImFont* legends_;
    std::vector<bool> latched_;  // per key: right-clicked down
    bool focusNext_ = false;
    float titleHeight_ = 0;
    float aspect_ = 1;  // height over width of the keyboard's case, for the size constraint
};

}  // namespace bw
