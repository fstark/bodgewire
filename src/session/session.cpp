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

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

// A hand's timing is not tied to the 1 ms slices: without this, every hold lasts whole
// milliseconds and a 1 MHz counter cycling every 6 µs keeps its parity (the dice only rolls even).
Time PressJitter() {
    const auto ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return static_cast<Time>(ns % 1'000'000) * 1000;  // ps, within 1 ms
}

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
        consumers.push_back(&live->meter);
        consumers.push_back(&live->scope);
        std::vector<LedNets> nets;
        for (const LoadedPart& part : live->circuit->parts) {
            for (std::size_t i = 0; i < part.info->lamps.size(); ++i) {
                const Lamp& lamp = part.info->lamps[i];
                const auto cathode = ConnectedNet(*live->circuit, part.reference, lamp.cathode);
                const std::array anodes{
                    ConnectedNet(*live->circuit, part.reference, lamp.anodes[0]),
                    ConnectedNet(*live->circuit, part.reference, lamp.anodes[1])};
                if (cathode && (anodes[0] || anodes[1])) {  // an unconnected lamp: never lit
                    nets.push_back({part.reference, i, anodes, *cathode});
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

void Session::touchLive() {
    live_->probe.touch(*live_->runner,
                       tip_ ? machine_->probeNet(*live_->circuit, tip_->layer, tip_->at)
                            : std::nullopt);
    const auto meterNet = meterMode_ == MeterMode::Volts && red_
                              ? machine_->probeNet(*live_->circuit, red_->layer, red_->at)
                              : std::nullopt;
    live_->meter.touch(*live_->runner, meterNet);
    meterRail_ = meterNet ? RailVolts(*live_->circuit, *meterNet) : std::nullopt;
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
        for (const auto& [reference, turn] : knobs_) {
            if (Paddle* paddle = PaddleOf(*live_->circuit, reference)) {
                paddle->setKnob(turn);
            }
        }
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
    for (const std::string& reference : pressed_) {
        if (Button* button = ButtonOf(*live_->circuit, reference)) {
            button->setPressed(false);
        }
    }
    pressed_.clear();
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
    const bool done = std::visit(
        Overloaded{[&](const SwapRepair& r) { return machine_->swap(r.reference); },
                   [&](const MendRepair& r) {
                       machine_->mend(r.layer, r.at);
                       blobs_.push_back({r.layer, r.at});
                       return true;
                   },
                   [&](const CutRepair& r) { return machine_->cut(r.layer, r.at); },
                   [&](const WireRepair& r) { return machine_->addWire(r.a, r.b); },
                   [&](const UnwireRepair& r) { return machine_->removeWire(r.a, r.b); },
                   [&](const UnbridgeRepair& r) { return machine_->removeBridge(r.a, r.b); },
                   [&](const ResolderRepair& r) { return machine_->resolder(r.pin); }},
        repair);
    if (!done) {
        return RepairResult::Unknown;
    }
    log_.push_back(FormatRepair(repair));
    dirty_ = true;
    recomputeContinuity();
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

std::optional<PadLead> Session::padLead(std::string_view reference, std::string_view pin) const {
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
    std::string number = std::to_string(*index + 1);
    const auto at = machine_->padPosition(reference, number);
    if (!at) {
        return std::nullopt;
    }
    std::string label =
        std::format("{}.{} {}", reference, number, component->second->pins()[*index].name);
    return PadLead{.reference = std::string(reference),
                   .pin = std::move(number),
                   .label = std::move(label),
                   .at = *at};
}

std::optional<std::size_t> Session::clip(std::string_view reference, std::string_view pin) {
    auto lead = padLead(reference, pin);
    if (!lead) {
        return std::nullopt;
    }
    if (const auto channel = clipAt(reference, lead->pin)) {
        return channel;
    }
    const auto free = std::ranges::find_if(clips_, [](const auto& c) { return !c; });
    if (free == clips_.end()) {
        return std::nullopt;
    }
    *free = std::move(*lead);
    configureLive();
    return static_cast<std::size_t>(free - clips_.begin());
}

void Session::unclip(std::size_t channel) {
    clips_[channel].reset();
    analyzer_.settings.trigger[channel] = TriggerCondition::Any;  // a free Channel reads X
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

std::optional<std::size_t> Session::hook(std::string_view reference, std::string_view pin) {
    auto lead = padLead(reference, pin);
    if (!lead) {
        return std::nullopt;
    }
    if (const auto channel = hookAt(reference, lead->pin)) {
        return channel;
    }
    const auto free = std::ranges::find_if(hooks_, [](const auto& h) { return !h; });
    if (free == hooks_.end()) {
        return std::nullopt;
    }
    *free = std::move(*lead);
    configureLive();
    return static_cast<std::size_t>(free - hooks_.begin());
}

void Session::unhook(std::size_t channel) {
    hooks_[channel].reset();
    configureLive();
}

std::optional<std::size_t> Session::hookAt(std::string_view reference, std::string_view pin) const {
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (hooks_[c] && hooks_[c]->reference == reference && hooks_[c]->pin == pin) {
            return c;
        }
    }
    return std::nullopt;
}

std::optional<double> Session::hookRail(std::size_t channel) const {
    return scope_.nets[channel] ? RailVolts(*live_->circuit, *scope_.nets[channel]) : std::nullopt;
}

void Session::setScope(const ScopeSettings& settings) {
    scope_.settings = settings;
    configureLive();
}

void Session::setMeterMode(MeterMode mode) {
    if (!machine_) {
        return;
    }
    meterMode_ = mode;
    blackNext_ = false;
    touchLive();
    recomputeContinuity();
}

bool Session::placeLead(TestLead lead) {
    if (!machine_) {
        return false;
    }
    if (meterMode_ == MeterMode::Volts) {
        red_ = std::move(lead);
    } else {
        if (powered_) {
            return false;
        }
        (blackNext_ ? black_ : red_) = std::move(lead);
        blackNext_ = !blackNext_;
    }
    touchLive();
    recomputeContinuity();
    return true;
}

void Session::liftLeads() {
    if (!machine_) {
        return;
    }
    red_.reset();
    black_.reset();
    blackNext_ = false;
    touchLive();
    recomputeContinuity();
}

std::optional<double> Session::volts() const {
    if (meterMode_ != MeterMode::Volts || !red_) {
        return std::nullopt;
    }
    if (!powered_ || !live_->meter.touched()) {
        return 0.0;  // power off; bare Copper
    }
    if (meterRail_) {
        return meterRail_;
    }
    return live_->meter.reading();
}

std::optional<bool> Session::continuity() const {
    return meterMode_ == MeterMode::Continuity && !powered_ ? connected_ : std::nullopt;
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

std::vector<std::string> Session::buttons() const {
    std::vector<std::string> references;
    const LoadedCircuit& circuit = machine_ ? machine_->designCircuit() : *live_->circuit;
    for (const LoadedPart& part : circuit.parts) {
        if (part.info->kind == PartKind::Button) {
            references.push_back(part.reference);
        }
    }
    return references;
}

void Session::setPressed(std::string_view reference, bool pressed) {
    if (!powered_ || pressed == pressed_.contains(reference)) {
        return;
    }
    if (pressed) {
        pressed_.emplace(reference);
    } else {
        pressed_.erase(pressed_.find(reference));
    }
    if (Button* button = ButtonOf(*live_->circuit, reference)) {
        button->setPressed(pressed);
        live_->runner->wake(button->id(), PressJitter());
    }
}

std::vector<std::string> Session::knobs() const {
    std::vector<std::string> references;
    const LoadedCircuit& circuit = machine_ ? machine_->designCircuit() : *live_->circuit;
    for (const LoadedPart& part : circuit.parts) {
        if (part.info->name == "PADDLE") {
            references.push_back(part.reference);
        }
    }
    return references;
}

bool Session::setKnob(std::string_view reference, double turn) {
    const std::vector<std::string> all = knobs();
    if (std::ranges::find(all, reference) == all.end()) {
        return false;
    }
    turn = std::clamp(turn, 0.0, 1.0);
    knobs_.insert_or_assign(std::string(reference), turn);
    if (Paddle* paddle = PaddleOf(*live_->circuit, reference)) {
        paddle->setKnob(turn);
    }
    return true;
}

double Session::knob(std::string_view reference) const {
    const auto it = knobs_.find(reference);
    return it == knobs_.end() ? 0.0 : it->second;
}

void Session::recomputeContinuity() {
    connected_ =
        red_ && black_
            ? std::optional(machine_->connected(red_->layer, red_->at, black_->layer, black_->at))
            : std::nullopt;
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
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        scope_.nets[c] = hooks_[c] ? PinNet(*live_->circuit, hooks_[c]->reference, hooks_[c]->pin)
                                   : std::nullopt;
    }
    live_->scope.configure(*live_->runner, scope_);
}

}  // namespace bw
