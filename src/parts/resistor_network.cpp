#include "parts/resistor_network.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

constexpr std::uint64_t kFar = std::numeric_limits<std::uint64_t>::max();

std::vector<PinSpec> JoinedPins(std::span<const std::string> netNames) {
    std::vector<PinSpec> pins;
    pins.reserve(netNames.size());
    for (const std::string& name : netNames) {
        pins.push_back({name, PinKind::Joined, Family::Ttl});
    }
    return pins;
}

// A candidate value reaching a Pin at `distance`: the nearer wins; at the same distance two
// different values give X.
void Offer(std::uint64_t distance, LogicValue value, std::uint64_t& bestDistance,
           LogicValue& bestValue) {
    if (distance < bestDistance) {
        bestDistance = distance;
        bestValue = value;
    } else if (distance == bestDistance && value != bestValue) {
        bestValue = LogicValue::X;
    }
}

}  // namespace

ResistorNetwork::ResistorNetwork(std::span<const std::string> netNames)
    : Component("resistors", JoinedPins(netNames)), distance_(netNames.size(), kFar),
      value_(netNames.size(), LogicValue::Z), source_(netNames.size(), 0),
      driven_(netNames.size(), LogicValue::Z) {}

void ResistorNetwork::join(PinIndex a, PinIndex b, std::uint32_t ohms) {
    Check(a != b && a < pins().size() && b < pins().size(), "resistor network: bad ends");
    Check(!built_, "resistor network: join after power-on");
    edges_.push_back({a, b, ohms});
}

void ResistorNetwork::pull(PinIndex pin, LogicValue value, std::uint32_t ohms) {
    Check(pin < pins().size(), "resistor network: bad pull");
    Check(value == LogicValue::Zero || value == LogicValue::One, "resistor network: pull to 0/1");
    Check(!built_, "resistor network: pull after power-on");
    pulls_.push_back({pin, value, ohms});
}

void ResistorNetwork::powerOn(PowerOnContext& /*ctx*/) {
    // Every Joined Pin starts released (Z); the first evaluation resolves every group.
    if (!built_) {
        build();
    }
    std::ranges::fill(driven_, LogicValue::Z);
    fresh_ = true;
}

// Groups (union-find over the series resistors), the links of each Pin, and the scratch arrays:
// once, so resolve() allocates nothing.
void ResistorNetwork::build() {
    built_ = true;
    const auto pinCount = static_cast<PinIndex>(pins().size());
    std::vector<PinIndex> parent(pinCount);
    std::iota(parent.begin(), parent.end(), PinIndex{0});
    const auto root = [&](PinIndex p) {
        while (parent[p] != p) {
            parent[p] = parent[parent[p]];
            p = parent[p];
        }
        return p;
    };
    for (const Edge& e : edges_) {
        const PinIndex ra = root(e.a), rb = root(e.b);
        if (ra != rb) {
            parent[std::max(ra, rb)] = std::min(ra, rb);
        }
    }

    linkStart_.assign(pinCount + 1, 0);
    for (const Edge& e : edges_) {
        ++linkStart_[e.a + 1];
        ++linkStart_[e.b + 1];
    }
    std::partial_sum(linkStart_.begin(), linkStart_.end(), linkStart_.begin());
    links_.resize(linkStart_.back());
    std::vector<std::uint32_t> cursor(linkStart_.begin(), linkStart_.end() - 1);
    for (const Edge& e : edges_) {
        links_[cursor[e.a]++] = {e.b, e.ohms};
        links_[cursor[e.b]++] = {e.a, e.ohms};
    }

    pullStart_.assign(pinCount + 1, 0);
    for (const Pull& pull : pulls_) {
        ++pullStart_[pull.pin + 1];
    }
    std::partial_sum(pullStart_.begin(), pullStart_.end(), pullStart_.begin());
    pullsByPin_.resize(pulls_.size());
    std::vector<std::uint32_t> next(pullStart_.begin(), pullStart_.end() - 1);
    for (const Pull& pull : pulls_) {
        pullsByPin_[next[pull.pin]++] = pull;
    }

    // Groups numbered by their lowest Pin, each Pin in exactly one (every Pin has a link).
    std::vector<std::uint32_t> groupOf(pinCount, UINT32_MAX);
    std::vector<std::uint32_t> size;
    for (PinIndex p = 0; p < pinCount; ++p) {
        const PinIndex r = root(p);
        if (groupOf[r] == UINT32_MAX) {
            groupOf[r] = static_cast<std::uint32_t>(size.size());
            size.push_back(0);
        }
        ++size[groupOf[r]];
    }
    groups_ = static_cast<std::uint32_t>(size.size());
    start_.assign(groups_ + 1, 0);
    for (std::uint32_t g = 0; g < groups_; ++g) {
        start_[g + 1] = start_[g] + size[g];
    }
    order_.resize(pinCount);
    std::vector<std::uint32_t> slot(start_.begin(), start_.end() - 1);
    for (PinIndex p = 0; p < pinCount; ++p) {
        order_[slot[groupOf[root(p)]]++] = p;
    }
    groupOf_.resize(pinCount);
    for (PinIndex p = 0; p < pinCount; ++p) {
        groupOf_[p] = groupOf[root(p)];
    }
    pair_.assign(groups_, 0);
    for (std::uint32_t g = 0; g < groups_; ++g) {
        if (size[g] != 2) {
            continue;
        }
        const PinIndex a = order_[start_[g]], b = order_[start_[g] + 1];
        const auto one = [&](PinIndex p) {
            return linkStart_[p + 1] - linkStart_[p] == 1 && pullStart_[p + 1] == pullStart_[p];
        };
        pair_[g] = one(a) && one(b) ? 1 : 0;
    }
    pending_.reserve(groups_);
}

