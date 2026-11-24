#include "view/bench_options.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <ranges>
#include <system_error>

#include "board/kicad_board.hpp"
#include "instruments/logic_analyzer.hpp"
#include "instruments/oscilloscope.hpp"
#include "netlist/units.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

template <class T>
using Names = std::span<const std::pair<std::string_view, T>>;

template <class T>
std::optional<T> Named(Names<T> names, std::string_view name) {
    const auto it = std::ranges::find(names, name, &std::pair<std::string_view, T>::first);
    return it != names.end() ? std::optional(it->second) : std::nullopt;
}

constexpr std::array<std::pair<std::string_view, BenchObject>, 7> kObjects{{
    {"manual", BenchObject::Manual},
    {"tv", BenchObject::Tv},
    {"keyboard", BenchObject::Keyboard},
    {"knob", BenchObject::Knob},
    {"analyzer", BenchObject::Analyzer},
    {"scope", BenchObject::Scope},
    {"meter", BenchObject::Meter},
}};

constexpr std::array<std::pair<std::string_view, Owner>, 9> kOwners{{
    {"probe", Owner::Probe},
    {"swap", Owner::Puller},
    {"iron", Owner::Iron},
    {"cut", Owner::Cutters},
    {"wire", Owner::Wire},
    {"pencil", Owner::Pencil},
    {"analyzer", Owner::Analyzer},
    {"scope", Owner::Scope},
    {"meter", Owner::Meter},
}};

constexpr std::array<std::pair<std::string_view, MarkShape>, 3> kShapes{{
    {"dot", MarkShape::Dot},
    {"cross", MarkShape::Cross},
    {"check", MarkShape::Check},
}};

constexpr std::array<std::pair<std::string_view, Divider>, 5> kDividers{{
    {"user", Divider::User},
    {"service", Divider::Service},
    {"datasheet", Divider::Datasheets},
    {"repairs", Divider::Repairs},
    {"giveup", Divider::GiveUp},  // a Random fault's bench only (P8d)
}};

constexpr std::array<std::pair<std::string_view, RunSwitch>, 3> kRuns{{
    {"pause", RunSwitch::Pause},
    {"1x", RunSwitch::RealTime},
    {"max", RunSwitch::Max},
}};

constexpr std::array<std::pair<std::string_view, ContentsView>, 3> kContentsViews{{
    {"hex", ContentsView::Hex},
    {"code", ContentsView::Code},
    {"glyphs", ContentsView::Glyphs},
}};

// "check:12.5:30" (Board millimetres, as --repair cut:): a pencil mark's shape and place.
std::optional<std::pair<MarkShape, Point>> ParseMark(std::string_view text) {
    const auto fields = Split(text, ':');
    if (fields.size() != 3) {
        return std::nullopt;
    }
    const auto shape = Named<MarkShape>(kShapes, fields[0]);
    const auto x = ParseMillimetres(fields[1]);
    const auto y = ParseMillimetres(fields[2]);
    if (!shape || !x || !y) {
        return std::nullopt;
    }
    return std::pair{*shape, Point{*x, *y}};
}

// "192.6:122:12" (Board millimetres, as --mark): a box W wide centred on X, Y, as tall.
std::optional<Bounds> ParseZoom(std::string_view text) {
    const auto fields = Split(text, ':');
    if (fields.size() != 3) {
        return std::nullopt;
    }
    const auto x = ParseMillimetres(fields[0]);
    const auto y = ParseMillimetres(fields[1]);
    const auto width = ParseMillimetres(fields[2]);
    if (!x || !y || !width || *width <= 0) {
        return std::nullopt;
    }
    const std::int64_t half = *width / 2;
    return Bounds{{*x - half, *y - half}, {*x + half, *y + half}};
}

// "+2us", "-1.5us", "2us", "0": ps.
std::optional<Time> ParseSignedDuration(std::string_view text) {
    if (text == "0") {
        return 0;
    }
    const bool negative = text.starts_with('-');
    if (negative || text.starts_with('+')) {
        text.remove_prefix(1);
    }
    const auto duration = ParseDuration(text);
    if (!duration) {
        return std::nullopt;
    }
    return negative ? -*duration : *duration;
}

// "1us" or "1us:+20us": a 1-2-5 time per division, centred on the Trigger or the time given.
std::optional<TimeView> ParseView(std::string_view text) {
    const std::size_t colon = text.find(':');
    const auto perDivision = ParseDuration(text.substr(0, colon));
    if (!perDivision || StepAtLeast(*perDivision) != *perDivision) {
        return std::nullopt;
    }
    const auto centre = colon == std::string_view::npos
                            ? std::optional<Time>(0)
                            : ParseSignedDuration(text.substr(colon + 1));
    if (!centre) {
        return std::nullopt;
    }
    return CentredOn(*perDivision, *centre);
}

