#include "sim/component.hpp"

#include <string>
#include <utility>

#include "sim/check.hpp"

namespace bw {

Component::Component(std::string name, std::vector<PinSpec> pins)
    : name_(std::move(name)), pins_(std::move(pins)) {}

PinIndex Component::pinIndex(std::string_view pinName) const {
    for (PinIndex i = 0; i < pins_.size(); ++i) {
        if (pins_[i].name == pinName) {
            return i;
        }
    }
    Check(false, name_ + ": unknown pin " + std::string(pinName));
    return 0;
}

void Component::declarePinGroup(std::string name, std::initializer_list<std::string_view> pins) {
    PinGroup group{std::move(name), {}};
    for (const std::string_view pin : pins) {
        group.pins.push_back(pinIndex(pin));
    }
    pinGroups_.push_back(std::move(group));
}

void Component::powerOn(PowerOnContext& /*ctx*/) {}

}  // namespace bw
