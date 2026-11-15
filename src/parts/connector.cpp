#include "parts/connector.hpp"

#include <array>
#include <span>
#include <string>
#include <utility>

#include "sim/check.hpp"

namespace bw {

Connector::Connector(std::string reference, std::vector<PinSpec> pins)
    : Component(std::move(reference), std::move(pins)) {}

void Connector::evaluate(EvalContext& /*ctx*/) {}

namespace {

// The PET 2001's edge connectors and cassette ports (P5q), by pad: number row 1-12 on the odd
// pads, letter row A-N (no G, I) on the even ones, so each letter faces its number as on the
// PET's edge.
constexpr std::array<std::string_view, 24> kIeee488{
    "DIO1",  "DIO5", "DIO2",  "DIO6", "DIO3",   "DIO7", "DIO4",    "DIO8",
    "EOI_N", "REN",  "DAV_N", "GND1", "NRFD_N", "GND2", "NDAC_N",  "GND3",
    "IFC_N", "GND4", "SRQ_N", "GND5", "ATN_N",  "GND6", "CHASSIS", "GND7"};
constexpr std::array<std::string_view, 24> kUserPort{
    "GND1", "GND4", "VIDEO", "CA1", "SRQ_N", "PA0", "EOI_N", "PA1",
    "DIAG", "PA2",  "READ1", "PA3", "WRITE", "PA4", "READ2", "PA5",
    "HORZ", "PA6",  "VERT",  "PA7", "GND2",  "CB2", "GND3",  "GND5"};
constexpr std::array<std::string_view, 6> kCassette{"GND",  "VCC",   "MOTOR",
                                                    "READ", "WRITE", "SENSE"};

// Every pin a sampled MOS Input: no behaviour, no "floats high", never woken by its Nets.
std::vector<PinSpec> PassivePins(std::string_view part) {
    std::span<const std::string_view> names = kCassette;
    if (part == "IEEE488") {
        names = kIeee488;
    } else if (part == "USERPORT") {
        names = kUserPort;
    }
    std::vector<PinSpec> pins;
    for (const std::string_view name : names) {
        pins.push_back({std::string(name), PinKind::Input, Family::Mos, false, true});
    }
    return pins;
}

}  // namespace

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
    if (part == "IEEE488" || part == "USERPORT" || part == "CASSETTE") {
        return PassivePins(part);
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
