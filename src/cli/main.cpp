#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "board/board_check.hpp"
#include "board/kicad_board.hpp"
#include "board/kicad_netlist.hpp"
#include "devices/bmp.hpp"
#include "devices/tv_input.hpp"
#include "instruments/logic_probe.hpp"
#include "machine/crack.hpp"
#include "machine/machine.hpp"
#include "machine/press.hpp"
#include "machine/repair.hpp"
#include "netlist/builder.hpp"
#include "netlist/units.hpp"
#include "sim/simulator.hpp"
#include "sim/vcd.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

constexpr int kExitNetlistErrors = 1;
constexpr int kExitUsage = 2;

constexpr std::string_view kUsage =
    "usage: bw run <file.bwn|file.bwp> [--time 1ms] [--seed 1] "
    "[--record NET,NET... | --record-all] [--vcd FILE] "
    "[--tv FILE.bmp] [--repair swap:REF,mend:LAYER:X:Y,cut:LAYER:X:Y,"
    "wire:REF.PIN:REF.PIN,unwire:REF.PIN:REF.PIN,unbridge:REF.PIN:REF.PIN,resolder:REF.PIN] "
    "[--probe REF.PIN,...] [--press REF:DOWN:UP,...]\n"
    "       bw export-kicad <file.bwn> -o <file.net>\n"
    "       bw check-board <file.bwn> <file.kicad_pcb>";

struct Options {
    std::string file;
    Time time = 1_ms;
    std::uint64_t seed = 1;
    std::vector<std::string> record;
    bool recordAll = false;
    std::string vcd;
    std::string tv;
    std::vector<std::string> repair;
    std::vector<std::string> probe;
    std::vector<ButtonPress> press;
};

int UsageError(std::string_view message) {
    std::println(stderr, "bw: {}", message);
    std::println(stderr, "{}", kUsage);
    return kExitUsage;
}

std::vector<std::string> SplitCommas(std::string_view text) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = text.find(',', start);
        parts.emplace_back(text.substr(start, comma - start));
        if (comma == std::string_view::npos) {
            return parts;
        }
        start = comma + 1;
    }
}

bool IsPuzzle(std::string_view file) { return file.ends_with(".bwp"); }

// Error message, or nullopt on success.
std::optional<std::string> ParseOptions(std::span<char*> args, Options& options) {
    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (!arg.starts_with("--")) {
            if (!options.file.empty()) {
                return "more than one netlist file";
            }
            options.file = arg;
            continue;
        }
        if (arg == "--record-all") {
            options.recordAll = true;
            continue;
        }
        if (arg != "--time" && arg != "--seed" && arg != "--record" && arg != "--vcd" &&
            arg != "--tv" && arg != "--repair" && arg != "--probe" && arg != "--press") {
            return "unknown option " + std::string(arg);
        }
        if (i + 1 >= args.size()) {
            return std::string(arg) + " needs a value";
        }
        const std::string_view value = args[++i];
        if (arg == "--time") {
            const auto time = ParseDuration(value);
            if (!time) {
                return time.error();
            }
            options.time = *time;
        } else if (arg == "--seed") {
            const auto [end, ec] =
                std::from_chars(value.data(), value.data() + value.size(), options.seed);
            if (ec != std::errc{} || end != value.data() + value.size()) {
                return "bad seed '" + std::string(value) + "'";
            }
        } else if (arg == "--record") {
            options.record = SplitCommas(value);
        } else if (arg == "--tv") {
            options.tv = value;
        } else if (arg == "--repair") {
            options.repair = SplitCommas(value);
        } else if (arg == "--probe") {
            options.probe = SplitCommas(value);
        } else if (arg == "--press") {
            for (const std::string& text : SplitCommas(value)) {
                auto press = ParsePress(text);
                if (!press) {
                    return press.error();
                }
                options.press.push_back(std::move(*press));
            }
        } else {
            options.vcd = value;
        }
    }
    if (options.file.empty()) {
        return "missing netlist file";
    }
    if (!options.repair.empty() && !IsPuzzle(options.file)) {
        return "--repair needs a .bwp puzzle file";
    }
    if (options.recordAll && !options.record.empty()) {
        return "--record and --record-all are exclusive";
    }
    return std::nullopt;
}

// Diagnostics printed; nullptr on errors.
std::unique_ptr<LoadedCircuit> Load(const std::string& file) {
    auto result = LoadNetlistFile(file);
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            std::println(stderr, "{}", Format(d));
        }
        return nullptr;
    }
    for (const Diagnostic& d : (*result)->warnings) {
        std::println(stderr, "{}", Format(d));
    }
    return std::move(*result);
}

