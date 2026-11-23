#pragma once

#include <optional>

#include "instruments/multimeter.hpp"
#include "runner/single_net_consumer.hpp"

namespace bw {

// MeterState as a Consumer of the touched Net. reading(): UI thread.
class MeterConsumer final : public SingleNetConsumer<MeterState> {
public:
    std::optional<double> reading() const;  // for the touched Net; nullopt before a refresh

    void advanceTo(Time time) override;  // published_: (net + 1) << 32 | float bits
};

}  // namespace bw
