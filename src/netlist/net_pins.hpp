#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "netlist/builder.hpp"

namespace bw {

// Series: a resistor end whose other end is on a signal Net (P5u).
enum class PinRole : std::uint8_t { Output, PullUp, PullDown, Series, Supply, Load };

struct NetPin {
    std::string reference;  // "U6"
    PinIndex pin;           // package pin number - 1
    std::string name;       // datasheet name "1Y"; "" for resistors
    PinRole role;
    std::string value;  // resistors: "4k7", else ""
    bool operator==(const NetPin&) const = default;
};

struct NetPins {
    std::string net;
    bool rail = false;            // GND or VCC
    std::vector<NetPin> drivers;  // Output, PullUp, PullDown, Series, Supply
    std::vector<NetPin> loads;    // everything else
};

// Drivers and Loads of a design Net, each sorted by ReferenceLess then pin. Unknown net → empty.
NetPins DescribeNet(const LoadedCircuit& design, std::string_view net);

// Natural order: "U2" < "U10" < "X1"; letters first, then the number, then the whole text.
bool ReferenceLess(std::string_view a, std::string_view b);

// "U6 pin 6 1Y", "R1 pin 2 · 4k7 pull-up", "J1 pin 1 VCC · supply" (NET_PANEL.md S3).
std::string FormatNetPin(const NetPin& pin);

}  // namespace bw