// Applies `repair` ("swap:U3", "mend:F.Cu:52.30:41.00", "cut:...", "wire:U1.14:U13.6",
// "unwire:...", "unbridge:...", "resolder:U15.13"); error message, or nullopt.
std::optional<std::string> Repair(Machine& machine, std::string_view repair) {
    const auto parsed = ParseRepair(repair);
    if (!parsed) {
        return parsed.error();
    }
    if (const auto* swap = std::get_if<SwapRepair>(&*parsed)) {
        if (!machine.swap(swap->reference)) {
            return "unknown reference '" + swap->reference + "'";
        }
        return std::nullopt;
    }
    if (const auto* mend = std::get_if<MendRepair>(&*parsed)) {
        if (!machine.mend(mend->layer, mend->at)) {
            std::println(stderr, "bw: no crack near {}", FormatPoint(mend->layer, mend->at));
        }
        return std::nullopt;
    }
    bool done = false;
    if (const auto* cut = std::get_if<CutRepair>(&*parsed)) {
        done = machine.cut(cut->layer, cut->at);
    } else if (const auto* wire = std::get_if<WireRepair>(&*parsed)) {
        done = machine.addWire(wire->a, wire->b);
    } else if (const auto* unwire = std::get_if<UnwireRepair>(&*parsed)) {
        done = machine.removeWire(unwire->a, unwire->b);
    } else if (const auto* resolder = std::get_if<ResolderRepair>(&*parsed)) {
        done = machine.resolder(resolder->pin);
    } else {
        const auto& unbridge = std::get<UnbridgeRepair>(*parsed);
        done = machine.removeBridge(unbridge.a, unbridge.b);
    }
    if (!done) {
        return "cannot apply '" + std::string(repair) + "'";
    }
    return std::nullopt;
}

// Diagnostics printed; nullptr on errors (`usage` set for a bad --repair).
std::unique_ptr<LoadedCircuit> LoadPuzzle(const Options& options,
                                          std::optional<std::string>& usage) {
    auto machine = LoadPuzzleFile(options.file);
    if (!machine) {
        for (const Diagnostic& d : machine.error()) {
            std::println(stderr, "{}", Format(d));
        }
        return nullptr;
    }
    for (const std::string& repair : options.repair) {
        if ((usage = Repair(*machine, repair))) {
            return nullptr;
        }
    }
    auto result = machine->build();
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            std::println(stderr, "{}", Format(d));
        }
        return nullptr;
    }
    return std::move(*result);
}

int ExportKicad(std::span<char*> args) {
    if (args.size() != 5 || std::string_view(args[3]) != "-o") {
        return UsageError("expected: export-kicad <file.bwn> -o <file.net>");
    }
    const auto loaded = Load(args[2]);
    if (loaded == nullptr) {
        return kExitNetlistErrors;
    }
    if (const auto problems = BoardNetlistProblems(*loaded); !problems.empty()) {
        for (const std::string& problem : problems) {
            std::println(stderr, "bw: {}", problem);
        }
        return kExitNetlistErrors;
    }
    std::ofstream out(args[4], std::ios::binary);
    if (!out) {
        return UsageError("cannot write " + std::string(args[4]));
    }
    WriteKicadNetlist(out, *loaded, std::filesystem::path(args[2]).filename().string());
    return 0;
}

int CheckBoardFile(std::span<char*> args) {
    if (args.size() != 4) {
        return UsageError("expected: check-board <file.bwn> <file.kicad_pcb>");
    }
    const auto loaded = Load(args[2]);
    if (loaded == nullptr) {
        return kExitNetlistErrors;
    }
    const auto board = ReadKicadBoardFile(args[3]);
    if (!board) {
        for (const Diagnostic& d : board.error()) {
            std::println(stderr, "{}", Format(d));
        }
        return kExitNetlistErrors;
    }
    auto problems = BoardNetlistProblems(*loaded);
    for (std::string& problem : CheckBoard(*loaded, *board)) {
        problems.push_back(std::move(problem));
    }
    for (const std::string& problem : problems) {
        std::println("{}", problem);
    }
    if (!problems.empty()) {
        return kExitNetlistErrors;
    }
    std::println("ok");
    return 0;
}

