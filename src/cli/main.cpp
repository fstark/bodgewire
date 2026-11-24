#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "asm/assembler.hpp"
#include "board/board_check.hpp"
#include "board/footprint.hpp"
#include "board/kicad_board.hpp"
#include "board/kicad_netlist.hpp"
#include "board/kicad_pcb_writer.hpp"
#include "board/place_rules.hpp"
#include "board/placer.hpp"
#include "devices/bmp.hpp"
#include "devices/tv_input.hpp"
#include "instruments/logic_probe.hpp"
#include "machine/crack.hpp"
#include "machine/input_edges.hpp"
#include "machine/knob_setting.hpp"
#include "machine/machine.hpp"
#include "machine/press.hpp"
#include "machine/repair.hpp"
#include "machine/typing.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "netlist/units.hpp"
#include "runner/led_input.hpp"
#include "session/session.hpp"
#include "sim/simulator.hpp"
#include "sim/vcd.hpp"
#include "text/text.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

constexpr int kExitNetlistErrors = 1;
constexpr int kExitUsage = 2;

constexpr std::string_view kUsage =
    "usage: bw run <file.bwn|file.bwp> [--time 1ms] [--seed 1] "
    "[--record NET,NET... | --record-all] [--vcd FILE] "
    "[--tv FILE.bmp] [--leds] [--repair swap:REF,mend:LAYER:X:Y,cut:LAYER:X:Y,"
    "wire:REF.PIN:REF.PIN,unwire:REF.PIN:REF.PIN,unbridge:REF.PIN:REF.PIN,resolder:REF.PIN] "
    "[--probe REF.PIN,...] [--press REF:DOWN:UP,...] [--knob REF:N%[:TIME],...] "
    "[--type REF:START:TEXT] [--roms DIR]\n"
    "       bw export-kicad <file.bwn> -o <file.net>\n"
    "       bw check-board <file.bwn> <file.kicad_pcb>\n"
    "       bw place <file.bwn> <file.place> -o <file.kicad_pcb> [--seed 1] [--tries 4] "
    "[--footprints DIR] [--force]\n"
    "       bw asm <file.asm> -o <file.rom|file.bin>";

struct Options {
    std::string file;
    Time time = 1_ms;
    std::uint64_t seed = 1;
    std::vector<std::string> record;
    bool recordAll = false;
    std::string vcd;
    std::string tv;
    bool leds = false;  // --leds: each lamp's brightness at the end, a line a lamp
    std::vector<std::string> repair;
    std::vector<std::string> probe;
    std::vector<ButtonPress> press;
    std::vector<KnobSetting> knob;
    std::vector<Typing> type;
    std::optional<std::string> roms;  // --roms DIR: ROM files named by reference (P5q2)
};

int UsageError(std::string_view message) {
    std::println(stderr, "bw: {}", message);
    std::println(stderr, "{}", kUsage);
    return kExitUsage;
}

bool IsPuzzle(std::string_view file) { return file.ends_with(".bwp"); }

