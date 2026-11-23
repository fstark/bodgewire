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

// One pair of legs the SwitchNetwork joins while closed: a Switch's, or one key of a BW2001KB
// (P5q). Its owner sets it from its own evaluate.
class SwitchContact {
public:
    bool closed() const { return closed_; }
    bool attached() const { return network_ != nullptr; }
    // Simulation thread: a change wakes the network kSwitchDelay later.
    void set(EvalContext& ctx, bool closed);
    // Before power-on, or at power-on: no wake (the network rebuilds at its own power-on).
    void reset(bool closed) { closed_ = closed; }

private:
    friend class SwitchNetwork;
    bool closed_ = false;
    SwitchNetwork* network_ = nullptr;
};

// BUTTON (a push button), KEY (a keyboard key) or SLIDE (stays where it is moved): the Part a
// Switch is, for how it is drawn and clicked.
enum class SwitchKind : std::uint8_t { Button, Key, Slide };

// A push button or key (P5i): two legs ("1", "2", pad numbers) joined while closed; the
// SwitchNetwork joins their Nets. Its Pins are TTL Outputs that always drive Z, as the P4d
// button's were, so a build draws the same random bits at power-on. Pin index = KiCad pad − 1.
class Switch final : public Component {
public:
    // `pcKey` "" or IsPcKey (letters upper case).
    Switch(std::string reference, SwitchKind kind, std::string legend, std::string pcKey);

    SwitchKind kind() const { return kind_; }

    std::string_view legend() const { return legend_; }  // "" for a BUTTON or a blank key
    std::string_view pcKey() const { return pcKey_; }    // "" none

    // Any thread. Takes effect at this switch's next evaluation: follow with Simulator or Runner
    // wake(id()).
    void setPressed(bool pressed) { pressed_.store(pressed, std::memory_order_relaxed); }
    bool pressed() const { return pressed_.load(std::memory_order_relaxed); }
    void setFault(SwitchFault fault) { fault_ = fault; }  // builder, before power-on
    SwitchFault fault() const { return fault_; }
    bool closed() const { return contact_.closed(); }  // simulation thread: at the last evaluation
    bool attached() const { return contact_.attached(); }  // joins two Nets
    SwitchContact& contact() { return contact_; }

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    friend class SwitchNetwork;
    SwitchKind kind_;
    std::string legend_, pcKey_;
    std::atomic<bool> pressed_{false};
    SwitchFault fault_ = SwitchFault::None;
    SwitchContact contact_;
};

// The Board's switches as one Component (P5i): a Switched Pin on each Net a switch leg is on,
// named after the Net. Closed switches join Nets into groups; each group is driven, at Supply
// strength, with the combined drives of its Nets (read without the network's own).
class SwitchNetwork final : public Component {
public:
    explicit SwitchNetwork(std::span<const std::string> netNames);
    void add(SwitchContact& member, PinIndex a, PinIndex b);  // builder: legs on Pins a ≠ b

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    friend class SwitchContact;
    struct Member {
        SwitchContact* member;
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
