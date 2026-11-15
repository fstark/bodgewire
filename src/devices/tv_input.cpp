#include "devices/tv_input.hpp"

#include <span>

namespace bw {

namespace {

// Samples later than `after`; scans from the back, as after a trim only a few remain.
std::span<const Sample> After(std::span<const Sample> samples, Time after) {
    std::size_t first = samples.size();
    while (first > 0 && samples[first - 1].time > after) {
        --first;
    }
    return samples.subspan(first);
}

}  // namespace

TvInput::TvInput(Simulator& sim, NetId video, NetId syncN)
    : sim_(sim), nets_{video, syncN}, tv_(TvSync::Composite) {
    for (const NetId net : nets_) {
        sim_.record(net);
    }
}

TvInput::TvInput(Simulator& sim, SeparateDrives drives)
    : sim_(sim), nets_{drives.videoN, drives.hdrive, drives.vdrive}, tv_(TvSync::Separate) {
    for (const NetId net : nets_) {
        sim_.record(net);
    }
}

void TvInput::update() {
    // Every Net's samples since the last update, fed in time order (an earlier Net first on a tie).
    std::vector<std::span<const Sample>> streams;
    for (const NetId net : nets_) {
        streams.push_back(After(sim_.recorder().samples(net), fedUntil_));
    }
    std::vector<std::size_t> next(streams.size(), 0);
    while (true) {
        std::size_t pick = streams.size();
        for (std::size_t i = 0; i < streams.size(); ++i) {
            if (next[i] < streams[i].size() &&
                (pick == streams.size() ||
                 streams[i][next[i]].time < streams[pick][next[pick]].time)) {
                pick = i;
            }
        }
        if (pick == streams.size()) {
            break;
        }
        const Sample sample = streams[pick][next[pick]++];
        if (tv_.sync() == TvSync::Composite) {
            if (pick == 0) {
                tv_.setVideo(sample.time, sample.value);
            } else {
                tv_.setSyncN(sample.time, sample.value);
            }
        } else if (pick == 0) {
            tv_.setVideo(sample.time,
                         sample.value == LogicValue::Z ? LogicValue::Z : Not(sample.value));
        } else if (pick == 1) {
            tv_.setHorizontalDrive(sample.time, sample.value);
        } else {
            tv_.setVerticalDrive(sample.time, sample.value);
        }
    }
    tv_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
