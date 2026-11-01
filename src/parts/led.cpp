#include "parts/led.hpp"

#include <utility>

namespace bw {

Led::Led(std::string reference)
    : Component(std::move(reference),
                {{"K", PinKind::Input, Family::Mos}, {"A", PinKind::Input, Family::Mos}}) {}

void Led::evaluate(EvalContext& /*ctx*/) {}

}  // namespace bw
