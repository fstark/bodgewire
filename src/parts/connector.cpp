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
    Check(part == "VIDEO", "unknown connector");
    return {{"VIDEO", PinKind::Input, Family::Ttl},
            {"SYNC_N", PinKind::Input, Family::Ttl},
            {"GND", PinKind::Power, Family::Ttl}};
}

}  // namespace bw
