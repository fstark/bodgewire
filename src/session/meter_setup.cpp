#include "session/meter_setup.hpp"

#include <utility>

namespace bw {

void MeterSetup::bind(MeterConsumer& consumer) {
    consumer_ = &consumer;
    follow();
}

void MeterSetup::follow() {
    if (build_->machine == nullptr) {
        return;
    }
    const auto net = mode() == MeterMode::Volts && red_
                         ? build_->machine->probeNet(*build_->circuit, red_->layer, red_->at)
                         : std::nullopt;
    consumer_->touch(*build_->runner, net);
    rail_ = net ? RailVolts(*build_->circuit, *net) : std::nullopt;
}

void MeterSetup::findContinuity() {
    connected_ = red_ && black_ ? std::optional(build_->machine->connected(
                                      red_->layer, red_->at, black_->layer, black_->at))
                                : std::nullopt;
}

void MeterSetup::hold(std::optional<TestLead> red) {
    if (build_->machine == nullptr) {
        return;
    }
    const bool same = red && red_ ? red->at == red_->at && red->layer == red_->layer
                                  : red.has_value() == red_.has_value();
    if (same) {
        return;
    }
    red_ = std::move(red);
    follow();
    findContinuity();
}

bool MeterSetup::clip(TestLead black) {
    if (build_->machine == nullptr || build_->powered) {
        return false;
    }
    black_ = std::move(black);
    findContinuity();
    return true;
}

void MeterSetup::unclip() {
    if (build_->machine == nullptr) {
        return;
    }
    black_.reset();
    findContinuity();
}

void MeterSetup::powerChanged() {
    if (build_->machine == nullptr) {
        return;
    }
    if (build_->powered) {
        black_.reset();
    }
    follow();
    findContinuity();
}

std::optional<double> MeterSetup::volts() const {
    if (mode() != MeterMode::Volts || !red_) {
        return std::nullopt;
    }
    if (!consumer_->touched()) {
        return 0.0;  // bare Copper
    }
    if (rail_) {
        return rail_;
    }
    return consumer_->reading();
}

std::optional<bool> MeterSetup::continuity() const {
    return mode() == MeterMode::Continuity ? connected_ : std::nullopt;
}

}  // namespace bw