// Only the groups with a Pin whose other drives changed now, in group order (as a scan of every
// group would find them); the power-on evaluation resolves every group.
void ResistorNetwork::evaluate(EvalContext& ctx) {
    if (fresh_) {
        fresh_ = false;
        for (std::uint32_t g = 0; g < groups_; ++g) {
            resolve(ctx, g);
        }
        return;
    }
    pending_.clear();
    const std::uint32_t first = ToIndex(firstPin());
    for (const PinId id : ctx.changedNetworkPins()) {
        const std::uint32_t p = ToIndex(id) - first;  // another Component's wraps past the end
        if (p < groupOf_.size()) {
            pending_.push_back(groupOf_[p]);
        }
    }
    if (pending_.size() > 1) {
        std::ranges::sort(pending_);
        const auto [end, last] = std::ranges::unique(pending_);
        pending_.erase(end, last);
    }
    for (const std::uint32_t g : pending_) {
        if (pair_[g] != 0) {
            resolvePair(ctx, g);
        } else {
            resolve(ctx, g);
        }
    }
}

// resolve() for two Nets joined by one resistor, neither pulled: a Net with a source of its own
// is released; one without takes the other's source if it has one, else floats (Z).
void ResistorNetwork::resolvePair(EvalContext& ctx, std::uint32_t group) {
    const PinIndex a = order_[start_[group]], b = order_[start_[group] + 1];
    const Resolver& externalA = ctx.external(a);
    const Resolver& externalB = ctx.external(b);
    const bool sourceA = externalA.strength() != DriveStrength::None;
    const bool sourceB = externalB.strength() != DriveStrength::None;
    const LogicValue valueA =
        sourceB && !sourceA ? externalB.finish(false, LogicValue::Zero).value : LogicValue::Z;
    const LogicValue valueB =
        sourceA && !sourceB ? externalA.finish(false, LogicValue::Zero).value : LogicValue::Z;
    if (valueA != driven_[a]) {
        driven_[a] = valueA;
        ctx.drive(a, valueA, kJoinDelay);
    }
    if (valueB != driven_[b]) {
        driven_[b] = valueB;
        ctx.drive(b, valueB, kJoinDelay);
    }
}

void ResistorNetwork::resolve(EvalContext& ctx, std::uint32_t group) {
    const std::uint32_t first = start_[group], last = start_[group + 1];
    // Sources: a Net driven by anything but this network.
    for (std::uint32_t i = first; i < last; ++i) {
        const PinIndex p = order_[i];
        const Resolver& own = ctx.external(p);
        source_[p] = own.strength() != DriveStrength::None ? 1 : 0;
        distance_[p] = source_[p] != 0 ? 0 : kFar;
        value_[p] = source_[p] != 0 ? own.finish(false, LogicValue::Zero).value : LogicValue::Z;
    }
    for (std::uint32_t i = first; i < last; ++i) {
        const PinIndex p = order_[i];
        if (source_[p] != 0) {
            continue;
        }
        for (std::uint32_t k = pullStart_[p]; k < pullStart_[p + 1]; ++k) {
            Offer(pullsByPin_[k].ohms, pullsByPin_[k].value, distance_[p], value_[p]);
        }
    }
    // Bellman-Ford over the group: at most size - 1 passes, stopping when nothing moves.
    for (std::uint32_t pass = first + 1; pass < last; ++pass) {
        bool moved = false;
        for (std::uint32_t i = first; i < last; ++i) {
            const PinIndex a = order_[i];
            if (distance_[a] == kFar) {
                continue;
            }
            for (std::uint32_t l = linkStart_[a]; l < linkStart_[a + 1]; ++l) {
                const PinIndex b = links_[l].other;
                if (source_[b] != 0) {
                    continue;
                }
                const std::uint64_t d = distance_[a] + links_[l].ohms;
                const std::uint64_t before = distance_[b];
                const LogicValue was = value_[b];
                Offer(d, value_[a], distance_[b], value_[b]);
                moved |= distance_[b] != before || value_[b] != was;
            }
        }
        if (!moved) {
            break;
        }
    }
    for (std::uint32_t i = first; i < last; ++i) {
        const PinIndex p = order_[i];
        const LogicValue value = source_[p] != 0 ? LogicValue::Z : value_[p];
        if (value != driven_[p]) {
            driven_[p] = value;
            ctx.drive(p, value, kJoinDelay);
        }
    }
}

}  // namespace bw
