#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "sim/component.hpp"

namespace bw {

// Wire-to-board header; passive: Pins are loads (Input) or power, no behaviour.
class Connector final : public Component {
public:
    Connector(std::string reference, std::vector<PinSpec> pins);
    void evaluate(EvalContext& ctx) override;
};

std::vector<PinSpec> ConnectorPins(std::string_view part);  // "POWER", "VIDEO"; Check()s the name

}  // namespace bw
