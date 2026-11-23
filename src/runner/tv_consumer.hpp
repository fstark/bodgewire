#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "devices/tv.hpp"
#include "runner/consumer.hpp"
#include "runner/triple_buffer.hpp"

namespace bw {

struct TvPicture {
    TvFrame frame;  // black when off or before the first frame
    bool locked = false;
    int lines = 0;  // lines started in the frame (0 when off or before the first)
};

// Tv as a Consumer; each completed frame is published for the UI. A VIDEO connector's two Nets
// (composite sync) or a MONITOR's three (separate drives; its video is low for a lit dot).
class TvConsumer final : public Consumer {
public:
    explicit TvConsumer(const TvNets& nets) : nets_(NetsOf(nets)), tv_(SyncOf(nets)) {}
    TvConsumer(NetId video, NetId syncN) : TvConsumer(CompositeVideo{video, syncN}) {}
    TripleBuffer<TvPicture>& pictures() { return pictures_; }  // the UI reads
    const Tv& tv() const { return tv_; }

    std::span<const NetId> nets() const override { return nets_; }
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    std::vector<NetId> nets_;  // VIDEO, SYNC_N; or VIDEO_N, HDRIVE, VDRIVE
    Tv tv_;
    std::uint64_t lastFrames_ = 0;
    TripleBuffer<TvPicture> pictures_;
};

}  // namespace bw
