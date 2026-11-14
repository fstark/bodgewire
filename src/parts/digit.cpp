#include "parts/digit.hpp"

#include <utility>
#include <vector>

namespace bw {

namespace {

std::vector<PinSpec> DigitPins(DigitCommon common) {
    const bool anode = common == DigitCommon::Anode;
    std::vector<PinSpec> pins;
    for (const char* name :
         {"E", "D", anode ? "CA1" : "CC1", "C", "DP", "B", "A", anode ? "CA2" : "CC2", "F", "G"}) {
        pins.push_back({name, PinKind::Input, Family::Mos});
    }
    return pins;
}

}  // namespace

Digit::Digit(std::string reference, DigitCommon common)
    : Component(std::move(reference), DigitPins(common)) {}

void Digit::evaluate(EvalContext& /*ctx*/) {}

}  // namespace bw
