#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sim/component.hpp"

namespace bw {

// Wire-to-board header; passive: Pins are loads (Input) or power, no behaviour. The PADDLE and
// KEYBOARD connectors are the devices behind them (Paddle, Keyboard), not Connectors.
class Connector final : public Component {
public:
    Connector(std::string reference, std::vector<PinSpec> pins);
    void evaluate(EvalContext& ctx) override;
};

// "POWER", "VIDEO", "PADDLE", "KEYBOARD"; Check()s the name.
std::vector<PinSpec> ConnectorPins(std::string_view part);

// True when the device behind connector `part` runs on the Board's power through the connector's
// own VCC and GND Pins ("PADDLE", "KEYBOARD"); false for POWER (it supplies) and VIDEO (the monitor
// does not).
bool ConnectorPowersDevice(std::string_view part);

// Declared voltage of a connector pin that supplies the Board: POWER VCC 5, GND 0; else nullopt.
std::optional<double> ConnectorVolts(std::string_view part, std::string_view pin);

}  // namespace bw
