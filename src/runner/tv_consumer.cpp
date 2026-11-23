#include "runner/tv_consumer.hpp"

#include <algorithm>

namespace bw {

void TvConsumer::powerOn() {
    tv_ = Tv{tv_.sync()};
    lastFrames_ = 0;
}

void TvConsumer::sample(NetId net, Sample sample) {
    const auto input = std::ranges::find(nets_, net) - nets_.begin();
    tv_.feed(static_cast<std::size_t>(input), sample.time, sample.value);
}

void TvConsumer::advanceTo(Time time) {
    tv_.advanceTo(time);
    if (tv_.completedFrames() != lastFrames_) {
        lastFrames_ = tv_.completedFrames();
        pictures_.back() = {tv_.completedFrame(), tv_.isLocked(), tv_.completedStats().lines};
        pictures_.publish();
    }
}

void TvConsumer::powerOff() {
    pictures_.back() = TvPicture{};
    pictures_.publish();
}

}  // namespace bw
