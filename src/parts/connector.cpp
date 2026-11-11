#include "parts/connector.hpp"

#include <string>
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
    if (part == "KEYBOARD") {
        std::vector<PinSpec> pins;
        for (int bit = 0; bit < 7; ++bit) {
            pins.push_back({"D" + std::to_string(bit), PinKind::Output, Family::Ttl});
        }
        pins.push_back({"STROBE", PinKind::Output, Family::Ttl});
        pins.push_back({"VCC", PinKind::Power, Family::Ttl});
        pins.push_back({"GND", PinKind::Power, Family::Ttl});
        return pins;
    }
    Check(part == "VIDEO", "unknown connector");
    return {{"VIDEO", PinKind::Input, Family::Ttl},
            {"SYNC_N", PinKind::Input, Family::Ttl},
            {"GND", PinKind::Power, Family::Ttl}};
}

bool ConnectorPowersDevice(std::string_view part) { return part == "PADDLE" || part == "KEYBOARD"; }

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
