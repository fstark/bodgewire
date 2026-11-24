#include "session/leads.hpp"

#include <format>

namespace bw {

std::optional<PadLead> PadLeadOf(const Machine& machine, std::string_view reference,
                                 std::string_view pin) {
    const auto& components = machine.designCircuit().components;
    const auto component = components.find(reference);
    if (component == components.end()) {
        const auto index = PassivePin(pin);
        const auto at = index ? machine.padPosition(reference, pin) : std::nullopt;
        if (!at) {
            return std::nullopt;
        }
        return PadLead{.reference = std::string(reference),
                       .pin = std::string(pin),
                       .label = std::format("{}.{}", reference, pin),
                       .at = *at,
                       .index = *index,
                       .chip = false};
    }
    const auto index = FindPin(*component->second, pin);
    if (!index) {
        return std::nullopt;
    }
    std::string number = std::to_string(*index + 1);
    const auto at = machine.padPosition(reference, number);
    if (!at) {
        return std::nullopt;
    }
    std::string label =
        std::format("{}.{} {}", reference, number, component->second->pins()[*index].name);
    return PadLead{.reference = std::string(reference),
                   .pin = std::move(number),
                   .label = std::move(label),
                   .at = *at,
                   .index = *index};
}

}  // namespace bw
