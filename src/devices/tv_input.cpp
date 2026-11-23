#include "devices/tv_input.hpp"

namespace bw {

TvInput::TvInput(Simulator& sim, const TvNets& nets)
    : sim_(sim), nets_(NetsOf(nets)), tv_(SyncOf(nets)) {
    for (const NetId net : nets_) {
        sim_.record(net);
    }
}

void TvInput::update() {
    for (const auto& [input, sample] : sim_.recorder().inOrder(nets_, fedUntil_)) {
        tv_.feed(input, sample.time, sample.value);
    }
    tv_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
