#include "ui/bench_setup.hpp"

#include <algorithm>
#include <format>

#include "instruments/logic_analyzer.hpp"
#include "instruments/oscilloscope.hpp"
#include "netlist/parser.hpp"

namespace bw {

std::expected<void, std::string> ApplyBenchOptions(Session& session, const BenchOptions& options) {
    if (options.probe) {
        const auto pin = ParsePinRef(*options.probe);
        std::optional<Point> at;
        if (session.machine() && pin) {
            at = session.machine()->padPosition(pin->reference, pin->pin);
        }
        if (!at) {
            return std::unexpected(std::format("unknown pin '{}'", *options.probe));
        }
        session.touch(ProbeTip{*at, Layer::Front});
    }
    if (options.contents) {
        const std::string& reference = options.contents->first;
        if (!session.machine() || !session.machine()->contents(reference)) {  // a ROM on a Board
            return std::unexpected(std::format("no memory chip '{}'", reference));
        }
    }
    AnalyzerSettings analyzer;
    if (options.sample) {
        analyzer.period = *options.sample;
    }
    for (const std::string& item : options.clip) {
        const std::size_t colon = item.find(':');
        const auto pin = ParsePinRef(std::string_view(item).substr(0, colon));
        const auto trigger = colon == std::string::npos
                                 ? std::optional(TriggerCondition::Any)
                                 : ParseTrigger(std::string_view(item).substr(colon + 1));
        const auto channel = pin ? session.clip(pin->reference, pin->pin) : std::nullopt;
        if (!trigger || !channel) {
            return std::unexpected(std::format("unknown pin '{}'", item));
        }
        analyzer.trigger[*channel] = *trigger;
    }
    if (!options.clip.empty() || !options.group.empty()) {
        session.setAnalyzer(analyzer);
    }
    for (const std::string& item : options.group) {
        const std::size_t colon = item.find(':');
        const auto pin = ParsePinRef(std::string_view(item).substr(0, colon));
        const auto clipped =
            pin ? std::optional(session.clipGroup(pin->reference, pin->pin)) : std::nullopt;
        const bool ok = clipped && *clipped &&
                        (colon == std::string::npos ||
                         ((*clipped)->group &&
                          session.setGroupTrigger(*(*clipped)->group, item.substr(colon + 1))));
        if (!ok) {
            return std::unexpected(std::format(
                "bad group '{}'{}", item, clipped && !*clipped ? ": " + clipped->error() : ""));
        }
    }
    if (!options.clip.empty() || !options.group.empty()) {
        session.arm();
    }
    for (const std::string& item : options.hook) {
        const auto pin = ParsePinRef(item);
        if (!pin || !session.hook(pin->reference, pin->pin)) {
            return std::unexpected(std::format("unknown pin '{}'", item));
        }
    }
    if (options.timebase || options.delay) {
        ScopeSettings scope = session.scopeSettings();
        scope.timebase = options.timebase.value_or(scope.timebase);
        scope.delay = options.delay.value_or(scope.delay);
        if (scope.delay > kMaxDelayDivisions * scope.timebase) {
            return std::unexpected(std::format("delay over {} divisions", kMaxDelayDivisions));
        }
        session.setScope(scope);
    }
    if (options.meter.size() == 2) {  // red then black, before the power-on: none for these
        session.setMeterMode(MeterMode::Continuity);
    }
    for (const std::string& item : options.meter) {
        const auto pin = ParsePinRef(item);
        const auto lead = pin ? session.padLead(pin->reference, pin->pin) : std::nullopt;
        if (!lead) {
            return std::unexpected(std::format("unknown pin '{}'", item));
        }
        const bool chip = std::ranges::any_of(
            session.machine()->designCircuit().parts, [&](const LoadedPart& part) {
                return part.reference == lead->reference && part.info->kind == PartKind::Chip;
            });
        session.placeLead({lead->at, Layer::Front, lead->label + (chip ? " leg" : "")});
    }
    for (const std::string& text : options.repair) {
        const auto repair = ParseRepair(text);
        std::optional<std::string> error;
        if (!repair) {
            error = repair.error();
        } else if (session.repair(*repair) != RepairResult::Done) {
            error = !session.machine() ? std::string("no Board to repair")
                                       : "cannot apply '" + text + "'";
        }
        if (error) {
            return std::unexpected(std::format("{}", *error));
        }
    }
    if (options.ask && !session.machine()) {
        return std::unexpected(std::string("no Board to repair"));
    }
    if (!options.mark.empty() && !session.machine()) {
        return std::unexpected(std::string("no Board to mark"));
    }
    if (options.tv && session.tv() == nullptr) {
        return std::unexpected(std::string("this Machine has no TV"));
    }
    for (const BenchObject object : options.pop) {
        const bool has = object == BenchObject::Tv ? session.tv() != nullptr
                         : object == BenchObject::Keyboard
                             ? !session.keyboards().empty() || !session.matrixKeyboards().empty()
                         : object == BenchObject::Knob   ? !session.knobs().empty()
                         : object == BenchObject::Manual ? true
                                                         : session.machine() != nullptr;
        if (!has) {
            return std::unexpected(
                std::format("this Machine has no {}", object == BenchObject::Tv         ? "TV"
                                                      : object == BenchObject::Keyboard ? "keyboard"
                                                      : object == BenchObject::Knob
                                                          ? "paddle controller"
                                                          : "Board for the instruments"));
        }
    }
    for (const KnobSetting& knob : options.knob) {
        if (!session.setKnob(knob.reference, knob.turn)) {
            return std::unexpected(std::format("'{}' is not a paddle controller", knob.reference));
        }
    }
    for (const Typing& typing : options.type) {
        if (!std::ranges::contains(session.keyboards(), typing.reference) &&
            !std::ranges::contains(session.matrixKeyboards(), typing.reference)) {
            return std::unexpected(std::format("'{}' is not a keyboard", typing.reference));
        }
        if (typing.untypable && std::ranges::contains(session.keyboards(), typing.reference)) {
            return std::unexpected(
                std::format("cannot type the text on '{}', a keyboard", typing.reference));
        }
    }
    return {};
}

}  // namespace bw
