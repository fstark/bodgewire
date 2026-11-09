#include "parts/connector.hpp"

#include <utility>

#include "sim/check.hpp"

namespace bw {

Connector::Connector(std::string reference, std::vector<PinSpec> pins)
    : Component(std::move(reference), std::move(pins)) {}

void Connector::evaluate(EvalContext& /*ctx*/) {}

std::vector<PinSpec> ConnectorPins(std::string_view part) {
    if (part == "POWER") {
        return {{"VCC", PinKind::Power, Family::Ttl}, {"GND", PinKind::Power, Family::Ttl}};
    }
    if (part == "PADDLE") {
        return {{"TRIG_N", PinKind::Input, Family::Ttl},
                {"OUT", PinKind::Output, Family::Ttl},
                {"VCC", PinKind::Power, Family::Ttl},
                {"GND", PinKind::Power, Family::Ttl}};
    }
    Check(part == "VIDEO", "unknown connector");
    return {{"VIDEO", PinKind::Input, Family::Ttl},
            {"SYNC_N", PinKind::Input, Family::Ttl},
            {"GND", PinKind::Power, Family::Ttl}};
}

bool ConnectorPowersDevice(std::string_view part) { return part == "PADDLE"; }

std::optional<double> ConnectorVolts(std::string_view part, std::string_view pin) {
    if (part == "POWER") {
        if (pin == "VCC") {
            return 5.0;
        }
        if (pin == "GND") {
            return 0.0;
        }
    }
    return std::nullopt;
}

}  // namespace bw
