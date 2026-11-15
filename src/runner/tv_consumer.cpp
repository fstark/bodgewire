#include "runner/tv_consumer.hpp"

namespace bw {

void TvConsumer::powerOn() {
    tv_ = Tv{tv_.sync()};
    lastFrames_ = 0;
}

void TvConsumer::sample(NetId net, Sample sample) {
    if (tv_.sync() == TvSync::Composite) {
        if (net == nets_[0]) {
            tv_.setVideo(sample.time, sample.value);
        } else {
            tv_.setSyncN(sample.time, sample.value);
        }
        return;
    }
    if (net == nets_[0]) {
        // Low is a lit dot; Z stays Z (Not would make it X).
        tv_.setVideo(sample.time,
                     sample.value == LogicValue::Z ? LogicValue::Z : Not(sample.value));
    } else if (net == nets_[1]) {
        tv_.setHorizontalDrive(sample.time, sample.value);
    } else {
        tv_.setVerticalDrive(sample.time, sample.value);
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
