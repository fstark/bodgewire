#include "session/session.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "sim/check.hpp"

namespace bw {

namespace {

// The Net of a Pin the netlist connects; nullopt for an unconnected Pin (PinNet would give its
// private Net) or an empty name.
std::optional<NetId> ConnectedNet(const LoadedCircuit& loaded, std::string_view reference,
                                  std::string_view pin) {
    const auto component = loaded.components.find(reference);
    if (pin.empty() || component == loaded.components.end()) {
        return std::nullopt;
    }
    const auto pins = component->second->pins();
    const auto spec = std::ranges::find(pins, pin, &PinSpec::name);
    if (spec == pins.end()) {
        return std::nullopt;
    }
    const auto index = static_cast<PinIndex>(std::distance(pins.begin(), spec));
    const bool connected = std::ranges::any_of(loaded.connections, [&](const Connection& c) {
        return c.reference == reference && c.pin == index;
    });
    return connected ? PinNet(loaded, reference, pin) : std::nullopt;
}

}  // namespace

std::optional<TvNets> FindTvNets(const LoadedCircuit& circuit) {
    const auto video = ConnectorNet(circuit, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(circuit, "VIDEO", "SYNC_N");
    if (video && syncN) {
        return CompositeVideo{*video, *syncN};
    }
    const auto videoN = ConnectorNet(circuit, "MONITOR", "VIDEO_N");
    const auto hdrive = ConnectorNet(circuit, "MONITOR", "HDRIVE");
    const auto vdrive = ConnectorNet(circuit, "MONITOR", "VDRIVE");
    if (videoN && hdrive && vdrive) {
        return SeparateDrives{*videoN, *hdrive, *vdrive};
    }
    return std::nullopt;
}

bool HasTv(const LoadedCircuit& circuit) { return FindTvNets(circuit).has_value(); }

Session::Session(std::optional<Machine> machine, std::unique_ptr<LoadedCircuit> circuit)
    : machine_(machine ? std::make_unique<Machine>(std::move(*machine)) : nullptr) {
    build_->machine = machine_.get();
    if (machine_) {
        drawn_ = machine_->board();
    }
    live_ = makeLive(std::move(circuit));
    bindBuild();
}

void Session::bindBuild() {
    build_->circuit = live_->circuit.get();
    build_->runner = live_->runner.get();
    inputs_.bind(machine_ ? machine_->designCircuit() : *live_->circuit, *live_->circuit,
                 *live_->runner);
    configureLive();
}

Session::Session(Session&&) noexcept = default;
Session::~Session() = default;

std::unique_ptr<Session::Live> Session::makeLive(std::unique_ptr<LoadedCircuit> circuit) const {
    auto live = std::make_unique<Live>();
    live->circuit = std::move(circuit);
    std::vector<Consumer*> consumers;
    if (const auto tvNets = FindTvNets(*live->circuit)) {
        live->tv.emplace(*tvNets);
        consumers.push_back(&*live->tv);
    }
    if (machine_) {
        consumers.push_back(&live->probe);
        consumers.push_back(&live->analyzer);
        consumers.push_back(&live->meter);
        consumers.push_back(&live->scope);
        std::vector<LedNets> nets;
        for (const LoadedPart& part : live->circuit->parts) {
            for (std::size_t i = 0; i < part.info->lamps.size(); ++i) {
                const Lamp& lamp = part.info->lamps[i];
                const std::array anodes{
                    ConnectedNet(*live->circuit, part.reference, lamp.anodes[0]),
                    ConnectedNet(*live->circuit, part.reference, lamp.anodes[1])};
                const std::array cathodes{
                    ConnectedNet(*live->circuit, part.reference, lamp.cathodes[0]),
                    ConnectedNet(*live->circuit, part.reference, lamp.cathodes[1])};
                // An unconnected lamp: never lit.
                if ((anodes[0] || anodes[1]) && (cathodes[0] || cathodes[1])) {
                    nets.push_back({part.reference, i, anodes, cathodes});
                }
            }
        }
        if (!nets.empty()) {
            live->leds.emplace(std::move(nets));
            consumers.push_back(&*live->leds);
        }
    }
    live->runner = std::make_unique<Runner>(live->circuit->circuit, consumers);
    live->runner->setSpeed(speed_);
    return live;
}

void Session::powerOn(std::uint64_t seed, std::optional<Time> pauseAt) {
    if (dirty_) {
        analyzer_.beforeRebuild();
        live_.reset();
        auto built = machine_->build();
        Check(built.has_value(), "session: a repaired Machine does not build");
        live_ = makeLive(std::move(*built));
        bindBuild();
        dirty_ = false;
    }
    if (pauseAt) {
        live_->runner->pauseAt(*pauseAt);
    }
    live_->runner->powerOn(seed);
    powered_ = build_->powered = true;
    inputs_.poweredOn();
}

void Session::powerOff() {
    inputs_.poweredOff();
    live_->runner->powerOff();
    powered_ = build_->powered = false;
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
    // A mend always places its solder: done even with no crack under it (the tool must not tell).
    const bool done = machine_->apply(repair) || std::holds_alternative<MendRepair>(repair);
    if (const auto* mend = std::get_if<MendRepair>(&repair)) {
        blobs_.push_back({mend->layer, mend->at});
    }
    if (!done) {
        return RepairResult::Unknown;
    }
    log_.push_back(FormatRepair(repair));
    dirty_ = true;
    meter_.copperChanged();
    return RepairResult::Done;
}

RepairResult Session::undoRepairs() {
    if (!machine_) {
        return RepairResult::Unknown;
    }
    if (powered_) {
        return RepairResult::Refused;
    }
    machine_->undoRepairs();
    log_.clear();
    blobs_.clear();
    dirty_ = true;
    meter_.copperChanged();
    return RepairResult::Done;
}

std::optional<PadLead> Session::padLead(std::string_view reference, std::string_view pin) const {
    return machine_ ? PadLeadOf(*machine_, reference, pin) : std::nullopt;
}

std::vector<LedLight> Session::leds() const {
    std::vector<LedLight> lights;
    if (!machine_) {
        return lights;
    }
    for (const LoadedPart& part : machine_->designCircuit().parts) {
        for (std::size_t i = 0; i < part.info->lamps.size(); ++i) {
            float brightness = 0;
            if (powered_ && live_->leds) {
                const auto leds = live_->leds->leds();
                const auto it = std::ranges::find_if(leds, [&](const LedNets& led) {
                    return led.reference == part.reference && led.lamp == i;
                });
                if (it != leds.end()) {
                    brightness = live_->leds->brightness(
                        static_cast<std::size_t>(std::distance(leds.begin(), it)));
                }
            }
            lights.push_back({part.reference, part.info->lamps[i].shape, brightness});
        }
    }
    return lights;
}

void Session::configureLive() {
    analyzer_.bind(live_->analyzer);
    scope_.bind(live_->scope);
    probe_.bind(live_->probe);
    meter_.bind(live_->meter);
}

}  // namespace bw
