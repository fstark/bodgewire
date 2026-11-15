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
    const auto videoN = ConnectorNet(*live->circuit, "MONITOR", "VIDEO_N");
    const auto hdrive = ConnectorNet(*live->circuit, "MONITOR", "HDRIVE");
    const auto vdrive = ConnectorNet(*live->circuit, "MONITOR", "VDRIVE");
    if (video && syncN) {
        live->tv.emplace(*video, *syncN);
        consumers.push_back(&*live->tv);
    } else if (videoN && hdrive && vdrive) {
        live->tv.emplace(SeparateDrives{*videoN, *hdrive, *vdrive});
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
    typed_.clear();
}

void Session::powerOff() {
    for (const std::string& reference : pressed_) {
        if (Switch* sw = SwitchOf(*live_->circuit, reference)) {
            sw->setPressed(false);
        }
    }
    pressed_.clear();
    for (const auto& [reference, key] : matrixHeld_) {
        if (MatrixKeyboard* matrix = MatrixKeyboardOf(*live_->circuit, reference)) {
            matrix->setHeld(key, false);
        }
    }
    matrixHeld_.clear();
    for (const std::string& reference : keyboards()) {
        if (Keyboard* keyboard = KeyboardOf(*live_->circuit, reference)) {
            keyboard->dropWaiting();
        }
    }
    for (const std::string& reference : matrixKeyboards()) {
        if (MatrixKeyboard* matrix = MatrixKeyboardOf(*live_->circuit, reference)) {
            matrix->dropWaiting();
        }
    }
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
    leaveGroup(channel);
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

std::expected<ClipGroupResult, std::string> Session::clipGroup(std::string_view reference,
                                                               std::string_view pin) {
    const auto lead = padLead(reference, pin);
    if (!lead) {
        return std::unexpected("no such pin");
    }
    const auto pins = machine_->designCircuit().components.find(reference)->second->pins();
    const auto joins = [](const PinSpec& spec) {
        return spec.kind != PinKind::Power && spec.kind != PinKind::NoConnect;
    };
    const PinSpec& clicked = pins[static_cast<std::size_t>(std::stoi(lead->pin) - 1)];
    const auto stem = joins(clicked) ? StemOf(clicked.name) : std::nullopt;
    std::vector<std::pair<std::uint32_t, std::string>> members;  // stem index, pin number
    for (std::size_t p = 0; stem && p < pins.size(); ++p) {
        const auto other = StemOf(pins[p].name);
        if (joins(pins[p]) && other && other->stem == stem->stem) {
            members.emplace_back(other->index, std::to_string(p + 1));
        }
    }
    if (members.size() < 2) {
        const auto channel = clip(reference, lead->pin);
        if (!channel) {
            return std::unexpected(std::format("all {} channels used", kChannels));
        }
        return ClipGroupResult{*channel, std::nullopt};
    }
    std::ranges::sort(members);
    std::string name = stem->stem;
    std::erase(name, '#');
    name = std::format("{} {}", reference, name);
    const auto needed = static_cast<std::size_t>(std::ranges::count_if(
        members, [&](const auto& m) { return !clipAt(reference, m.second).has_value(); }));
    const auto free =
        static_cast<std::size_t>(std::ranges::count_if(clips_, [](const auto& c) { return !c; }));
    if (needed > free) {
        return std::unexpected(
            std::format("group {} needs {} channels, {} free", name, needed, free));
    }
    std::vector<std::size_t> channels;
    for (const auto& [index, number] : members) {
        channels.push_back(*clip(reference, number));
    }
    for (const std::size_t channel : channels) {
        leaveGroup(channel);
    }
    groups_.push_back({std::move(name), std::move(channels)});
    return ClipGroupResult{*clipAt(reference, lead->pin), groups_.size() - 1};
}

void Session::unclipGroup(std::size_t group) {
    const std::vector<std::size_t> channels = std::move(groups_[group].channels);
    groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(group));
    for (const std::size_t channel : channels) {
        clips_[channel].reset();
        analyzer_.settings.trigger[channel] = TriggerCondition::Any;
    }
    configureLive();
}

std::optional<std::size_t> Session::group(std::vector<std::size_t> channels) {
    if (channels.size() < 2) {
        return std::nullopt;
    }
    std::vector<std::pair<std::uint32_t, std::size_t>> stems;  // stem index, Channel
    for (const std::size_t channel : channels) {
        if (channel >= kChannels || !clips_[channel] ||
            std::ranges::count(channels, channel) != 1) {
            return std::nullopt;
        }
        const PadLead& clip = *clips_[channel];
        const auto pins = machine_->designCircuit().components.at(clip.reference)->pins();
        const auto stem = StemOf(pins[static_cast<std::size_t>(std::stoi(clip.pin) - 1)].name);
        if (stem) {
            stems.emplace_back(stem->index, channel);
        }
    }
    std::ranges::sort(stems);
    const bool byStem =
        stems.size() == channels.size() &&
        std::ranges::adjacent_find(stems, {}, &std::pair<std::uint32_t, std::size_t>::first) ==
            stems.end();
    if (byStem) {
        channels.clear();
        for (const auto& [index, channel] : stems) {
            channels.push_back(channel);
        }
    } else {
        std::ranges::reverse(channels);  // given most significant first
    }
    for (const std::size_t channel : channels) {
        leaveGroup(channel);
    }
    int n = 1;
    while (std::ranges::any_of(
        groups_, [&](const ChannelGroup& g) { return g.name == std::format("group {}", n); })) {
        ++n;
    }
    groups_.push_back({std::format("group {}", n), std::move(channels)});
    return groups_.size() - 1;
}

