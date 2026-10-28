#include "devices/tv_input.hpp"

#include <span>

namespace bw {

TvInput::TvInput(Simulator& sim, NetId video, NetId syncN, History history)
    : sim_(sim), video_(video), syncN_(syncN), history_(history) {
    sim_.record(video_);
    sim_.record(syncN_);
}

void TvInput::update() {
    const std::span<const Sample> video = sim_.recorder().samples(video_).subspan(videoFed_);
    const std::span<const Sample> syncN = sim_.recorder().samples(syncN_).subspan(syncNFed_);
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
    if (history_ == History::Trim) {
        sim_.trimRecording(video_);
        sim_.trimRecording(syncN_);
        videoFed_ = syncNFed_ = 1;  // the kept Sample was fed
    } else {
        videoFed_ += video.size();
        syncNFed_ += syncN.size();
    }
}

}  // namespace bw
