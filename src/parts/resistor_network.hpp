#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// ps: the network drives a Net this long after a change of another Net in its group.
inline constexpr Time kJoinDelay = 1;

// The Circuit's series resistors as one Component (P5u): a Joined Pin on each Net a series
// resistor is on, named after the Net. A group (Nets linked by series resistors) is resolved as
// a whole: a Net with any drive of its own is a source; every other Net takes the value of its
// nearest source or pull resistor, nearest counted in ohms, at Weak strength. Equal distances
// that disagree give X; no source in reach gives Z (the Net floats). Limits: any drive of a Net's
// own makes it a source, a weak one too (a port's pull-up 1 would beat a strong output beyond a
// resistor); a Net its source lets go of floats for kJoinDelay before the network drives it.
class ResistorNetwork final : public Component {
public:
    explicit ResistorNetwork(std::span<const std::string> netNames);
    void join(PinIndex a, PinIndex b, std::uint32_t ohms);          // builder: a ≠ b
    void pull(PinIndex pin, LogicValue value, std::uint32_t ohms);  // builder: Zero or One

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    struct Link {
        PinIndex other;
        std::uint32_t ohms;
    };
    struct Pull {
        PinIndex pin;
        LogicValue value;
        std::uint32_t ohms;
    };
    struct Edge {
        PinIndex a, b;
        std::uint32_t ohms;
    };
    void build();
    void resolve(EvalContext& ctx, std::uint32_t group);

    std::vector<Edge> edges_;               // builder input
    std::vector<Pull> pulls_;               // builder input
    std::vector<std::uint32_t> linkStart_;  // Pin p: links_[linkStart_[p], linkStart_[p + 1])
    std::vector<Link> links_;
    std::vector<std::uint32_t> pullStart_;  // Pin p: pullsByPin_[pullStart_[p], pullStart_[p + 1])
    std::vector<Pull> pullsByPin_;
    std::vector<PinIndex> order_;          // Pins of the groups, group by group
    std::vector<std::uint32_t> start_;     // group g: order_[start_[g], start_[g + 1])
    std::vector<std::uint64_t> distance_;  // per Pin, scratch: ohms to its value's source
    std::vector<LogicValue> value_;        // per Pin, scratch
    std::vector<std::uint8_t> source_;     // per Pin, scratch: driven by something else
    std::vector<LogicValue> driven_;       // per Pin: last value driven
    std::uint32_t groups_ = 0;
    bool built_ = false;
    bool fresh_ = false;  // powered on, not evaluated yet
};

}  // namespace bw
