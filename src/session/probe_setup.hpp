#pragma once

#include <optional>

#include "board/board.hpp"
#include "instruments/logic_probe.hpp"
#include "runner/probe_consumer.hpp"
#include "session/leads.hpp"

namespace bw {

struct ProbeTip {
    Point at;     // nm
    Layer layer;  // Active Layer when touched: which Trace a point on two Traces reads
};

// The Logic Probe as the player holds it (Session::probe()): where its tip touches, kept across
// rebuilds (the Net under it found again in each). Without a Machine it touches nothing.
class ProbeSetup {
public:
    explicit ProbeSetup(const SessionBuild& build) : build_(&build) {}

    void bind(ProbeConsumer& consumer);  // a new build: the Net under the tip watched

    void touch(std::optional<ProbeTip> tip);  // nullopt: lifted
    std::optional<ProbeTip> tip() const { return tip_; }
    std::optional<NetId> net() const { return consumer_->touched(); }
    ProbeLamps lamps() const;  // dark when not powered

private:
    void follow();  // the consumer onto the Net under tip_

    const SessionBuild* build_;
    ProbeConsumer* consumer_ = nullptr;
    std::optional<ProbeTip> tip_;
};

}  // namespace bw
