#include "session/session.hpp"

#include <algorithm>
#include <format>
#include <string>
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
        consumers.push_back(&live->analyzer);
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
        const AnalyzerPhase phase = analyzerPhase();
        if (phase != AnalyzerPhase::Armed && phase != AnalyzerPhase::Filling) {
            analyzer_.armed = false;  // the new consumer would re-arm a finished Capture
        }
        live_.reset();
        auto built = machine_->build();
        Check(built.has_value(), "session: a repaired Machine does not build");
        live_ = makeLive(std::move(*built));
        touchLive();
        configureLive();
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

std::optional<std::size_t> Session::clip(std::string_view reference, std::string_view pin) {
    if (!machine_) {
        return std::nullopt;
    }
    const auto& components = machine_->designCircuit().components;
    const auto component = components.find(reference);
    if (component == components.end()) {
        return std::nullopt;
    }
    const auto index = FindPin(*component->second, pin);
    if (!index) {
        return std::nullopt;
    }
    const std::string number = std::to_string(*index + 1);
    const auto at = machine_->padPosition(reference, number);
    if (!at) {
        return std::nullopt;
    }
    if (const auto channel = clipAt(reference, number)) {
        return channel;
    }
    const auto free = std::ranges::find_if(clips_, [](const auto& c) { return !c; });
    if (free == clips_.end()) {
        return std::nullopt;
    }
    *free = Clip{
        .reference = std::string(reference),
        .pin = number,
        .label = std::format("{}.{} {}", reference, number, component->second->pins()[*index].name),
        .at = *at};
    configureLive();
    return static_cast<std::size_t>(free - clips_.begin());
}

void Session::unclip(std::size_t channel) {
    clips_[channel].reset();
    configureLive();
}

std::optional<std::size_t> Session::clipAt(std::string_view reference, std::string_view pin) const {
    for (std::size_t c = 0; c < kChannels; ++c) {
        if (clips_[c] && clips_[c]->reference == reference && clips_[c]->pin == pin) {
            return c;
        }
    }
    return std::nullopt;
}

void Session::setAnalyzer(const AnalyzerSettings& settings) {
    analyzer_.settings = settings;
    configureLive();
}

void Session::arm() {
    ++analyzer_.arms;
    analyzer_.armed = true;
    configureLive();
}

void Session::disarm() {
    analyzer_.armed = false;
    configureLive();
}

void Session::configureLive() {
    if (!machine_) {
        return;
    }
    for (std::size_t c = 0; c < kChannels; ++c) {
        analyzer_.nets[c] = clips_[c]
                                ? PinNet(*live_->circuit, clips_[c]->reference, clips_[c]->pin)
                                : std::nullopt;
        analyzer_.labels[c] = clips_[c] ? clips_[c]->label : "";
    }
    live_->analyzer.configure(*live_->runner, analyzer_);
}

}  // namespace bw
