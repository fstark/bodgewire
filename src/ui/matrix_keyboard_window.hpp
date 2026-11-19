#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <imgui.h>

#include "session/session.hpp"
#include "ui/matrix_capture.hpp"
#include "view/matrix_keyboard_layout.hpp"

namespace bw {

// Embedded by src/ui/CMakeLists.txt (pet_font.cpp.in).
extern const unsigned char kPetFont[];
extern const std::size_t kPetFontSize;

// The matrix keyboard window's ID (its title shows the reference), for the shelf to bring it to the
// front.
inline constexpr const char* kMatrixKeyboardWindowName = "###matrix keyboard";

// A key matrix keyboard as a floating window of drawn keys: the BW-2001's point-of-sale keys, as
// the PET 2001's (P5q, P6l2), or the BW-80's membrane in its cream case, as the ZX80's (P5x, P6l),
// by the capture's kind. Typed on from the PC keys (MatrixCapture); a click holds a
// key while the mouse is down, a right-click latches it (SHIFT for a sequence) until clicked again.
class MatrixKeyboardWindow {
public:
    // `petLegends`: the BW-2001's square bold legends (Saira Expanded Bold, P6l2).
    MatrixKeyboardWindow(ImFont* legends, ImFont* petLegends)
        : legends_(legends), petLegends_(petLegends) {}

    // Nothing drawn without a MATRIXKB.
    void draw(bool& open, Session& session, MatrixCapture& capture);

private:
    void drawKeys(Session& session, const MatrixCapture& capture);
    void drawMembrane(Session& session, const MatrixCapture& capture);
    // The cap loop both looks share: the buttons, the latch, the Session's held keys; each cap
    // drawn `gap` (u) smaller than its pitch, `height` (u) high.
    template <class Draw>
    void drawCaps(Session& session, const MatrixCapture& capture, std::span<const MatrixCap> caps,
                  std::size_t shift, ImVec2 keys, float u, ImVec2 gap, float height, Draw&& draw);

    ImFont* legends_;
    ImFont* petLegends_;
    std::vector<bool> latched_;  // per key: right-clicked down
};

}  // namespace bw
