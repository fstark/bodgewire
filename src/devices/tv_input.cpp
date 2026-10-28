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
    : sim_(sim), video_(video), syncN_(syncN) {
    sim_.record(video_);
    sim_.record(syncN_);
}

void TvInput::update() {
    const auto video = After(sim_.recorder().samples(video_), fedUntil_);
    const auto syncN = After(sim_.recorder().samples(syncN_), fedUntil_);
    std::size_t v = 0, s = 0;
    while (v < video.size() || s < syncN.size()) {
        if (s == syncN.size() || (v < video.size() && video[v].time <= syncN[s].time)) {
            tv_.setVideo(video[v].time, video[v].value);
            ++v;
        } else {
            tv_.setSyncN(syncN[s].time, syncN[s].value);
            ++s;
        }
    }
    tv_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
