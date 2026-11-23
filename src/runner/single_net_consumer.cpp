#include "runner/single_net_consumer.hpp"

#include "instruments/logic_probe.hpp"
#include "instruments/multimeter.hpp"
#include "runner/runner.hpp"

namespace bw {

template <class State>
void SingleNetConsumer<State>::touch(Runner& runner, std::optional<NetId> net) {
    if (net == touched_) {
        return;
    }
    target_.store(net ? static_cast<std::int64_t>(ToIndex(*net)) : -1, std::memory_order_release);
    if (net) {
        runner.watch(*net);
    }
    if (touched_) {
        runner.unwatch(*touched_);
    }
    touched_ = net;
}

template <class State>
std::span<const NetId> SingleNetConsumer<State>::nets() const {
    const std::int64_t t = target_.load(std::memory_order_acquire);
    if (t < 0) {
        return {};
    }
    nets_[0] = NetId{static_cast<std::uint32_t>(t)};
    return nets_;
}

template <class State>
void SingleNetConsumer<State>::powerOn() {
    state_.reset();
}

template <class State>
void SingleNetConsumer<State>::sample(NetId net, Sample sample) {
    if (static_cast<std::int64_t>(ToIndex(net)) != fedNet_) {
        state_.reset();
        fedNet_ = ToIndex(net);
    }
    state_.feed(sample);
}

template <class State>
void SingleNetConsumer<State>::powerOff() {
    state_.reset();
    published_.store(0, std::memory_order_release);
}

template <class State>
std::int64_t SingleNetConsumer<State>::follow() {
    const std::int64_t t = target_.load(std::memory_order_acquire);
    if (t != fedNet_) {
        state_.reset();
        fedNet_ = t;
    }
    return t;
}

template class SingleNetConsumer<ProbeState>;
template class SingleNetConsumer<MeterState>;

}  // namespace bw
