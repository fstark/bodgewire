#include "session/session.hpp"

#include <utility>
#include <variant>

#include "sim/check.hpp"

namespace bw {

Session::Session(std::optional<Machine> machine, std::unique_ptr<LoadedCircuit> circuit)
    : machine_(std::move(machine)) {
    if (machine_) {
        drawn_ = machine_->board();
    }
    live_ = makeLive(std::move(circuit));
}

Session::Session(Session&&) noexcept = default;
Session::~Session() = default;

std::unique_ptr<Session::Live> Session::makeLive(std::unique_ptr<LoadedCircuit> circuit) const {
    auto live = std::make_unique<Live>();
    live->circuit = std::move(circuit);
    const auto video = ConnectorNet(*live->circuit, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(*live->circuit, "VIDEO", "SYNC_N");
    std::vector<Consumer*> consumers;
    if (video && syncN) {
        live->tv.emplace(*video, *syncN);
        consumers.push_back(&*live->tv);
    }
    if (machine_) {
        consumers.push_back(&live->probe);
    }
    live->runner = std::make_unique<Runner>(live->circuit->circuit, consumers);
    live->runner->setSpeed(speed_);
    return live;
}

void Session::touchLive() {
    live_->probe.touch(*live_->runner,
                       tip_ ? machine_->probeNet(*live_->circuit, tip_->layer, tip_->at)
                            : std::nullopt);
}

void Session::powerOn(std::uint64_t seed, std::optional<Time> pauseAt) {
    if (dirty_) {
        live_.reset();
        auto built = machine_->build();
        Check(built.has_value(), "session: a repaired Machine does not build");
        live_ = makeLive(std::move(*built));
        touchLive();
        dirty_ = false;
    }
    if (pauseAt) {
        live_->runner->pauseAt(*pauseAt);
    }
    live_->runner->powerOn(seed);
    powered_ = true;
}

void Session::powerOff() {
    live_->runner->powerOff();
    powered_ = false;
}

void Session::setSpeed(Speed speed) {
    speed_ = speed;
    live_->runner->setSpeed(speed);
}

RepairResult Session::repair(const RepairAction& repair) {
    if (!machine_) {
        return RepairResult::Unknown;
    }
    if (powered_) {
        return RepairResult::Refused;
    }
    if (const auto* swap = std::get_if<SwapRepair>(&repair)) {
        if (!machine_->swap(swap->reference)) {
            return RepairResult::Unknown;
        }
    } else {
        const auto& mend = std::get<MendRepair>(repair);
        machine_->mend(mend.layer, mend.at);
        blobs_.push_back({mend.layer, mend.at});
    }
    log_.push_back(FormatRepair(repair));
    dirty_ = true;
    return RepairResult::Done;
}

void Session::touch(std::optional<ProbeTip> tip) {
    if (!machine_) {
        return;
    }
    tip_ = tip;
    touchLive();
}

ProbeLamps Session::lamps() const { return powered_ ? live_->probe.lamps() : ProbeLamps{}; }

}  // namespace bw
