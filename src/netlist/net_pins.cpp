#include "netlist/net_pins.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>

#include "parts/catalogue.hpp"

namespace bw {

namespace {

struct SplitReference {
    std::string_view prefix;
    std::optional<unsigned> number;  // trailing digits
};

SplitReference Split(std::string_view reference) {
    std::size_t start = reference.size();
    while (start > 0 && reference[start - 1] >= '0' && reference[start - 1] <= '9') {
        --start;
    }
    if (start == reference.size()) {
        return {reference, std::nullopt};
    }
    unsigned number = 0;
    std::from_chars(reference.data() + start, reference.data() + reference.size(), number);
    return {reference.substr(0, start), number};
}

const LoadedPart& FindLoadedPart(const LoadedCircuit& design, std::string_view reference) {
    return *std::ranges::find(design.parts, reference, &LoadedPart::reference);
}

const Connection* FindConnection(const LoadedCircuit& design, std::string_view reference,
                                 PinIndex pin) {
    const auto it = std::ranges::find_if(design.connections, [&](const Connection& c) {
        return c.reference == reference && c.pin == pin;
    });
    return it == design.connections.end() ? nullptr : &*it;
}

}  // namespace

NetPins DescribeNet(const LoadedCircuit& design, std::string_view net) {
    NetPins result{std::string(net), IsRail(net), {}, {}};
    for (const Connection& c : design.connections) {
        if (c.net != net) {
            continue;
        }
        const LoadedPart& part = FindLoadedPart(design, c.reference);
        NetPin pin{c.reference, c.pin, "", PinRole::Load, ""};
        if (part.info->kind == PartKind::Resistor) {
            pin.value = part.value;
            if (!result.rail) {
                const Connection* other = FindConnection(design, c.reference, 1 - c.pin);
                pin.role =
                    other != nullptr && other->net == "VCC" ? PinRole::PullUp : PinRole::PullDown;
            }
        } else {
            const PinSpec& spec = design.components.at(c.reference)->pins()[c.pin];
            pin.name = spec.name;
            if (IsDriver(spec.kind)) {
                pin.role = PinRole::Output;
            } else if (spec.kind == PinKind::Power && part.info->name == "POWER") {
                pin.role = PinRole::Supply;
            }
        }
        (pin.role != PinRole::Load ? result.drivers : result.loads).push_back(std::move(pin));
    }
    const auto less = [](const NetPin& a, const NetPin& b) {
        if (a.reference != b.reference) {
            return ReferenceLess(a.reference, b.reference);
        }
        return a.pin < b.pin;
    };
    std::ranges::sort(result.drivers, less);
    std::ranges::sort(result.loads, less);
    return result;
}

bool ReferenceLess(std::string_view a, std::string_view b) {
    const SplitReference sa = Split(a);
    const SplitReference sb = Split(b);
    if (sa.prefix != sb.prefix) {
        return sa.prefix < sb.prefix;
    }
    if (sa.number != sb.number) {
        return sa.number < sb.number;  // nullopt first
    }
    return a < b;
}

std::string FormatNetPin(const NetPin& pin) {
    std::string text = std::format("{} pin {}", pin.reference, pin.pin + 1);
    if (!pin.name.empty()) {
        text += " " + pin.name;
    }
    switch (pin.role) {
    case PinRole::PullUp:
        return text + std::format(" \u00b7 {} pull-up", pin.value);
    case PinRole::PullDown:
        return text + std::format(" \u00b7 {} pull-down", pin.value);
    case PinRole::Supply:
        return text + " \u00b7 supply";
    case PinRole::Output:
    case PinRole::Load:
        break;
    }
    return text;
}

}  // namespace bw
