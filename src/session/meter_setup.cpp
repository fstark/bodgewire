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
    const auto net = mode_ == MeterMode::Volts && red_
                         ? build_->machine->probeNet(*build_->circuit, red_->layer, red_->at)
                         : std::nullopt;
    consumer_->touch(*build_->runner, net);
    rail_ = net ? RailVolts(*build_->circuit, *net) : std::nullopt;
}

void MeterSetup::copperChanged() {
    connected_ = red_ && black_ ? std::optional(build_->machine->connected(
                                      red_->layer, red_->at, black_->layer, black_->at))
                                : std::nullopt;
}

void MeterSetup::setMode(MeterMode mode) {
    if (build_->machine == nullptr) {
        return;
    }
    mode_ = mode;
    blackNext_ = false;
    follow();
    copperChanged();
}

bool MeterSetup::placeLead(TestLead lead) {
    if (build_->machine == nullptr) {
        return false;
    }
    if (mode_ == MeterMode::Volts) {
        red_ = std::move(lead);
    } else {
        if (build_->powered) {
            return false;
        }
        (blackNext_ ? black_ : red_) = std::move(lead);
        blackNext_ = !blackNext_;
    }
    follow();
    copperChanged();
    return true;
}

void MeterSetup::liftLeads() {
    if (build_->machine == nullptr) {
        return;
    }
    red_.reset();
    black_.reset();
    blackNext_ = false;
    follow();
    copperChanged();
}

std::optional<double> MeterSetup::volts() const {
    if (mode_ != MeterMode::Volts || !red_) {
        return std::nullopt;
    }
    if (!build_->powered || !consumer_->touched()) {
        return 0.0;  // power off; bare Copper
    }
    if (rail_) {
        return rail_;
    }
    return consumer_->reading();
}

std::optional<bool> MeterSetup::continuity() const {
    return mode_ == MeterMode::Continuity && !build_->powered ? connected_ : std::nullopt;
}

}  // namespace bw
