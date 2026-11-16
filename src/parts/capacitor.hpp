#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// ps: the capacitor repeats an edge this long after it (no zero delays).
inline constexpr Time kCouplingDelay = 1;

// A capacitor coupling a driven Net to a pulled one (P5v; the builder adds it only there): each
// edge of the input is repeated on the output (0, 1 or X, Strong), which is released `width`
// later for its pull resistor to bring back; `width` is the pull's ohms times the capacitor's
// farads. Pins "1" and "2" as the Part's; `input` is the index of the driven end. The pulse's
// length does not depend on the edge's direction or on what else drives the output Net: a chip
// driving that Net fights it (X) for the pulse's length.
class Capacitor final : public Component {
public:
    Capacitor(std::string reference, PinIndex input, Time width);
    void powerOn(PowerOnContext& ctx) override;  // output released
    void evaluate(EvalContext& ctx) override;

    PinIndex input() const { return input_; }
    PinIndex output() const { return 1 - input_; }
    Time width() const { return width_; }

private:
    PinIndex input_;
    Time width_;
    Time releaseAt_ = 0;
};

}  // namespace bw