// A lamp as --leds prints it: "D1" for an LED, "DS1.a" to "DS1.g" and "DS1.dp" for a digit's.
std::string LampName(const LoadedCircuit& loaded, const LedNets& led) {
    constexpr std::array<std::string_view, 9> kSuffix{"",   ".a", ".b", ".c", ".d",
                                                      ".e", ".f", ".g", ".dp"};  // by LampShape
    const auto part = std::ranges::find(loaded.parts, led.reference, &LoadedPart::reference);
    const LampShape shape = part->info->lamps[led.lamp].shape;
    return led.reference + std::string(kSuffix[static_cast<std::size_t>(shape)]);
}

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
        if (arg == "--leds") {
            options.leds = true;
            continue;
        }
        if (arg != "--time" && arg != "--seed" && arg != "--record" && arg != "--vcd" &&
            arg != "--tv" && arg != "--repair" && arg != "--probe" && arg != "--press" &&
            arg != "--knob" && arg != "--type" && arg != "--roms") {
            return "unknown option " + std::string(arg);
        }
        if (i + 1 >= args.size()) {
            return std::string(arg) + " needs a value";
        }
        const std::string_view value = args[++i];
        if (arg == "--roms") {
            options.roms = std::string(value);
        } else if (arg == "--time") {
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
            options.record = std::ranges::to<std::vector<std::string>>(Split(value, ','));
        } else if (arg == "--tv") {
            options.tv = value;
        } else if (arg == "--repair") {
            options.repair = std::ranges::to<std::vector<std::string>>(Split(value, ','));
        } else if (arg == "--probe") {
            options.probe = std::ranges::to<std::vector<std::string>>(Split(value, ','));
        } else if (arg == "--press") {
            for (const std::string_view text : Split(value, ',')) {
                auto press = ParsePress(text);
                if (!press) {
                    return press.error();
                }
                options.press.push_back(std::move(*press));
            }
        } else if (arg == "--knob") {
            for (const std::string_view text : Split(value, ',')) {
                auto knob = ParseKnob(text);
                if (!knob) {
                    return knob.error();
                }
                options.knob.push_back(std::move(*knob));
            }
        } else if (arg == "--type") {
            auto typing = ParseTyping(value);  // not split at commas: the text may hold them
            if (!typing) {
                return typing.error();
            }
            options.type.push_back(std::move(*typing));
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

// Diagnostics printed; nullptr on errors. `roms`: a ROM directory (ReadNetlistFile).
std::unique_ptr<LoadedCircuit> Load(const std::string& file,
                                    const std::optional<std::string>& roms = std::nullopt) {
    LoadResult result =
        LoadNetlistFile(file, roms ? std::optional<std::filesystem::path>(*roms) : std::nullopt);
    if (!result) {
        PrintDiagnostics(result.error());
        return nullptr;
    }
    PrintDiagnostics((*result)->warnings);
    return std::move(*result);
}

// Applies `repair` ("swap:U3", "mend:F.Cu:52.30:41.00", "cut:...", "wire:U1.14:U13.6",
// "unwire:...", "unbridge:...", "resolder:U15.13"); error message, or nullopt.
std::optional<std::string> Repair(Machine& machine, std::string_view repair) {
    const auto parsed = ParseRepair(repair);
    if (!parsed) {
        return parsed.error();
    }
    if (machine.apply(*parsed)) {
        return std::nullopt;
    }
    if (const auto* mend = std::get_if<MendRepair>(&*parsed)) {  // the solder is placed anyway
        std::println(stderr, "bw: no crack near {}", FormatPoint(mend->layer, mend->at));
        return std::nullopt;
    }
    if (const auto* swap = std::get_if<SwapRepair>(&*parsed)) {
        return "unknown reference '" + swap->reference + "'";
    }
    return "cannot apply '" + std::string(repair) + "'";
}

// Diagnostics printed; nullptr on errors (`usage` set for a bad --repair).
std::unique_ptr<LoadedCircuit> LoadPuzzle(const Options& options,
                                          std::optional<std::string>& usage) {
    auto machine = LoadPuzzleFile(options.file, options.roms);
    if (!machine) {
        PrintDiagnostics(machine.error());
        return nullptr;
    }
    for (const std::string& repair : options.repair) {
        if ((usage = Repair(*machine, repair))) {
            return nullptr;
        }
    }
    if (!options.repair.empty()) {  // the Puzzle author's check (P8c)
        std::println(stderr, "bw: Puzzle {}completed", machine->completed() ? "" : "not ");
    }
    auto result = machine->build();
    if (!result) {
        PrintDiagnostics(result.error());
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

int AsmCommand(std::span<char*> args) {
    if (args.size() != 5 || std::string_view(args[3]) != "-o") {
        return UsageError("asm needs <file.asm> -o <file.rom|file.bin>");
    }
    const std::string_view out = args[4];
    const auto rom = AssembleFile(args[2]);
    if (!rom) {
        std::println(stderr, "bw: {}", rom.error());
        return kExitNetlistErrors;
    }
    // A .bin is the raw image, a byte a word; anything else the .rom text with its listing.
    std::ofstream file{std::string(out), std::ios::binary};
    if (out.ends_with(".bin")) {
        file.write(reinterpret_cast<const char*>(rom->words.data()),
                   static_cast<std::streamsize>(rom->words.size()));
    } else {
        file << rom->text;
    }
    if (!file) {
        std::println(stderr, "bw: cannot write '{}'", out);
        return kExitNetlistErrors;
    }
    std::println("{}: {} bytes at ${:04X}-${:04X}", out, rom->words.size(), rom->base,
                 rom->base + rom->words.size() - 1);
    return 0;
}

int PlaceCommand(std::span<char*> args) {
    std::vector<std::string_view> files;
    std::string out;
    std::filesystem::path footprints = BW_FOOTPRINT_DIR;
    PlaceOptions options;
    bool force = false;
    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--force") {
            force = true;
            continue;
        }
        if (!arg.starts_with("-")) {
            files.push_back(arg);
            continue;
        }
        if (i + 1 >= args.size()) {
            return UsageError(std::string(arg) + " needs a value");
        }
        const std::string_view value = args[++i];
        if (arg == "-o") {
            out = value;
        } else if (arg == "--footprints") {
            footprints = value;
        } else if (arg == "--seed" || arg == "--tries") {
            std::uint64_t number = 0;
            const auto [end, ec] =
                std::from_chars(value.data(), value.data() + value.size(), number);
            if (ec != std::errc{} || end != value.data() + value.size() ||
                (arg == "--tries" && (number < 1 || number > 1000))) {
                return UsageError("bad " + std::string(arg.substr(2)) + " '" + std::string(value) +
                                  "'");
            }
            if (arg == "--seed") {
                options.seed = number;
            } else {
                options.tries = static_cast<std::int32_t>(number);
            }
        } else {
            return UsageError("unknown option " + std::string(arg));
        }
    }
    if (files.size() != 2 || out.empty()) {
        return UsageError("expected: place <file.bwn> <file.place> -o <file.kicad_pcb>");
    }
    const auto loaded = Load(std::string(files[0]));
    if (loaded == nullptr) {
        return kExitNetlistErrors;
    }
    if (const auto problems = BoardNetlistProblems(*loaded); !problems.empty()) {
        for (const std::string& problem : problems) {
            std::println(stderr, "bw: {}", problem);
        }
        return kExitNetlistErrors;
    }
    const std::string rulesFile(files[1]);
    const auto rulesText = ReadTextFile(rulesFile);
    if (!rulesText) {
        return UsageError("cannot read " + rulesFile);
    }
    const auto rules = ParsePlaceRules(*rulesText, rulesFile);
    if (!rules) {
        PrintDiagnostics(rules.error());
        return kExitNetlistErrors;
    }
    std::vector<std::string_view> packages;
    for (const LoadedPart& part : loaded->parts) {
        packages.push_back(part.info->package);
    }
    const auto library = LoadFootprints(footprints, packages);
    if (!library) {
        PrintDiagnostics(library.error());
        return kExitNetlistErrors;
    }
    if (const auto existing = ReadTextFile(out);
        existing && !force &&
        (existing->contains("(segment") || existing->contains("(arc") ||
         existing->contains("(via"))) {
        std::println(stderr, "bw: {} has tracks; --force replaces it", out);
        return kExitUsage;
    }
    const auto placement = PlaceBoard(*loaded, *rules, rulesFile, *library, options);
    if (!placement) {
        PrintDiagnostics(placement.error());
        return kExitNetlistErrors;
    }
    std::ostringstream board;
    WriteKicadPcb(board, *loaded, *placement);
    std::ofstream file(out, std::ios::binary);
    if (!(file << board.str())) {
        return UsageError("cannot write " + out);
    }
    std::filesystem::path project(out);
    project.replace_extension(".kicad_pro");
    if (!std::filesystem::exists(project)) {
        std::ofstream projectFile(project, std::ios::binary);
        WriteKicadProject(projectFile, project.stem().string());
        if (!projectFile) {
            return UsageError("cannot write " + project.string());
        }
    }
    std::println("wire_length_m {:.2f}", static_cast<double>(placement->wireLength) / 1e9);
    for (std::int32_t r = 0; r < placement->rows; ++r) {
        std::string line;
        for (std::int32_t c = 0; c < placement->columns; ++c) {
            const std::string& reference =
                placement->grid[static_cast<std::size_t>(r * placement->columns + c)];
            line += std::format("{:>4} ", reference.empty() ? "--" : reference);
        }
        line.pop_back();
        std::println("{}", line);
    }
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
        PrintDiagnostics(board.error());
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
    const auto result =
        IsPuzzle(options.file) ? LoadPuzzle(options, usage) : Load(options.file, options.roms);
    if (usage) {
        return UsageError(*usage);
    }
    if (result == nullptr) {
        return kExitNetlistErrors;
    }
    LoadedCircuit& loaded = *result;

    std::vector<NetId> probed;
    for (const std::string& probe : options.probe) {
        const auto pin = ParsePinRef(probe);
        const auto net = pin ? PinNet(loaded, pin->reference, pin->pin) : std::nullopt;
        if (!net) {
            return UsageError("unknown pin '" + probe + "'");
        }
        probed.push_back(*net);
    }

    // Button presses, typed keys and timed knobs, applied at their simulated times.
    const auto edges = InputEdges(loaded, options.press, options.knob, options.type);
    if (!edges) {
        return UsageError(edges.error());
    }

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
    std::optional<TvNets> tvNets;
    if (!options.tv.empty()) {
        tvNets = FindTvNets(loaded);
        if (!tvNets) {
            std::println(stderr, "bw: netlist has no VIDEO or MONITOR connector");
            return kExitNetlistErrors;
        }
        tvFile.open(options.tv, std::ios::binary);
        if (!tvFile) {
            return UsageError("cannot write " + options.tv);
        }
    }

    std::vector<LedNets> ledNets;
    if (options.leds) {
        ledNets = FindLedNets(loaded);
        if (ledNets.empty()) {
            std::println(stderr, "bw: netlist has no LED or digit");
            return kExitNetlistErrors;
        }
    }

    Simulator sim(loaded.circuit);
    sim.powerOn(options.seed);
    for (const NetId net : recorded) {
        sim.record(net);
    }
    std::unique_ptr<TvInput> tv;
    if (tvFile.is_open()) {
        tv = std::make_unique<TvInput>(sim, *tvNets);
    }
    std::unique_ptr<LedInput> leds;
    if (options.leds) {
        leds = std::make_unique<LedInput>(sim, std::move(ledNets));
    }
    std::vector<LogicProbe> probes;
    for (const NetId net : probed) {
        probes.emplace_back(sim, net);
    }
    const auto start = std::chrono::steady_clock::now();
    if (tv || leds || !probes.empty() || !edges->empty()) {
        std::size_t next = 0;
        RunTo(sim, *edges, next, sim.now() + options.time, [&] {
            if (tv) {
                tv->update();
            }
            if (leds) {
                leds->update();
            }
            for (LogicProbe& probe : probes) {
                probe.update();
            }
            if (!vcd.is_open()) {
                sim.trimRecordings();
            }
            return true;
        });
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
    if (leds) {  // as the eye sees each lamp at the end: 0.00 dark to 1.00 fully lit
        for (std::size_t i = 0; i < leds->leds().size(); ++i) {
            std::println("led {} {:.2f}", LampName(loaded, leds->leds()[i]), leds->brightness(i));
        }
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
    if (command == "place") {
        return PlaceCommand(args);
    }
    if (command == "asm") {
        return AsmCommand(args);
    }
    return UsageError("expected 'run', 'export-kicad', 'check-board', 'place' or 'asm'");
}