int Run(std::span<char*> args) {
    Options options;
    if (const auto error = ParseOptions(args, options)) {
        return UsageError(*error);
    }

    std::optional<std::string> usage;
    const auto result = IsPuzzle(options.file) ? LoadPuzzle(options, usage) : Load(options.file);
    if (usage) {
        return UsageError(*usage);
    }
    if (result == nullptr) {
        return kExitNetlistErrors;
    }
    LoadedCircuit& loaded = *result;

    std::vector<NetId> probed;
    for (const std::string& probe : options.probe) {
        const std::size_t dot = probe.find('.');
        const auto net = dot == std::string::npos
                             ? std::nullopt
                             : PinNet(loaded, probe.substr(0, dot), probe.substr(dot + 1));
        if (!net) {
            return UsageError("unknown pin '" + probe + "'");
        }
        probed.push_back(*net);
    }

    struct PressEdge {
        Time at;
        Button* button;
        bool down;
    };
    std::vector<PressEdge> edges;
    for (const ButtonPress& press : options.press) {
        Button* button = ButtonOf(loaded, press.reference);
        if (button == nullptr) {
            return UsageError("'" + press.reference + "' is not a button");
        }
        edges.push_back({press.down, button, true});
        edges.push_back({press.up, button, false});
    }
    std::ranges::stable_sort(edges, {}, &PressEdge::at);

    std::vector<NetId> recorded;
    if (options.recordAll) {
        for (const auto& [name, net] : loaded.nets) {
            if (name != "GND" && name != "VCC") {
                recorded.push_back(net);
            }
        }
    }
    for (const std::string& name : options.record) {
        const auto it = loaded.nets.find(name);
        if (it == loaded.nets.end()) {
            return UsageError("unknown net '" + name + "'");
        }
        recorded.push_back(it->second);
    }

    std::ofstream vcd;
    if (!options.vcd.empty()) {
        vcd.open(options.vcd, std::ios::binary);
        if (!vcd) {
            return UsageError("cannot write " + options.vcd);
        }
    }

    std::ofstream tvFile;
    std::optional<NetId> tvVideo, tvSyncN;
    if (!options.tv.empty()) {
        tvVideo = ConnectorNet(loaded, "VIDEO", "VIDEO");
        tvSyncN = ConnectorNet(loaded, "VIDEO", "SYNC_N");
        if (!tvVideo || !tvSyncN) {
            std::println(stderr, "bw: netlist has no VIDEO connector");
            return kExitNetlistErrors;
        }
        tvFile.open(options.tv, std::ios::binary);
        if (!tvFile) {
            return UsageError("cannot write " + options.tv);
        }
    }

    Simulator sim(loaded.circuit);
    sim.powerOn(options.seed);
    for (const NetId net : recorded) {
        sim.record(net);
    }
    std::unique_ptr<TvInput> tv;
    if (tvFile.is_open()) {
        tv = std::make_unique<TvInput>(sim, *tvVideo, *tvSyncN);
    }
    std::vector<LogicProbe> probes;
    for (const NetId net : probed) {
        probes.emplace_back(sim, net);
    }
    const auto start = std::chrono::steady_clock::now();
    if (tv || !probes.empty() || !edges.empty()) {
        const Time end = sim.now() + options.time;
        std::size_t next = 0;
        while (sim.now() < end) {
            Time stop = std::min(end, sim.now() + 1_ms);
            if (next < edges.size()) {
                stop = std::min(stop, edges[next].at);
            }
            sim.runUntil(stop);
            for (; next < edges.size() && edges[next].at == sim.now(); ++next) {
                edges[next].button->setPressed(edges[next].down);
                sim.wake(edges[next].button->id());
            }
            if (tv) {
                tv->update();
            }
            for (LogicProbe& probe : probes) {
                probe.update();
            }
            if (!vcd.is_open()) {
                sim.trimRecordings();
            }
        }
    } else {
        sim.runFor(options.time);
    }
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - start;

    if (vcd.is_open()) {
        WriteVcd(vcd, loaded.circuit, sim.recorder(), sim.now());
    }
    if (tv) {
        WriteGreyscaleBmp(tvFile, tv->tv().completedFrame());
    }

    const double simulated = static_cast<double>(sim.now()) / static_cast<double>(kPicosPerSecond);
    std::println("simulated_s {}", simulated);
    std::println("wall_s {}", wall.count());
    std::println("ratio {}", wall.count() > 0 ? simulated / wall.count() : 0.0);
    std::println("applied_events {}", sim.stats().appliedEvents);
    std::println("evaluations {}", sim.stats().evaluations);
    if (tv) {
        std::println("tv_locked {}", tv->tv().isLocked());
    }
    for (std::size_t i = 0; i < probes.size(); ++i) {
        std::println("probe {} {}", options.probe[i], FormatLamps(probes[i].lamps()));
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::span args(argv, static_cast<std::size_t>(argc));
    const std::string_view command = args.size() >= 2 ? args[1] : "";
    if (command == "run") {
        return Run(args);
    }
    if (command == "export-kicad") {
        return ExportKicad(args);
    }
    if (command == "check-board") {
        return CheckBoardFile(args);
    }
    return UsageError("expected 'run', 'export-kicad' or 'check-board'");
}
