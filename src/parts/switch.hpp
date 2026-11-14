#pragma once

#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// ps: a switch woken by a press wakes the network after this, and the network drives its Nets
// this long after its evaluation.
inline constexpr Time kSwitchDelay = 1;
// From a switch's wake to its Nets' new values.
inline constexpr Time kPressLatency = 2 * kSwitchDelay;

enum class SwitchFault : std::uint8_t { None, StuckClosed, NeverCloses };

// One printable ASCII character other than space and '#' (letters in either case), or one of
// Space, Enter, Tab, Backspace, Escape, Up, Down, Left, Right.
bool IsPcKey(std::string_view key);

class SwitchNetwork;

// A push button or key (P5i): two legs ("1", "2", pad numbers) joined while closed; the
// SwitchNetwork joins their Nets. Its Pins are TTL Outputs that always drive Z, as the P4d
// button's were, so a build draws the same random bits at power-on. Pin index = KiCad pad − 1.
class Switch final : public Component {
public:
    // `pcKey` "" or IsPcKey (letters upper case).
    Switch(std::string reference, std::string legend, std::string pcKey);

    std::string_view legend() const { return legend_; }  // "" for a BUTTON or a blank key
    std::string_view pcKey() const { return pcKey_; }    // "" none

    // Any thread. Takes effect at this switch's next evaluation: follow with Simulator or Runner
    // wake(id()).
    void setPressed(bool pressed) { pressed_.store(pressed, std::memory_order_relaxed); }
    bool pressed() const { return pressed_.load(std::memory_order_relaxed); }
    void setFault(SwitchFault fault) { fault_ = fault; }  // builder, before power-on
    SwitchFault fault() const { return fault_; }
    bool closed() const { return closed_; }  // simulation thread: as of the last evaluation
    bool attached() const { return network_ != nullptr; }  // joins two Nets

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    friend class SwitchNetwork;
    std::string legend_, pcKey_;
    std::atomic<bool> pressed_{false};
    SwitchFault fault_ = SwitchFault::None;
    bool closed_ = false;
    SwitchNetwork* network_ = nullptr;
};

// The Board's switches as one Component (P5i): a Switched Pin on each Net a switch leg is on,
// named after the Net. Closed switches join Nets into groups; each group is driven, at Supply
// strength, with the combined drives of its Nets (read without the network's own).
class SwitchNetwork final : public Component {
public:
    explicit SwitchNetwork(std::span<const std::string> netNames);
    void add(Switch& member, PinIndex a, PinIndex b);  // builder: legs on Pins a ≠ b

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    friend class Switch;
    struct Member {
        Switch* member;
        PinIndex a, b;
    };
    PinIndex root(PinIndex pin);
    void rebuild(EvalContext& ctx);
    void resolve(EvalContext& ctx, std::uint32_t group);

    std::vector<Member> members_;
    std::vector<PinIndex> parent_;        // union-find over Pins
    std::vector<PinIndex> order_;         // Pins of the groups, group by group
    std::vector<std::uint32_t> start_;    // group g: order_[start_[g], start_[g + 1])
    std::vector<std::uint32_t> size_;     // rebuild scratch, per root: Pins in its set
    std::vector<std::uint8_t> grouped_;   // per Pin: in a group now
    std::vector<std::uint32_t> groupOf_;  // rebuild scratch, per root: its group
    std::vector<std::uint32_t> cursor_;   // rebuild scratch, per group: next slot in order_
    std::uint32_t groups_ = 0;
    bool rebuild_ = true;  // a member's closed() changed (simulation thread)
};

}  // namespace bw