using Result = std::expected<void, std::string>;

Result Bad() { return std::unexpected(std::string()); }

template <class T>
Result Set(std::optional<T>& slot, std::optional<T> value) {
    if (!value) {
        return Bad();
    }
    slot = std::move(value);
    return {};
}

// Each comma-separated item through `parse` into `out`.
template <class T, class Parse>
Result Each(std::vector<T>& out, std::string_view text, Parse parse) {
    for (const std::string_view item : Split(text, ',')) {
        auto value = parse(item);
        if (!value) {
            return Bad();
        }
        out.push_back(std::move(*value));
    }
    return {};
}

std::vector<std::string> Items(std::string_view text) {
    return std::ranges::to<std::vector<std::string>>(Split(text, ','));
}

struct Flag {
    std::string_view name;   // "--probe"
    std::string_view value;  // the usage's placeholder; "" for a flag without one
    bool launcher = false;   // taken without a file
    Result (*apply)(Options& options, std::string_view value);
};

// In the usage's order.
const std::array kFlags{
    Flag{"--machines", "DIR", true,
         [](Options& o, std::string_view v) -> Result {
             o.machines = v;
             return {};
         }},
    Flag{"--visit", "FILE,...", true,
         [](Options& o, std::string_view v) -> Result {
             for (const std::string_view file : Split(v, ',')) {
                 o.visit.emplace_back(file);
             }
             return {};
         }},
    Flag{"--select", "FILE", true,
         [](Options& o, std::string_view v) -> Result {
             o.select = v;
             return {};
         }},
    Flag{"--make", "", true,
         [](Options& o, std::string_view) -> Result {
             o.make = true;
             return {};
         }},
    Flag{"--seed", "N", true,
         [](Options& o, std::string_view v) -> Result {
             std::uint64_t seed = 0;
             const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), seed);
             if (ec != std::errc{} || end != v.data() + v.size()) {
                 return Bad();
             }
             o.seed = seed;
             return {};
         }},
    Flag{"--probe", "REF.PIN", false,
         [](Options& o, std::string_view v) -> Result {
             o.bench.probe = v;
             return {};
         }},
    Flag{"--repair",
         "swap:REF,mend:LAYER:X:Y,cut:LAYER:X:Y,wire:REF.PIN:REF.PIN,unwire:REF.PIN:REF.PIN,"
         "unbridge:REF.PIN:REF.PIN,resolder:REF.PIN",
         false,
         [](Options& o, std::string_view v) -> Result {
             o.bench.repair = Items(v);
             return {};
         }},
    Flag{"--clip", "REF.PIN[:0|1|r|f],...", false,
         [](Options& o, std::string_view v) -> Result {
             o.bench.clip = Items(v);
             return {};
         }},
    Flag{"--group", "REF.PIN[:E0xx],...", false,
         [](Options& o, std::string_view v) -> Result {
             o.bench.group = Items(v);
             return {};
         }},
    Flag{"--sample", "100ns", false,
         [](Options& o, std::string_view v) -> Result {
             const auto period = ParseDuration(v);
             if (!period ||
                 std::ranges::find(kAnalyzerPeriods, *period) == kAnalyzerPeriods.end()) {
                 return Bad();
             }
             o.bench.sample = *period;
             return {};
         }},
    Flag{"--view", "1us[:+20us]", false,
         [](Options& o, std::string_view v) { return Set(o.bench.view, ParseView(v)); }},
    Flag{
        "--marker", "+2us", false,
        [](Options& o, std::string_view v) { return Set(o.bench.marker, ParseSignedDuration(v)); }},
    Flag{
        "--cursor", "+3.7us", false,
        [](Options& o, std::string_view v) { return Set(o.bench.cursor, ParseSignedDuration(v)); }},
    Flag{"--hook", "REF.PIN,...", false,
         [](Options& o, std::string_view v) -> Result {
             o.bench.hook = Items(v);
             return o.bench.hook.size() <= kScopeChannels ? Result{} : Bad();
         }},
    Flag{"--timebase", "1us", false,
         [](Options& o, std::string_view v) -> Result {
             const auto timebase = ParseDuration(v);
             if (!timebase || !TimebaseIndex(*timebase)) {
                 return Bad();
             }
             o.bench.timebase = *timebase;
             return {};
         }},
    Flag{"--delay", "10us", false,
         [](Options& o, std::string_view v) -> Result {
             const auto delay = ParseDuration(v);
             if (!delay || *delay < 0) {
                 return Bad();
             }
             o.bench.delay = *delay;
             return {};
         }},
    Flag{"--meter", "REF.PIN[,REF.PIN]", false,
         [](Options& o, std::string_view v) -> Result {
             o.bench.meter = Items(v);
             return o.bench.meter.size() <= 2 ? Result{} : Bad();
         }},
    Flag{"--press", "REF:DOWN:UP,...", false,
         [](Options& o, std::string_view v) { return Each(o.bench.press, v, ParsePress); }},
    Flag{"--knob", "REF:N%,...", false,
         [](Options& o, std::string_view v) { return Each(o.bench.knob, v, ParseKnob); }},
    Flag{"--type", "REF:START:TEXT", false,
         [](Options& o, std::string_view v) -> Result {
             auto typing = ParseTyping(v);  // not split at commas: the text may hold them
             if (!typing) {
                 return std::unexpected(typing.error());
             }
             o.bench.type.push_back(std::move(*typing));
             return {};
         }},
    Flag{"--roms", "DIR", false,
         [](Options& o, std::string_view v) -> Result {
             o.roms = v;
             return {};
         }},
    Flag{"--manual", "user|service|datasheet|repairs|giveup", false,
         [](Options& o, std::string_view v) {
             return Set(o.bench.manual, Named<Divider>(kDividers, v));
         }},
    Flag{"--tv", "hpos:N%,vpos:N%,bright:N%,contrast:N%,off,underscan", false,
         [](Options& o, std::string_view v) { return Set(o.bench.tv, ParseTvSettings(v)); }},
    Flag{"--loupe", "X%:Y%", false,
         [](Options& o, std::string_view v) { return Set(o.bench.loupe, ParseLoupe(v)); }},
    Flag{"--zoom", "X:Y:W", false,
         [](Options& o, std::string_view v) { return Set(o.bench.zoom, ParseZoom(v)); }},
    Flag{"--look", "real|plain", false,
         [](Options& o, std::string_view v) -> Result {
             if (v != "real" && v != "plain") {
                 return Bad();
             }
             o.bench.plainLook = v == "plain";
             return {};
         }},
    Flag{"--focus", "probe|swap|iron|cut|wire|pencil|analyzer|scope|meter", false,
         [](Options& o, std::string_view v) {
             return Set(o.bench.focus, Named<Owner>(kOwners, v));
         }},
    Flag{"--mark", "dot|cross|check:X:Y,...", false,
         [](Options& o, std::string_view v) { return Each(o.bench.mark, v, ParseMark); }},
    Flag{"--pop", "manual|tv|keyboard|knob|analyzer|scope|meter,...", false,
         [](Options& o, std::string_view v) {
             return Each(o.bench.pop, v,
                         [](std::string_view name) { return Named<BenchObject>(kObjects, name); });
         }},
    Flag{"--away", "tv|keyboard|knob,...", false,
         [](Options& o, std::string_view v) {
             return Each(o.bench.away, v, [](std::string_view name) {
                 const auto object = Named<BenchObject>(kObjects, name);
                 const bool slot = object == BenchObject::Tv || object == BenchObject::Keyboard ||
                                   object == BenchObject::Knob;
                 return slot ? object : std::nullopt;
             });
         }},
    Flag{"--run", "pause|1x|max", false,
         [](Options& o, std::string_view v) -> Result {
             const auto run = Named<RunSwitch>(kRuns, v);
             if (!run) {
                 return Bad();
             }
             o.bench.run = *run;
             return {};
         }},
    Flag{"--side", "front|back", false,
         [](Options& o, std::string_view v) -> Result {
             if (v != "front" && v != "back") {
                 return Bad();
             }
             o.bench.back = v == "back";
             return {};
         }},
    Flag{"--ask", "REPAIR", false,
         [](Options& o, std::string_view v) -> Result {
             auto repair = ParseRepair(v);
             if (!repair) {
                 return Bad();
             }
             o.bench.ask = std::move(*repair);
             return {};
         }},
    Flag{"--leave", "", false,
         [](Options& o, std::string_view) -> Result {
             o.bench.leave = true;
             return {};
         }},
    Flag{"--reset", "", false,
         [](Options& o, std::string_view) -> Result {
             o.bench.reset = true;
             return {};
         }},
    Flag{"--binder-height", "N%", false,
         [](Options& o, std::string_view v) -> Result {
             float value = 0;  // percent: the stretch's range is past ParsePercent's 0-100
             const auto [end, error] = std::from_chars(v.data(), v.data() + v.size(), value);
             if (error != std::errc{} || std::string_view(end, v.data() + v.size()) != "%" ||
                 value < kMinStretch * 100 || value > kMaxStretch * 100) {
                 return Bad();
             }
             o.bench.binderHeight = value / 100;
             return {};
         }},
    Flag{"--brief", "DX:DY", false,
         [](Options& o, std::string_view v) -> Result {
             const auto fields = Split(v, ':');
             float x = 0, y = 0;
             const auto number = [](std::string_view text, float& out) {
                 const char* last = text.data() + text.size();
                 return !text.empty() && std::from_chars(text.data(), last, out).ptr == last;
             };
             if (fields.size() != 2 || !number(fields[0], x) || !number(fields[1], y)) {
                 return Bad();
             }
             o.bench.brief = std::pair{x, y};
             return {};
         }},
    Flag{"--contents", "REF[:hex|code|glyphs]", false,
         [](Options& o, std::string_view v) -> Result {
             const std::size_t colon = v.find(':');
             std::optional<ContentsView> view;
             if (colon != std::string_view::npos) {
                 view = Named<ContentsView>(kContentsViews, v.substr(colon + 1));
                 if (!view) {
                     return Bad();
                 }
             }
             o.bench.contents.emplace(std::string(v.substr(0, colon)), view);
             return {};
         }},
    Flag{"--show-fault", "", false,
         [](Options& o, std::string_view) -> Result {
             o.bench.showFault = true;
             return {};
         }},
    Flag{"--debug", "", false,
         [](Options& o, std::string_view) -> Result {
             o.bench.debug = true;
             return {};
         }},
    Flag{"--screenshot", "OUT.png", true,
         [](Options& o, std::string_view v) -> Result {
             o.screenshot = v;
             return {};
         }},
    Flag{"--after", "1s", true,
         [](Options& o, std::string_view v) -> Result {
             const auto duration = ParseDuration(v);
             if (!duration) {
                 return Bad();
             }
             o.after = std::chrono::nanoseconds(*duration / 1000);
             return {};
         }},
};

}  // namespace