void Session::ungroup(std::size_t group) {
    groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(group));
}

bool Session::setGroupTrigger(std::size_t group, std::string_view pattern) {
    const auto& channels = groups_[group].channels;
    const auto conditions = ParseBusPattern(pattern, channels.size());
    if (!conditions) {
        return false;
    }
    for (std::size_t b = 0; b < channels.size(); ++b) {
        analyzer_.settings.trigger[channels[b]] = (*conditions)[b];
    }
    configureLive();
    return true;
}

std::string Session::groupTrigger(std::size_t group) const {
    std::vector<TriggerCondition> conditions;
    for (const std::size_t channel : groups_[group].channels) {
        conditions.push_back(analyzer_.settings.trigger[channel]);
    }
    return FormatBusPattern(conditions);
}

std::optional<std::size_t> Session::groupOf(std::size_t channel) const {
    for (std::size_t g = 0; g < groups_.size(); ++g) {
        if (std::ranges::contains(groups_[g].channels, channel)) {
            return g;
        }
    }
    return std::nullopt;
}

void Session::leaveGroup(std::size_t channel) {
    const auto group = groupOf(channel);
    if (!group) {
        return;
    }
    std::erase(groups_[*group].channels, channel);
    if (groups_[*group].channels.size() < 2) {
        groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(*group));
    }
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

std::vector<std::string> Session::switches() const {
    std::vector<std::string> references;
    const LoadedCircuit& circuit = machine_ ? machine_->designCircuit() : *live_->circuit;
    for (const LoadedPart& part : circuit.parts) {
        if (part.info->kind == PartKind::Switch) {
            references.push_back(part.reference);
        }
    }
    return references;
}

std::vector<SwitchKey> Session::switchKeys() const {
    std::vector<SwitchKey> keys;
    const LoadedCircuit& circuit = machine_ ? machine_->designCircuit() : *live_->circuit;
    for (const std::string& reference : switches()) {
        if (const Switch* sw = SwitchOf(circuit, reference); sw && !sw->pcKey().empty()) {
            keys.push_back({reference, std::string(sw->pcKey())});
        }
    }
    return keys;
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
    if (Switch* sw = SwitchOf(*live_->circuit, reference)) {
        sw->setPressed(pressed);
        live_->runner->wake(sw->id(), PressJitter());
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

std::vector<std::string> Session::keyboards() const {
    std::vector<std::string> references;
    const LoadedCircuit& circuit = machine_ ? machine_->designCircuit() : *live_->circuit;
    for (const LoadedPart& part : circuit.parts) {
        if (part.info->name == "KEYBOARD") {
            references.push_back(part.reference);
        }
    }
    return references;
}

bool Session::type(std::string_view reference, char32_t key) {
    if (MatrixKeyboard* matrix = MatrixKeyboardOf(*live_->circuit, reference)) {
        if (!powered_ || !matrix->type(key)) {
            return false;
        }
        live_->runner->wake(matrix->id(), PressJitter());
        return true;
    }
    const auto code = KeyboardCode(key);
    return code && typeCode(reference, *code);
}

std::vector<std::string> Session::matrixKeyboards() const {
    std::vector<std::string> references;
    const LoadedCircuit& circuit = machine_ ? machine_->designCircuit() : *live_->circuit;
    for (const LoadedPart& part : circuit.parts) {
        if (part.info->name == "MATRIXKB") {
            references.push_back(part.reference);
        }
    }
    return references;
}

void Session::setMatrixKeyHeld(std::string_view reference, std::size_t key, bool held) {
    MatrixKeyboard* matrix = MatrixKeyboardOf(*live_->circuit, reference);
    if (!powered_ || matrix == nullptr || key >= Bw2001Keys().size() ||
        held == matrixKeyHeld(reference, key)) {
        return;
    }
    if (held) {
        matrixHeld_.emplace(std::string(reference), key);
    } else {
        matrixHeld_.erase({std::string(reference), key});
    }
    matrix->setHeld(key, held);
    live_->runner->wake(matrix->id(), PressJitter());
}

bool Session::matrixKeyHeld(std::string_view reference, std::size_t key) const {
    return matrixHeld_.contains({std::string(reference), key});
}

bool Session::matrixKeyDown(std::string_view reference, std::size_t key) const {
    if (matrixKeyHeld(reference, key)) {
        return true;
    }
    const MatrixKeyboard* matrix = MatrixKeyboardOf(*live_->circuit, reference);
    return powered_ && matrix != nullptr && matrix->scripted(key);
}

bool Session::typeCode(std::string_view reference, std::uint8_t code) {
    const std::vector<std::string> all = keyboards();
    if (!powered_ || code > kKeyboardLastCode || std::ranges::find(all, reference) == all.end()) {
        return false;
    }
    Keyboard* keyboard = KeyboardOf(*live_->circuit, reference);
    if (keyboard == nullptr || !keyboard->type(code)) {
        return false;
    }
    live_->runner->wake(keyboard->id(), PressJitter());
    std::string& sent = typed_[std::string(reference)];
    sent.push_back(static_cast<char>(code));
    if (sent.size() > kTypedShown) {
        sent.erase(0, sent.size() - kTypedShown);
    }
    return true;
}

std::string Session::typed(std::string_view reference) const {
    const auto it = typed_.find(reference);
    return it == typed_.end() ? std::string() : it->second;
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
