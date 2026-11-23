#pragma once

#include "instruments/logic_probe.hpp"
#include "runner/single_net_consumer.hpp"

namespace bw {

// The Logic Probe as a Consumer of the touched Net. lamps(): UI thread.
class ProbeConsumer final : public SingleNetConsumer<ProbeState> {
public:
    ProbeLamps lamps() const;  // latest completed slice, for the touched Net; dark otherwise

    void advanceTo(Time time) override;  // published_: net id << 32 | kValid | lamp bits
};

}  // namespace bw