std::string Usage() {
    std::string usage = "usage: bw_ui [file.bwn|file.bwp|random.bwr]";
    for (const Flag& flag : kFlags) {
        usage += " [" + std::string(flag.name);
        if (!flag.value.empty()) {
            usage += " " + std::string(flag.value);
        }
        usage += "]";
    }
    return usage;
}

std::expected<Options, std::string> ParseOptions(std::span<const std::string_view> args) {
    Options options;
    if (!args.empty() && !args.front().starts_with("--")) {
        options.file = args.front();
        args = args.subspan(1);
    }
    bool after = false;
    bool benchFlag = false;  // a flag only a bench takes: a usage error without a file (P7a)
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto flag = std::ranges::find(kFlags, args[i], &Flag::name);
        if (flag == kFlags.end()) {
            return std::unexpected(std::string());
        }
        benchFlag = benchFlag || !flag->launcher;
        after = after || flag->name == "--after";
        std::string_view value;
        if (!flag->value.empty()) {
            if (i + 1 >= args.size()) {
                return std::unexpected(std::string());
            }
            value = args[++i];
        }
        if (auto applied = flag->apply(options, value); !applied) {
            return std::unexpected(std::move(applied.error()));
        }
    }
    const BenchOptions& bench = options.bench;
    if ((after && !options.screenshot) || (benchFlag && !options.file) ||
        (!options.visit.empty() && options.file) ||
        (!options.visit.empty() && !options.screenshot) || (bench.leave && bench.ask) ||
        (bench.reset && (bench.leave || bench.ask)) ||
        (options.select && (options.file || !options.visit.empty()))) {
        return std::unexpected(std::string());
    }
    // A Random fault's (P8d): its seed needs a Vetting Script to make one for, its Give up page
    // a bench on one.
    const auto script = [](const std::filesystem::path& file) {
        return file.extension() == ".bwr";
    };
    const bool randomBench = options.file && script(*options.file);
    const bool randomSelected = options.select && script(*options.select);
    if ((options.make && !randomSelected) ||
        (options.seed && !randomBench && !options.make &&
         std::ranges::none_of(options.visit, script)) ||
        ((bench.showFault || bench.manual == Divider::GiveUp) && !randomBench)) {
        return std::unexpected(std::string());
    }
    return options;
}

}  // namespace bw
