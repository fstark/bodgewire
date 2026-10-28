#include "runner/tv_consumer.hpp"

namespace bw {

void TvConsumer::powerOn() {
    tv_ = Tv{};
    lastFrames_ = 0;
}

void TvConsumer::sample(NetId net, Sample sample) {
    if (net == nets_[0]) {
        tv_.setVideo(sample.time, sample.value);
    } else {
        tv_.setSyncN(sample.time, sample.value);
    }
}

void TvConsumer::advanceTo(Time time) {
    tv_.advanceTo(time);
    if (tv_.completedFrames() != lastFrames_) {
        lastFrames_ = tv_.completedFrames();
        pictures_.back() = {tv_.completedFrame(), tv_.isLocked()};
        pictures_.publish();
    }
}

void TvConsumer::powerOff() {
    pictures_.back() = TvPicture{};
    pictures_.publish();
}

}  // namespace bw
