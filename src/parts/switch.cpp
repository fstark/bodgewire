#include "parts/switch.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

constexpr std::uint32_t kNoGroup = UINT32_MAX;

constexpr std::array<std::string_view, 9> kKeyNames{"Space", "Enter", "Tab",  "Backspace", "Escape",
                                                    "Up",    "Down",  "Left", "Right"};

std::vector<PinSpec> NetworkPins(std::span<const std::string> netNames) {
    std::vector<PinSpec> pins;
    pins.reserve(netNames.size());
    for (const std::string& name : netNames) {
        pins.push_back({name, PinKind::Switched, Family::Ttl});
    }
    return pins;
}

}  // namespace

bool IsPcKey(std::string_view key) {
    if (key.size() == 1) {
        return key[0] > ' ' && key[0] < 0x7F && key[0] != '#';
    }
    return std::ranges::contains(kKeyNames, key);
}

// --- Switch ---

Switch::Switch(std::string reference, std::string legend, std::string pcKey)
    : Component(std::move(reference),
                {{"1", PinKind::Output, Family::Ttl}, {"2", PinKind::Output, Family::Ttl}}),
      legend_(std::move(legend)), pcKey_(std::move(pcKey)) {}

void Switch::powerOn(PowerOnContext& ctx) {
    ctx.setOutput(0, LogicValue::Z);
    ctx.setOutput(1, LogicValue::Z);
    closed_ = fault_ == SwitchFault::StuckClosed;
}

void Switch::evaluate(EvalContext& ctx) {
    const bool closed =
        fault_ == SwitchFault::StuckClosed || (fault_ != SwitchFault::NeverCloses && pressed());
    if (closed == closed_) {
        return;
    }
    closed_ = closed;
    if (network_ != nullptr) {
        network_->rebuild_ = true;
        ctx.wake(network_->id(), kSwitchDelay);
    }
}

// --- SwitchNetwork ---

SwitchNetwork::SwitchNetwork(std::span<const std::string> netNames)
    : Component("switches", NetworkPins(netNames)), parent_(netNames.size()),
      order_(netNames.size()), start_(netNames.size() + 1), size_(netNames.size()),
      grouped_(netNames.size()), groupOf_(netNames.size()), cursor_(netNames.size()) {}

void SwitchNetwork::add(Switch& member, PinIndex a, PinIndex b) {
    Check(a != b && a < pins().size() && b < pins().size(), "switch network: bad legs");
    Check(member.network_ == nullptr, "switch network: switch added twice");
    member.network_ = this;
    members_.push_back({&member, a, b});
}

void SwitchNetwork::powerOn(PowerOnContext& /*ctx*/) {
    // Every Switched Pin starts released (Z); the first evaluation joins the stuck-closed ones.
    std::ranges::fill(grouped_, 0);
    groups_ = 0;
    rebuild_ = true;
}

PinIndex SwitchNetwork::root(PinIndex pin) {
    while (parent_[pin] != pin) {
        parent_[pin] = parent_[parent_[pin]];
        pin = parent_[pin];
    }
    return pin;
}

void SwitchNetwork::evaluate(EvalContext& ctx) {
    if (rebuild_) {
        rebuild_ = false;
        rebuild(ctx);
        for (std::uint32_t g = 0; g < groups_; ++g) {
            resolve(ctx, g);
        }
        return;
    }
    for (std::uint32_t g = 0; g < groups_; ++g) {
        for (std::uint32_t i = start_[g]; i < start_[g + 1]; ++i) {
            if (ctx.externalChanged(order_[i])) {
                resolve(ctx, g);
                break;
            }
        }
    }
}

void SwitchNetwork::rebuild(EvalContext& ctx) {
    const auto pinCount = static_cast<PinIndex>(parent_.size());
    for (PinIndex p = 0; p < pinCount; ++p) {
        parent_[p] = p;
        size_[p] = 0;
        groupOf_[p] = kNoGroup;
    }
    for (const Member& m : members_) {
        if (m.member->closed()) {
            const PinIndex ra = root(m.a);
            const PinIndex rb = root(m.b);
            if (ra != rb) {
                parent_[std::max(ra, rb)] = std::min(ra, rb);
            }
        }
    }
    for (PinIndex p = 0; p < pinCount; ++p) {
        ++size_[root(p)];
    }
    // Groups of two Pins or more, numbered by their lowest Pin.
    groups_ = 0;
    for (PinIndex p = 0; p < pinCount; ++p) {
        const PinIndex r = root(p);
        if (size_[r] >= 2 && groupOf_[r] == kNoGroup) {
            groupOf_[r] = groups_;
            start_[groups_ + 1] = size_[r];
            ++groups_;
        }
    }
    start_[0] = 0;
    for (std::uint32_t g = 0; g < groups_; ++g) {
        start_[g + 1] += start_[g];
        cursor_[g] = start_[g];
    }
    for (PinIndex p = 0; p < pinCount; ++p) {
        const std::uint32_t g = groupOf_[root(p)];
        const bool now = g != kNoGroup;
        if (now) {
            order_[cursor_[g]++] = p;
        } else if (grouped_[p] != 0) {
            ctx.drive(p, LogicValue::Z, kSwitchDelay);  // left its group: its Net on its own
        }
        grouped_[p] = now ? 1 : 0;
    }
}

void SwitchNetwork::resolve(EvalContext& ctx, std::uint32_t group) {
    Resolver drives;
    for (std::uint32_t i = start_[group]; i < start_[group + 1]; ++i) {
        drives.merge(ctx.external(order_[i]));
    }
    // Floating: each Net floats on its own (the value finish() gives it is not used).
    const Resolution r = drives.finish(false, LogicValue::Zero);
    const LogicValue value = r.floating ? LogicValue::Z : r.value;
    for (std::uint32_t i = start_[group]; i < start_[group + 1]; ++i) {
        ctx.drive(order_[i], value, kSwitchDelay);
    }
}

}  // namespace bw
