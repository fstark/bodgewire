#include "parts/membrane_keyboard.hpp"

#include <utility>

#include "parts/connector.hpp"
#include "sim/simulator.hpp"

namespace bw {

MembraneKeyboard::MembraneKeyboard(std::string reference)
    : Component(std::move(reference), ConnectorPins("MEMBRANEKB")) {}

bool MembraneKeyboard::type(std::u32string_view text) {
    bool any = false;
    for (const MatrixPress& press : Zx80Presses(text)) {
        any = queue_.push(press) || any;
    }
    return any;
}

void MembraneKeyboard::powerOn(PowerOnContext& ctx) {
    queue_.reset();
    for (int c = 0; c < kColumns; ++c) {
        ctx.setOutput(kColumn0 + c, LogicValue::Z);
    }
}

void MembraneKeyboard::evaluate(EvalContext& ctx) {
    if (const auto wake = queue_.step(ctx.now())) {
        ctx.wakeAfter(*wake);
    }
    const auto current = queue_.current();
    const std::size_t shift = shiftKey();
    bool low[kColumns]{};
    const auto keys = Zx80Keys();
    for (std::size_t k = 0; k < keys.size(); ++k) {
        const bool down =
            held(k) || (current && (k == current->key || (current->shift && k == shift)));
        // The diode conducts only into a row held low; a floating or X line does nothing.
        if (down && ctx.read(kRow0 + keys[k].row) == LogicValue::Zero) {
            low[keys[k].column] = true;
        }
    }
    for (int c = 0; c < kColumns; ++c) {
        ctx.drive(kColumn0 + c, low[c] ? LogicValue::Zero : LogicValue::Z, kMembraneDelay);
    }
}

}  // namespace bw
