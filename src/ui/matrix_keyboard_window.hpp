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
// key while the mouse is down, a click on SHIFT holds it until the next key clicked is up (or SHIFT
// is clicked again), a right-click latches a key until right-clicked again.
class MatrixKeyboardWindow {
public:
    // `petLegends`: the BW-2001's square bold legends (Saira Expanded Bold, P6l2).
    MatrixKeyboardWindow(ImFont* legends, ImFont* petLegends)
        : legends_(legends), petLegends_(petLegends) {}

    // Nothing drawn without a BW2001KB.
    void draw(bool& open, Session& session, MatrixCapture& capture);

private:
    void drawKeys(Session& session, const MatrixCapture& capture);
    void drawMembrane(Session& session, const MatrixCapture& capture);
    // The cap loop both looks share: the buttons, the latches, the Session's held keys; each cap
    // drawn `gap` (u) smaller than its pitch, `height` (u) high. `shifts`: the SHIFT keys, the
    // first one drawn down for a PC key typed with SHIFT.
    template <class Draw>
    void drawCaps(Session& session, const MatrixCapture& capture, std::span<const MatrixCap> caps,
                  std::span<const std::size_t> shifts, ImVec2 keys, float u, ImVec2 gap,
                  float height, Draw&& draw);

    ImFont* legends_;
    ImFont* petLegends_;
    std::vector<bool> latched_;       // per key: right-clicked down
    std::vector<bool> shiftClicked_;  // per SHIFT key: clicked down, for the next key clicked
};

}  // namespace bw
