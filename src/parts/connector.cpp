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
// The ZX80's expansion bus (P5x): upper contact n on pad 2n - 1, lower contact n on pad 2n, so
// each pair faces as on the edge. The upper row is the memory-side data bus and the Z80's
// control lines; the lower the supplies, the clock and the address bus. ROMCS (lower 23) is the
// ZX81's: not connected on the ZX80, nor the key slot and the 9 V pad.
constexpr std::array<std::string_view, 46> kExpansion{
    "D7",      "VCC5", "RAMCS_N", "NC9V", "SLOTA",   "SLOTB", "D0",     "GND1", "D1",      "GND2",
    "D2",      "PHI",  "D6",      "A0",   "D5",      "A1",    "D3",     "A2",   "D4",      "A3",
    "INT_N",   "A15",  "NMI_N",   "A14",  "HALT_N",  "A13",   "MREQ_N", "A12",  "IORQ_N",  "A11",
    "RD_N",    "A10",  "WR_N",    "A9",   "BUSAK_N", "A8",    "WAIT_N", "A7",   "BUSRQ_N", "A6",
    "RESET_N", "A5",   "M1_N",    "A4",   "RFSH_N",  "NC23"};
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
    if (part == "MONITOR") {
        // The PET 2001's J7: signals as VIDEO's, the grounds plain loads on GND, pad 6 unused.
        const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
        const auto ground = [](const char* name) {
            return PinSpec{name, PinKind::Input, Family::Mos, false, true};
        };
        return {in("VIDEO_N"),  ground("GND1"), in("VDRIVE"),
                ground("GND2"), in("HDRIVE"),   {"NC6", PinKind::NoConnect, Family::Ttl},
                ground("GND3")};
    }
    if (part == "BW2001KB") {
        std::vector<PinSpec> pins;
        for (int row = 0; row < 10; ++row) {
            pins.push_back({"ROW" + std::to_string(row), PinKind::Input, Family::Mos, false, true});
        }
        for (int column = 0; column < 8; ++column) {
            pins.push_back(
                {"COL" + std::to_string(column), PinKind::Input, Family::Mos, false, true});
        }
        pins.push_back({"NC19", PinKind::NoConnect, Family::Ttl});
        pins.push_back({"NC20", PinKind::NoConnect, Family::Ttl});
        return pins;
    }
    if (part == "BW80KB") {
        // The rows wake the Part: it answers inside an IN. The columns are diodes pulling low.
        std::vector<PinSpec> pins;
        for (int row = 8; row < 16; ++row) {
            pins.push_back({"A" + std::to_string(row), PinKind::Input, Family::Mos});
        }
        for (int column = 0; column < 5; ++column) {
            pins.push_back({"KB" + std::to_string(column), PinKind::OpenCollector, Family::Ttl});
        }
        return pins;
    }
    if (part == "EXPANSION") {
        std::vector<PinSpec> pins;
        for (std::size_t i = 0; i < kExpansion.size(); ++i) {
            const std::string_view name = kExpansion[i];
            const bool unconnected = name.starts_with("SLOT") || name.starts_with("NC");
            pins.push_back(
                unconnected ? PinSpec{std::string(name), PinKind::NoConnect, Family::Ttl}
                            : PinSpec{std::string(name), PinKind::Input, Family::Mos, false, true});
        }
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
