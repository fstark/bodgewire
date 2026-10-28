#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "devices/tv.hpp"
#include "runner/consumer.hpp"
#include "runner/triple_buffer.hpp"

namespace bw {

struct TvPicture {
    TvFrame frame;  // black when off or before the first frame
    bool locked = false;
};

// Tv as a Consumer; each completed frame is published for the UI.
class TvConsumer final : public Consumer {
public:
    TvConsumer(NetId video, NetId syncN) : nets_{video, syncN} {}
    TripleBuffer<TvPicture>& pictures() { return pictures_; }  // the UI reads
    const Tv& tv() const { return tv_; }

    std::span<const NetId> nets() const override { return nets_; }
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    std::array<NetId, 2> nets_;  // VIDEO, SYNC_N
    Tv tv_;
    std::uint64_t lastFrames_ = 0;
    TripleBuffer<TvPicture> pictures_;
};

}  // namespace bw
