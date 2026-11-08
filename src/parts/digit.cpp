#include "parts/digit.hpp"

#include <utility>
#include <vector>

namespace bw {

namespace {

std::vector<PinSpec> DigitPins() {
    std::vector<PinSpec> pins;
    for (const char* name : {"E", "D", "CA1", "C", "DP", "B", "A", "CA2", "F", "G"}) {
        pins.push_back({name, PinKind::Input, Family::Mos});
    }
    return pins;
}

}  // namespace

Digit::Digit(std::string reference) : Component(std::move(reference), DigitPins()) {}

void Digit::evaluate(EvalContext& /*ctx*/) {}

}  // namespace bw
