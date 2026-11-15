#pragma once

#include <imgui.h>

#include "session/session.hpp"
#include "ui/matrix_capture.hpp"

namespace bw {

// The BW-2001 keyboard (the first MATRIXKB) as a floating window of drawn chiclet keys, as the
// PET 2001's (P5q). Typing while it is focused (MatrixCapture), F2 toggles; a click holds a key
// while the mouse is down, a right-click latches it (SHIFT for a sequence) until clicked again.
class MatrixKeyboardWindow {
public:
    explicit MatrixKeyboardWindow(ImFont* legends) : legends_(legends) {}

    // `capture.on` follows the window's focus. Nothing drawn without a MATRIXKB.
    void draw(bool& open, Session& session, MatrixCapture& capture);

private:
    void drawKeys(Session& session, const MatrixCapture& capture);

    ImFont* legends_;
    std::vector<bool> latched_;  // per key: right-clicked down
    bool focusNext_ = false;
    float titleHeight_ = 0;
};

}  // namespace bw
