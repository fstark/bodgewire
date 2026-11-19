#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <print>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include "board/kicad_board.hpp"
#include "machine/knob_setting.hpp"
#include "machine/machine.hpp"
#include "machine/press.hpp"
#include "machine/typing.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "netlist/rom_directory.hpp"
#include "netlist/units.hpp"
#include "parts/catalogue.hpp"
#include "session/session.hpp"
#include "ui/analyzer_window.hpp"
#include "ui/bench.hpp"
#include "ui/board_keys.hpp"
#include "ui/board_view.hpp"
#include "ui/brief_window.hpp"
#include "ui/debug_window.hpp"
#include "ui/gpu_render.hpp"
#include "ui/keyboard_window.hpp"
#include "ui/knob_window.hpp"
#include "ui/loaded.hpp"
#include "ui/manual_window.hpp"
#include "ui/matrix_keyboard_window.hpp"
#include "ui/meter_window.hpp"
#include "ui/power_cycle_popup.hpp"
#include "ui/scope_window.hpp"
#include "ui/shelf.hpp"
#include "ui/tv_view.hpp"
#include "ui/tv_window.hpp"
#include "view/focus.hpp"
#include "view/pencil_marks.hpp"
#include "view/power_cycle.hpp"
#include "view/time_view.hpp"
#include "view/tv_controls.hpp"

using namespace bw;

namespace {

constexpr int kExitErrors = 1;
constexpr int kExitUsage = 2;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr std::string_view kUsage =
    "usage: bw_ui <file.bwn|file.bwp> [--probe REF.PIN] "
    "[--repair swap:REF,mend:LAYER:X:Y,cut:LAYER:X:Y,wire:REF.PIN:REF.PIN,unwire:REF.PIN:REF.PIN,"
    "unbridge:REF.PIN:REF.PIN,resolder:REF.PIN] "
    "[--clip REF.PIN[:0|1|r|f],...] [--group REF.PIN[:E0xx],...] [--sample 100ns] "
    "[--view 1us[:+20us]] [--marker +2us] [--cursor +3.7us] "
    "[--hook REF.PIN,...] [--timebase 1us] [--delay 10us] "
    "[--meter REF.PIN] [--press REF:DOWN:UP,...] [--knob REF:N%,...] "
    "[--type REF:START:TEXT] [--roms DIR] [--manual user|service|datasheet|repairs] "
    "[--tv hpos:N%,vpos:N%,bright:N%,contrast:N%,off,underscan] [--look "
    "real|plain] "
    "[--focus probe|swap|iron|cut|wire|pencil|analyzer|scope|meter] "
    "[--mark dot|cross|check:X:Y,...] [--pop manual|tv|keyboard|knob|analyzer|scope|meter,...] "
    "[--away tv|keyboard|knob,...] [--run pause|1x|max] [--side front|back] [--ask REPAIR] "
    "[--binder-height N%] [--brief DX:DY] "
    "[--debug] "
    "[--screenshot OUT.png [--after 1s]]";

struct Options {
    std::filesystem::path file;
    std::optional<std::filesystem::path> screenshot;
    std::chrono::nanoseconds after{std::chrono::seconds(1)};
    std::optional<std::string> probe;  // REF.PIN
    std::vector<std::string> repair;
    std::vector<std::string> clip;     // REF.PIN[:T]
    std::vector<std::string> group;    // REF.PIN[:PATTERN]
    std::optional<Time> sample;        // Analyzer sample period, ps
    std::optional<TimeView> view;      // Analyzer view at its first Capture
    std::optional<Time> marker;        // ps from the Trigger, both instrument windows
    std::optional<Time> cursor;        // a fake hover, ps from the Trigger
    std::optional<std::string> meter;  // REF.PIN: red Test Lead, V mode
    std::vector<std::string> hook;     // REF.PIN, at most kScopeChannels
    std::optional<Time> timebase;      // Oscilloscope ps per division
    std::optional<Time> delay;         // Oscilloscope Trigger delay, ps
    std::vector<ButtonPress> press;
    std::vector<KnobSetting> knob;
    std::optional<std::filesystem::path> roms;  // --roms DIR: ROM files named by reference
    std::vector<Typing> type;
    std::optional<Divider> manual;  // --manual: the binder out at this divider (P6e)
    std::optional<TvControls> tv;   // --tv: the monitor's controls at start (P6f)
    bool plainLook = false;         // --look plain: the specialist's Board, no parts drawn (P6)
    std::optional<Owner> focus;     // --focus: the owner at start (P6c)
    bool debug = false;             // --debug: the Debug window out (P6d)
    std::vector<std::pair<MarkShape, Point>> mark;  // --mark: pencil marks at start (P6h)
    std::vector<BenchObject> pop;   // --pop: out in their own desktop windows at start (P6j)
    std::vector<BenchObject> away;  // --away: the Machine slot's objects put away at start (P6o)
    RunSwitch run = RunSwitch::RealTime;  // --run: the run switch at start (P6o)
    bool back = false;                    // --side back: the Board flipped at start (P6o)
    std::optional<RepairAction> ask;      // --ask: the power-cycle popup open at start (P6o)
    float binderHeight = 1;               // --binder-height: the binder's stretch (P6o)
    std::optional<ImVec2> brief;          // --brief: the note's offset on the Board, px (P6o)
};

std::optional<BenchObject> ParseBenchObject(std::string_view name) {
    constexpr std::array<std::pair<std::string_view, BenchObject>, 7> kNames{{
        {"manual", BenchObject::Manual},
        {"tv", BenchObject::Tv},
        {"keyboard", BenchObject::Keyboard},
        {"knob", BenchObject::Knob},
        {"analyzer", BenchObject::Analyzer},
        {"scope", BenchObject::Scope},
        {"meter", BenchObject::Meter},
    }};
    const auto it =
        std::ranges::find(kNames, name, &std::pair<std::string_view, BenchObject>::first);
    return it != kNames.end() ? std::optional(it->second) : std::nullopt;
}

std::optional<Owner> ParseOwner(std::string_view name) {
    constexpr std::array<std::pair<std::string_view, Owner>, 9> kNames{{
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
    const auto it = std::ranges::find(kNames, name, &std::pair<std::string_view, Owner>::first);
    return it != kNames.end() ? std::optional(it->second) : std::nullopt;
}

// "check:12.5:30" (Board millimetres, as --repair cut:): a pencil mark's shape and place.
std::optional<std::pair<MarkShape, Point>> ParseMark(std::string_view text) {
    constexpr std::array<std::pair<std::string_view, MarkShape>, 3> kShapes{{
        {"dot", MarkShape::Dot},
        {"cross", MarkShape::Cross},
        {"check", MarkShape::Check},
    }};
    const std::size_t first = text.find(':');
    const std::size_t second = first == std::string_view::npos ? first : text.find(':', first + 1);
    if (second == std::string_view::npos) {
        return std::nullopt;
    }
    const auto shape = std::ranges::find(kShapes, text.substr(0, first),
                                         &std::pair<std::string_view, MarkShape>::first);
    const auto x = ParseMillimetres(text.substr(first + 1, second - first - 1));
    const auto y = ParseMillimetres(text.substr(second + 1));
    if (shape == kShapes.end() || !x || !y) {
        return std::nullopt;
    }
    return std::pair{shape->second, Point{*x, *y}};
}

std::vector<std::string> SplitCommas(std::string_view list) {
    std::vector<std::string> items;
    for (std::size_t start = 0;;) {
        const std::size_t comma = list.find(',', start);
        items.emplace_back(list.substr(start, comma - start));
        if (comma == std::string_view::npos) {
            return items;
        }
        start = comma + 1;
    }
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

std::optional<Options> ParseOptions(int argc, char** argv) {
    if (argc < 2) {
        return std::nullopt;
    }
    Options options{.file = argv[1]};
    bool after = false;
    for (int i = 2; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--debug") {  // the one flag without a value
            options.debug = true;
            continue;
        }
        if (i + 1 >= argc) {
            return std::nullopt;
        }
        if (arg == "--roms") {
            options.roms = argv[++i];
        } else if (arg == "--screenshot") {
            options.screenshot = argv[++i];
        } else if (arg == "--after") {
            const auto duration = ParseDuration(argv[++i]);
            if (!duration) {
                return std::nullopt;
            }
            options.after = std::chrono::nanoseconds(*duration / 1000);
            after = true;
        } else if (arg == "--probe") {
            options.probe = argv[++i];
        } else if (arg == "--manual") {
            const std::string_view manual = argv[++i];
            if (manual == "user") {
                options.manual = Divider::User;
            } else if (manual == "service") {
                options.manual = Divider::Service;
            } else if (manual == "datasheet") {
                options.manual = Divider::Datasheets;
            } else if (manual == "repairs") {
                options.manual = Divider::Repairs;
            } else {
                return std::nullopt;
            }
        } else if (arg == "--look") {
            const std::string_view look = argv[++i];
            if (look != "real" && look != "plain") {
                return std::nullopt;
            }
            options.plainLook = look == "plain";
        } else if (arg == "--focus") {
            options.focus = ParseOwner(argv[++i]);
            if (!options.focus) {
                return std::nullopt;
            }
        } else if (arg == "--repair") {
            options.repair = SplitCommas(argv[++i]);
        } else if (arg == "--clip") {
            options.clip = SplitCommas(argv[++i]);
        } else if (arg == "--group") {
            options.group = SplitCommas(argv[++i]);
        } else if (arg == "--delay") {
            const auto delay = ParseDuration(argv[++i]);
            if (!delay || *delay < 0) {
                return std::nullopt;
            }
            options.delay = *delay;
        } else if (arg == "--sample") {
            const auto period = ParseDuration(argv[++i]);
            if (!period || std::ranges::find(kAnalyzerPeriods, *period) == kAnalyzerPeriods.end()) {
                return std::nullopt;
            }
            options.sample = *period;
        } else if (arg == "--view") {
            options.view = ParseView(argv[++i]);
            if (!options.view) {
                return std::nullopt;
            }
        } else if (arg == "--marker" || arg == "--cursor") {
            const auto time = ParseSignedDuration(argv[++i]);
            if (!time) {
                return std::nullopt;
            }
            (arg == "--marker" ? options.marker : options.cursor) = *time;
        } else if (arg == "--meter") {
            options.meter = argv[++i];
        } else if (arg == "--hook") {
            options.hook = SplitCommas(argv[++i]);
            if (options.hook.size() > kScopeChannels) {
                return std::nullopt;
            }
        } else if (arg == "--timebase") {
            const auto timebase = ParseDuration(argv[++i]);
            if (!timebase || !TimebaseIndex(*timebase)) {
                return std::nullopt;
            }
            options.timebase = *timebase;
        } else if (arg == "--press") {
            for (const std::string& text : SplitCommas(argv[++i])) {
                auto press = ParsePress(text);
                if (!press) {
                    return std::nullopt;
                }
                options.press.push_back(std::move(*press));
            }
        } else if (arg == "--mark") {
            for (const std::string& text : SplitCommas(argv[++i])) {
                const auto mark = ParseMark(text);
                if (!mark) {
                    return std::nullopt;
                }
                options.mark.push_back(*mark);
            }
        } else if (arg == "--pop") {
            for (const std::string& text : SplitCommas(argv[++i])) {
                const auto object = ParseBenchObject(text);
                if (!object) {
                    return std::nullopt;
                }
                options.pop.push_back(*object);
            }
        } else if (arg == "--away") {
            for (const std::string& text : SplitCommas(argv[++i])) {
                const auto object = ParseBenchObject(text);
                if (!object || (*object != BenchObject::Tv && *object != BenchObject::Keyboard &&
                                *object != BenchObject::Knob)) {
                    return std::nullopt;
                }
                options.away.push_back(*object);
            }
        } else if (arg == "--binder-height") {
            const std::string_view text = argv[++i];
            float percent = 0;
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), percent);
            if (error != std::errc{} || std::string_view(end, text.data() + text.size()) != "%" ||
                percent < kMinStretch * 100 || percent > kMaxStretch * 100) {
                return std::nullopt;
            }
            options.binderHeight = percent / 100;
        } else if (arg == "--brief") {
            const std::string_view text = argv[++i];
            const std::size_t colon = text.find(':');
            float x = 0, y = 0;
            const char* last = text.data() + text.size();
            if (colon == std::string_view::npos ||
                std::from_chars(text.data(), text.data() + colon, x).ptr != text.data() + colon ||
                std::from_chars(text.data() + colon + 1, last, y).ptr != last) {
                return std::nullopt;
            }
            options.brief = ImVec2{x, y};
        } else if (arg == "--ask") {
            auto repair = ParseRepair(argv[++i]);
            if (!repair) {
                return std::nullopt;
            }
            options.ask = std::move(*repair);
        } else if (arg == "--side") {
            const std::string_view side = argv[++i];
            if (side != "front" && side != "back") {
                return std::nullopt;
            }
            options.back = side == "back";
        } else if (arg == "--run") {
            const std::string_view run = argv[++i];
            if (run == "pause") {
                options.run = RunSwitch::Pause;
            } else if (run == "1x") {
                options.run = RunSwitch::RealTime;
            } else if (run == "max") {
                options.run = RunSwitch::Max;
            } else {
                return std::nullopt;
            }
        } else if (arg == "--tv") {
            options.tv = ParseTvSettings(argv[++i]);
            if (!options.tv) {
                return std::nullopt;
            }
        } else if (arg == "--knob") {
            for (const std::string& text : SplitCommas(argv[++i])) {
                auto knob = ParseKnob(text);
                if (!knob) {
                    return std::nullopt;
                }
                options.knob.push_back(std::move(*knob));
            }
        } else if (arg == "--type") {
            auto typing = ParseTyping(argv[++i]);  // not split at commas: the text may hold them
            if (!typing) {
                std::println(stderr, "bw_ui: {}", typing.error());
                return std::nullopt;
            }
            options.type.push_back(std::move(*typing));
        } else {
            return std::nullopt;
        }
    }
    if (after && !options.screenshot) {
        return std::nullopt;
    }
    return options;
}

void Print(std::span<const Diagnostic> diagnostics) {
    for (const Diagnostic& d : diagnostics) {
        std::println(stderr, "{}", Format(d));
    }
}

// Diagnostics printed; no session on errors.
Loaded Load(const std::filesystem::path& file, const std::optional<std::filesystem::path>& roms) {
    std::expected<Machine, std::vector<Diagnostic>> machine =
        std::unexpected(std::vector<Diagnostic>{});
    std::string title, brief;
    if (file.extension() == ".bwp") {
        const auto puzzle = ReadPuzzleFile(file);
        if (!puzzle) {
            Print(puzzle.error());
            return {};
        }
        title = puzzle->title;
        brief = puzzle->brief;
        machine = LoadMachine(*puzzle, MachineDir(file), roms);
    } else if (HasBoard(file)) {
        machine = LoadNetlistMachine(file, roms);
    } else {
        LoadResult result = LoadNetlistFile(file);
        if (roms) {
            std::ifstream in(file, std::ios::binary);
            std::stringstream text;
            text << in.rdbuf();
            auto netlist = ParseNetlist(text.str(), file.filename().string());
            if (!netlist) {
                Print(netlist.error());
                return {};
            }
            ResolveContentsPaths(*netlist, std::filesystem::absolute(file).parent_path());
            if (const auto applied = ApplyRomDirectory(*netlist, *roms); !applied) {
                std::println(stderr, "bw_ui: {}", applied.error());
                return {};
            }
            result = BuildCircuit(*netlist);
        }
        if (!result) {
            Print(result.error());
            return {};
        }
        Print((*result)->warnings);
        const auto dir = MachineDir(file);
        LoadedManuals manuals = LoadManuals(dir, DatasheetDir(dir), **result);
        Print(manuals.problems);
        return {Session(std::nullopt, std::move(*result)), file.stem().string(), "",
                std::move(manuals)};
    }
    if (!machine) {
        Print(machine.error());
        return {};
    }
    auto built = machine->build();
    if (!built) {
        Print(built.error());
        return {};
    }
    if (title.empty()) {
        title = machine->name();
    }
    const auto dir = MachineDir(file);
    LoadedManuals manuals = LoadManuals(dir, DatasheetDir(dir), machine->designCircuit());
    Print(manuals.problems);
    return {Session(std::move(*machine), std::move(*built)), std::move(title), std::move(brief),
            std::move(manuals)};
}

// A tooltip past the main window's edge is a desktop window of its own (multi-viewports, P6j).
// On macOS ImGui's SDL3 backend activates every window it shows, so a tooltip would take the keys
// from the window under the mouse; shown without activation it still comes on top (ImGui makes
// tooltips TopMost). Popups keep the backend's way (unactivated, they appear under the window).
void KeepFocusFromTooltips() {
    static void (*show)(ImGuiViewport*) = nullptr;
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    show = platform.Platform_ShowWindow;
    platform.Platform_ShowWindow = [](ImGuiViewport* viewport) {
        constexpr ImGuiViewportFlags kTooltip =
            ImGuiViewportFlags_TopMost | ImGuiViewportFlags_NoFocusOnAppearing;
        if ((viewport->Flags & kTooltip) != kTooltip) {
            show(viewport);
            return;
        }
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
        SDL_ShowWindow(SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<std::uintptr_t>(viewport->PlatformHandle))));
    };
}

// Simulated / wall time over the last second.
class SpeedRatio {
public:
    double update(Time simTime) {
        const auto now = std::chrono::steady_clock::now();
        if (!history_.empty() && simTime < history_.back().simTime) {
            history_.clear();  // powered on again
        }
        history_.push_back({now, simTime});
        while (now - history_.front().wall > std::chrono::seconds(1)) {
            history_.pop_front();
        }
        const std::chrono::duration<double> wall = now - history_.front().wall;
        if (wall.count() <= 0.0) {
            return 0.0;
        }
        const double sim = static_cast<double>(simTime - history_.front().simTime) /
                           static_cast<double>(kPicosPerSecond);
        return sim / wall.count();
    }

private:
    struct Point {
        std::chrono::steady_clock::time_point wall;
        Time simTime;
    };
    std::deque<Point> history_;
};

std::string_view StateName(RunState state) {
    switch (state) {
    case RunState::Off:
        return "off";
    case RunState::Running:
        return "running";
    case RunState::Paused:
        return "paused";
    }
    return "";
}

// What a click with a tool that needs the power off would do (P6o): the repairs to try in order
// (the first Done wins: the iron resolders a pin, else mends), the puller's component (its pencil
// marks leave with it), the refusal when none is Done.
struct RepairPlan {
    std::vector<RepairAction> tries;
    std::optional<std::size_t> swapped;
    std::string unknown;
};

// Board hover text plus the Part's description, or the pin's Datasheet line.
std::string Tooltip(const BoardScene& scene, const Hit& hit, const Manuals& manuals) {
    std::string text = scene.hoverText(hit);
    if (hit.kind == HitKind::Component) {
        if (const PartInfo* part = FindPart(scene.partName(hit.component))) {
            text += " \u00b7 " + std::string(part->description);
        }
    }
    if (hit.kind == HitKind::Pad) {
        const auto sheet = manuals.datasheets.find(scene.partName(hit.component));
        const std::string name(scene.pinName(hit.component, hit.index));
        if (sheet != manuals.datasheets.end() && !name.empty()) {
            if (const Block* bullet = PinBullet(sheet->second, name)) {
                const std::string line = PlainText(bullet->spans);
                const std::size_t colon = line.find(':');
                const std::size_t start =
                    colon == std::string::npos ? 0 : line.find_first_not_of(' ', colon + 1);
                if (start != std::string::npos) {
                    text += " - " + line.substr(start);
                }
            }
        }
    }
    return Displayable(text);
}

int RunWindow(SDL_Window* window, GpuRender& gpu, const Loaded& loaded, Session& session,
              const Options& options, ImFont* legends, ImFont* petLegends) {
    TvView view(gpu.device());
    Shelf shelf;
    TvWindow tvWindow;
    if (options.tv) {
        tvWindow.controls() = *options.tv;
    }
    KnobWindow knobWindow;
    bool debugOut = options.debug;
    const Machine* machine = session.machine();
    BoardView boardView(machine ? std::optional<BoardScene>(std::in_place, *session.drawnBoard(),
                                                            machine->pristineBoard(),
                                                            machine->designCircuit())
                                : std::nullopt);
    std::uint64_t seed = 1;
    std::optional<PinRef> wireStart;  // Wire tool: the first pin
    PencilMarks marks;                // the grease pencil's (P6h)
    for (const auto& [shape, at] : options.mark) {
        marks.add({at, shape, boardView.scene()->bodyAt(at)});  // a Board: checked in main()
    }
    Point wireStartAt{};
    std::set<std::string> latched;  // Buttons kept pressed by a right-click
    std::set<std::string> slid;     // SLIDE switches at ON, kept across power off and on
    std::vector<std::size_t> typedKeys(options.type.size(), 0);  // --type: keys sent so far
    bool wasPowered = false;
    KeyboardCapture capture;
    if (const auto keyboards = session.keyboards(); !keyboards.empty()) {
        capture.reference = keyboards.front();
    }
    KeyboardWindow keyboardWindow(legends);
    MatrixCapture matrixCapture;  // the BW-2001 keyboard (P5q)
    if (const auto matrices = session.matrixKeyboards(); !matrices.empty()) {
        matrixCapture.reference = matrices.front();
        matrixCapture.kind = session.matrixKind(matrices.front()).value_or(KeyMatrixKind::Bw2001);
    }
    MatrixKeyboardWindow matrixWindow(legends, petLegends);
    BoardKeyCapture boardKeys;  // only without a keyboard: that one takes the typing
    if (capture.reference.empty() && matrixCapture.reference.empty()) {
        boardKeys.keys = session.switchKeys();
    }
    boardView.setLegendFont(legends);
    shelf.keyboardOut = !capture.reference.empty() || !matrixCapture.reference.empty();
    shelf.keyboardWindow = !capture.reference.empty()         ? kKeyboardWindowName
                           : !matrixCapture.reference.empty() ? kMatrixKeyboardWindowName
                                                              : nullptr;
    if (const BoardScene* scene = boardView.scene(); scene && options.probe) {
        if (const auto component =
                scene->componentIndex(options.probe->substr(0, options.probe->find('.')))) {
            boardView.select(*component);
        }
    }
    RunSwitch run = options.run;  // the shelf's switch (P6o)
    shelf.manualOut = options.manual.has_value();
    shelf.tvOut = session.tv() != nullptr;
    shelf.knobOut = !session.knobs().empty();
    // Every PC key goes to the Machine (P6o): its keyboard, its Board keys, its knobs, out or put
    // away; but while a text field is edited, a Cmd combination, Tab (the plain look) and ` (the
    // Board flipped). A Machine with both a keyboard and knobs (none yet) would give the knobs only
    // the keys the keyboard has no use for.
    KnobKeys knobKeys;
    const bool hasKnobs = !session.knobs().empty();
    bool knobsOn = false;
    bool textField = false;  // last frame: a text field had the keyboard
    bool dropText = false;   // the text event of a Tab or ` just taken
    const auto applyKeys = [&] {
        capture.on = !capture.reference.empty() && !textField;
        matrixCapture.on = !matrixCapture.reference.empty() && !textField;
        boardKeys.on = !boardKeys.keys.empty() && !textField;
        knobsOn = hasKnobs && !textField;
        if (!knobsOn) {
            knobKeys.clear();
        }
    };
    shelf.hasBrief = shelf.briefOut = !loaded.brief.empty();
    boardView.setPlainLook(options.plainLook);
    if (options.back) {
        boardView.toggleLayer();
    }
    Focus focus;
    if (!options.clip.empty() || !options.group.empty()) {
        focus.bringOut(Owner::Analyzer);
    }
    if (!options.hook.empty()) {
        focus.bringOut(Owner::Scope);
    }
    if (options.meter) {
        focus.bringOut(Owner::Meter);
    }
    if (options.focus && IsInstrument(*options.focus)) {
        focus.key(*options.focus);
    } else if (options.focus) {
        focus.take(*options.focus);
    }
    for (const BenchObject object : options.pop) {  // the Machine has each: checked in main()
        switch (object) {
        case BenchObject::Manual:
            shelf.manualOut = true;
            break;
        case BenchObject::Tv:
            shelf.tvOut = true;
            break;
        case BenchObject::Keyboard:
            shelf.keyboardOut = true;
            break;
        case BenchObject::Knob:
            shelf.knobOut = true;
            break;
        case BenchObject::Analyzer:
            focus.bringOut(Owner::Analyzer);
            break;
        case BenchObject::Scope:
            focus.bringOut(Owner::Scope);
            break;
        case BenchObject::Meter:
            focus.bringOut(Owner::Meter);
            break;
        case BenchObject::Brief:
        case BenchObject::Debug:
            break;
        }
        PopOutAtStart(object);
    }
    for (const BenchObject object : options.away) {
        (object == BenchObject::Tv         ? shelf.tvOut
         : object == BenchObject::Keyboard ? shelf.keyboardOut
                                           : shelf.knobOut) = false;
    }
    Owner lastOwner = focus.owner();
    ManualWindows manualWindows;
    if (options.manual) {
        manualWindows.show(*options.manual);
    }
    manualWindows.setStretch(options.binderHeight);
    BriefNote briefNote;
    if (options.brief) {
        briefNote.offset = *options.brief;
    }
    AnalyzerWindow analyzerWindow;
    MeterWindow meterWindow;
    ScopeWindow scopeWindow;
    analyzerWindow.preset(options.view, options.marker, options.cursor);
    scopeWindow.preset(options.marker, options.cursor);
    boardView.setTooltip([&loaded](const BoardScene& scene, const Hit& hit) {
        return Tooltip(scene, hit, loaded.manuals.manuals);
    });
    // --run pause: paused once a picture is drawn, the switch kept at `‖` (PowerOn would move it).
    constexpr Time kPauseAt = 200 * kPicosPerSecond / 1000;
    if (run == RunSwitch::Max) {
        session.setSpeed(Speed::Max);
    }
    if (run == RunSwitch::Pause) {
        session.powerOn(seed, kPauseAt);
    } else {
        PowerOn(session, run, seed);
    }
    // The power-off tools with the power on (P6o): asked, then cycled at the next frame's clicks
    // (so a frame is unpowered, as in a manual power cycle), powered on at the frame after.
    PowerCyclePopup popup;
    std::optional<RepairPlan> pending;    // asked, not answered
    std::optional<RepairPlan> cycleNext;  // OK: cycled at the next frame's clicks
    bool powerOnNext = false;
    bool askAtStart = options.ask.has_value();
    const auto start = std::chrono::steady_clock::now();
    SpeedRatio ratio;
    double uiMilliseconds = 0;
    int exitCode = 0;
    long frames = 0;  // drawn, for the frame rate --screenshot prints

    bool running = true;
    while (running) {
        const auto frameStart = std::chrono::steady_clock::now();
        SDL_Event event;
        if (powerOnNext) {  // before session.tv(): a repaired Machine's power-on rebuilds it
            PowerOn(session, run, seed);
            powerOnNext = false;
        }
        applyKeys();
        while (SDL_PollEvent(&event)) {
            const bool keyDown = event.type == SDL_EVENT_KEY_DOWN;
            const bool keyUpDown = keyDown || event.type == SDL_EVENT_KEY_UP;
            // Cmd combinations are the app's (quit, the windows), but the matrix keyboard's paste.
            const bool cmd = keyUpDown && (event.key.mod & SDL_KMOD_GUI) != 0 &&
                             !(matrixCapture.on && event.key.key == SDLK_V);
            if (keyDown) {
                dropText = false;
            }
            if (keyDown && !cmd && !textField && !event.key.repeat) {
                // Tab: the real Board or the plain one; ` (by its character, any layout): the
                // Board flipped. Their text event, if any, is dropped.
                const SDL_Keycode typed = SDL_GetKeyFromScancode(
                    event.key.scancode, static_cast<SDL_Keymod>(event.key.mod & SDL_KMOD_SHIFT),
                    false);
                if (event.key.key == SDLK_TAB || typed == '`') {
                    if (event.key.key == SDLK_TAB) {
                        boardView.setPlainLook(!boardView.plainLook());
                    } else {
                        boardView.toggleLayer();
                    }
                    dropText = true;
                    continue;
                }
            }
            if (dropText && event.type == SDL_EVENT_TEXT_INPUT) {
                dropText = false;
                continue;
            }
            // The modifiers always reach ImGui: a Shift-click stays one.
            const bool modifier =
                keyUpDown && (event.key.key == SDLK_LSHIFT || event.key.key == SDLK_RSHIFT ||
                              event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL ||
                              event.key.key == SDLK_LALT || event.key.key == SDLK_RALT ||
                              event.key.key == SDLK_LGUI || event.key.key == SDLK_RGUI);
            if (!cmd) {
                knobKeys.handle(event, knobsOn);
                if (capture.handle(event, session) || matrixCapture.handle(event, session) ||
                    boardKeys.handle(event)) {
                    if (modifier) {
                        ImGui_ImplSDL3_ProcessEvent(&event);
                    }
                    continue;
                }
            }
            const bool keyEvent = keyDown || event.type == SDL_EVENT_TEXT_INPUT ||
                                  event.type == SDL_EVENT_TEXT_EDITING;
            if (keyEvent && !modifier && !cmd && !textField) {
                continue;  // the Machine's, or nobody's: no shortcut acts
            }
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window))) {
                running = false;
            }
        }
        TvConsumer* tv = session.tv();
        if (tv && tv->pictures().update()) {
            view.show(tv->pictures().front());
        }
        const RunnerStatus status = session.runner().status();
        const double speedRatio = ratio.update(status.simTime);

        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        OnBench();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Bodgewire", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (focus.owner() != lastOwner) {
            wireStart.reset();  // a wire is started with one tool only
            lastOwner = focus.owner();
        }
        boardView.setOwner(focus.owner());
        const Extent bench = BenchArea().extent();  // above the shelf
        const ImVec2 available{bench.width, bench.height};
        boardView.draw(available);
        const auto refuse = [&](std::string reason) {
            boardView.refuse(reason);
            shelf.refused(std::move(reason));
        };
        const auto applyPlan = [&](const RepairPlan& plan) {
            for (const RepairAction& repair : plan.tries) {
                if (session.repair(repair) == RepairResult::Done) {
                    if (plan.swapped) {
                        marks.swapped(*plan.swapped);  // its marks leave with it (P6h)
                    }
                    return;
                }
            }
            if (!plan.unknown.empty()) {
                refuse(plan.unknown);
            }
        };
        const auto cycle = [&](const RepairPlan& plan) {
            session.powerOff();
            applyPlan(plan);
            powerOnNext = true;
        };
        if (cycleNext) {
            cycle(*cycleNext);
            cycleNext.reset();
        }
        if (askAtStart) {  // --ask: as if clicked with the power on, at the bench's centre
            askAtStart = false;
            RepairPlan plan{{*options.ask}, std::nullopt, ""};
            if (const auto* swap = std::get_if<SwapRepair>(&*options.ask);
                swap && boardView.scene()) {
                plan.swapped = boardView.scene()->componentIndex(swap->reference);
            }
            pending = std::move(plan);
            const BenchRect area = BenchArea();
            popup.open({(area.min.x + area.max.x) / 2, (area.min.y + area.max.y) / 2});
        }
        if (const auto click = boardView.takeClick()) {
            const auto& pick = click->pick;
            // A click on a part selects it (its pins too, as a probe touches them); on the
            // selected part's body again, or on bare board, it deselects.
            // Not under the pencil: the selection's gold would cover what was just marked.
            const bool body = pick && pick->hit.kind == HitKind::Component;
            if (focus.owner() != Owner::Pencil && !click->fromPage) {
                if (!click->component || (body && boardView.selected() == click->component)) {
                    boardView.deselect();
                } else {
                    boardView.select(*click->component);
                }
            }
            std::optional<RepairPlan> plan;  // a tool that needs the power off (P6o)
            switch (focus.owner()) {
            case Owner::Probe:
                if (!pick) {
                    session.touch(std::nullopt);  // bare Board: the tip lifted (P6o)
                } else if (pick->hit.kind != HitKind::Component) {
                    session.touch(ProbeTip{pick->contact, pick->layer});
                }
                break;
            case Owner::Puller:
                if (click->component) {
                    plan = RepairPlan{
                        {SwapRepair{session.drawnBoard()->components[*click->component].reference}},
                        click->component,
                        ""};
                }
                break;
            case Owner::Iron: {
                // A chip pin is resoldered, else a crack mended (a mend is always Done: Session);
                // on the Board only, so a click beside it never power-cycles.
                if (!InsideOutline(boardView.scene()->outline(), click->at)) {
                    break;
                }
                plan = RepairPlan{};
                if (pick && pick->hit.kind == HitKind::Pad) {
                    const BoardComponent& part =
                        session.drawnBoard()->components[pick->hit.component];
                    plan->tries.push_back(
                        ResolderRepair{{part.reference, part.pads[pick->hit.index].number}});
                }
                plan->tries.push_back(pick && pick->hit.kind == HitKind::Trace
                                          ? MendRepair{pick->layer, pick->contact}
                                          : MendRepair{click->active, click->at});
                break;
            }
            case Owner::Cutters: {
                constexpr std::string_view kCutRefusal = "Cut: click a trace or a wire";
                if (const auto wire = boardView.wireAt(click->at)) {
                    const WireEnds& w = machine->wires()[*wire];
                    plan = RepairPlan{
                        {UnwireRepair{w.a, w.b}}, std::nullopt, std::string(kCutRefusal)};
                } else if (const auto bridge = boardView.bridgeAt(click->at)) {
                    const WireEnds& b = machine->bridges()[*bridge];
                    plan = RepairPlan{
                        {UnbridgeRepair{b.a, b.b}}, std::nullopt, std::string(kCutRefusal)};
                } else if (!pick || pick->hit.kind != HitKind::Trace) {
                    refuse(std::string(kCutRefusal));
                } else if (boardView.scene()->board().traces[pick->hit.index].mid) {
                    refuse("Cut: straight traces only");
                } else {
                    plan = RepairPlan{{CutRepair{pick->layer, pick->contact}},
                                      std::nullopt,
                                      std::string(kCutRefusal)};
                }
                break;
            }
            case Owner::Wire: {
                if (!pick || pick->hit.kind != HitKind::Pad) {
                    refuse("Wire: solder to pins only");
                    break;
                }
                const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
                const Pad& pad = part.pads[pick->hit.index];
                const PinRef end{part.reference, pad.number};
                if (!wireStart) {  // the first pin, with the power on too
                    wireStart = end;
                    wireStartAt = pad.position;
                    break;
                }
                if (wireStart->reference != end.reference || wireStart->pin != end.pin) {
                    plan = RepairPlan{{WireRepair{*wireStart, end}}, std::nullopt, ""};
                }
                wireStart.reset();  // the same pin again: cancelled
                break;
            }
            case Owner::Analyzer:
            case Owner::Scope: {
                const bool isClip = focus.owner() == Owner::Analyzer;
                if (!pick || pick->hit.kind == HitKind::Component) {
                    break;
                }
                if (!AcceptsLead(LeadKind::Attached, pick->hit.kind)) {
                    refuse(std::format("{}: attaches to pins only", isClip ? "Clip" : "Hook"));
                    break;
                }
                const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
                const std::string& pin = part.pads[pick->hit.index].number;
                if (!session.padLead(part.reference, pin)) {  // a resistor's, a capacitor's
                    refuse(std::format("{}: chip pins only", isClip ? "Clip" : "Hook"));
                    break;
                }
                if (isClip) {
                    const auto channel = session.clipAt(part.reference, pin);
                    const auto group = channel ? session.groupOf(*channel) : std::nullopt;
                    if (channel && click->shift && group) {
                        session.unclipGroup(*group);
                    } else if (channel) {
                        session.unclip(*channel);
                    } else if (click->shift) {
                        if (const auto clipped = session.clipGroup(part.reference, pin); !clipped) {
                            refuse("Clip: " + clipped.error());
                        }
                    } else if (!session.clip(part.reference, pin)) {
                        refuse(std::format("Clip: all {} channels used", kChannels));
                    }
                } else {
                    if (const auto channel = session.hookAt(part.reference, pin)) {
                        session.unhook(*channel);
                    } else if (!session.hook(part.reference, pin)) {
                        refuse(std::format("Hook: all {} channels used", kScopeChannels));
                    }
                }
                break;
            }
            case Owner::Pencil:
                if (!InsideOutline(boardView.scene()->outline(), click->at)) {
                    break;  // on the Board only (P6o)
                }
                if (click->shift) {
                    marks.clear(click->at, boardView.markReach());
                } else {
                    marks.click(click->at, boardView.scene()->bodyAt(click->at),
                                boardView.markReach());
                }
                break;
            case Owner::Meter:
                if (pick && AcceptsLead(LeadKind::Held, pick->hit.kind) &&
                    !session.placeLead({pick->contact, pick->layer,
                                        LeadLabel(*boardView.scene(), pick->hit, pick->layer)})) {
                    refuse("Meter: power off for continuity");
                }
                break;
            }
            if (plan) {
                switch (*StepFor(true, session.powered(), popup.dontAsk())) {
                case RepairStep::Apply:
                    applyPlan(*plan);
                    break;
                case RepairStep::Ask:
                    pending = std::move(plan);
                    popup.open(ImGui::GetIO().MousePos);
                    break;
                case RepairStep::Cycle:
                    cycle(*plan);
                    break;
                }
            }
        }
        if (const auto at = boardView.takeRightClick();
            at && focus.owner() == Owner::Pencil &&
            InsideOutline(boardView.scene()->outline(), *at)) {
            marks.clear(*at, boardView.markReach());
        }
        boardView.setMarks(marks.marks());
        const auto tip = session.probeTip();
        boardView.setProbeTip(tip ? std::optional(tip->at) : std::nullopt);
        boardView.setBlobs(session.blobs());
        std::vector<ClipTag> tags;
        for (std::size_t c = 0; c < kChannels; ++c) {
            if (const auto& clip = session.clips()[c]) {
                const auto group = session.groupOf(c);
                tags.push_back({clip->at, c, group ? GroupColour(*group) : ChannelColour(c)});
            }
        }
        boardView.setClips(tags);
        std::vector<HookTag> hookTags;
        for (std::size_t c = 0; c < kScopeChannels; ++c) {
            if (const auto& hook = session.hooks()[c]) {
                hookTags.push_back({hook->at, c});
            }
        }
        boardView.setHooks(hookTags);
        std::vector<LeadTag> leadTags;
        if (session.red()) {
            leadTags.push_back({session.red()->at, true});
        }
        if (session.black() && session.meterMode() == MeterMode::Continuity) {
            leadTags.push_back({session.black()->at, false});
        }
        boardView.setLeads(leadTags);
        if (const BoardScene* scene = boardView.scene()) {
            std::vector<LedTag> ledTags;
            for (const LedLight& led : session.leds()) {
                if (const auto component = scene->componentIndex(led.reference)) {
                    ledTags.push_back({*component, led.shape, led.brightness});
                }
            }
            boardView.setLeds(ledTags);
        }
        {
            // Buttons: held or latched on the Board, or scripted by --press (also without a
            // Board).
            const BoardScene* scene = boardView.scene();
            if (!session.powered()) {
                latched.clear();
            }
            const auto toggled = boardView.takeLatchToggle();
            std::vector<SwitchTag> switchTags;
            for (const std::string& reference : session.switches()) {
                const auto component =
                    scene ? scene->componentIndex(reference) : std::optional<std::size_t>();
                const auto part =
                    std::ranges::find(session.circuit().parts, reference, &LoadedPart::reference);
                const std::string_view type =
                    part != session.circuit().parts.end() ? part->info->name : "";
                const SwitchLook look = type == "KEY"     ? SwitchLook::Key
                                        : type == "SLIDE" ? SwitchLook::Slide
                                                          : SwitchLook::Button;
                if (look == SwitchLook::Slide) {
                    // A slide stays where it is moved, powered or not.
                    if (component && toggled == component && !slid.erase(reference)) {
                        slid.insert(reference);
                    }
                } else if (component && toggled == component && session.powered() &&
                           !latched.erase(reference)) {
                    latched.insert(reference);
                }
                const bool scripted = std::ranges::any_of(options.press, [&](const ButtonPress& p) {
                    return p.reference == reference && p.down <= status.simTime &&
                           status.simTime < p.up;
                });
                session.setPressed(reference, latched.contains(reference) ||
                                                  slid.contains(reference) || scripted ||
                                                  (component && boardView.held() == component) ||
                                                  boardKeys.holds(reference));
                if (component) {
                    const Switch* sw = SwitchOf(session.circuit(), reference);
                    switchTags.push_back({*component, session.pressed(reference), look,
                                          sw ? std::string(sw->legend()) : std::string()});
                }
            }
            if (scene) {
                boardView.setSwitches(switchTags);
            }
        }
        {
            // Keys scripted by --type. Not in the first frame powered: its status.simTime is the
            // previous run's (the power-on is only queued).
            if (!session.powered()) {
                std::ranges::fill(typedKeys, 0);
            } else if (wasPowered) {
                for (std::size_t t = 0; t < options.type.size(); ++t) {
                    const Typing& typing = options.type[t];
                    if (typing.reference == matrixCapture.reference) {
                        // The matrix paces its own presses: the text queued whole, once.
                        if (typedKeys[t] == 0 && typing.start <= status.simTime) {
                            session.typeText(typing.reference, typing.text);
                            typedKeys[t] = 1;
                        }
                        continue;
                    }
                    while (typedKeys[t] < typing.keys.size() &&
                           typing.at(typedKeys[t]) <= status.simTime) {
                        const std::uint8_t code = typing.keys[typedKeys[t]++];
                        if (session.typeCode(typing.reference, code) &&
                            typing.reference == capture.reference) {
                            keyboardWindow.flash(code);
                        }
                    }
                }
            }
            wasPowered = session.powered();
        }
        if (machine) {
            std::vector<CutMark> cuts;
            for (const Crack& cut : machine->cuts()) {
                cuts.push_back({cut.layer, cut.at});
            }
            boardView.setCuts(cuts);
            std::vector<CutMark> cracks;
            for (const Crack& crack : machine->cracks()) {
                cracks.push_back({crack.layer, crack.at});
            }
            boardView.setCracks(cracks);
            std::vector<BodgeWire> wires;
            for (const WireEnds& w : machine->wires()) {
                wires.push_back(w.wire);
            }
            boardView.setBodgeWires(wires);
            std::vector<BodgeWire> bridges;
            for (const WireEnds& b : machine->bridges()) {
                bridges.push_back(b.wire);
            }
            boardView.setBridges(bridges);
        }
        boardView.setWireStart(wireStart ? std::optional(wireStartAt) : std::nullopt);
        shelf.wireStarted = wireStart.has_value();
        const std::string_view lock =
            !tv ? "no video" : (tv->pictures().front().locked ? "locked" : "no lock");
        const DebugStats stats{static_cast<double>(status.simTime) /
                                   static_cast<double>(kPicosPerSecond),
                               speedRatio,
                               status.ringWaitShare,
                               uiMilliseconds,
                               lock,
                               StateName(status.state)};
        shelf.draw(session, boardView, focus, run, seed);
        ImGui::End();
        // The bench, back to front at start: brief, TV, knob, manuals, instruments, keyboard,
        // Debug.
        knobWindow.keys(session, knobKeys, knobsOn);
        DrawBrief(shelf.briefOut, loaded, boardView.corner(), briefNote);
        tvWindow.draw(shelf.tvOut, view);
        knobWindow.draw(shelf.knobOut, session);
        manualWindows.draw(shelf.manualOut, loaded.manuals.manuals, boardView, DesignOf(session),
                           session.log());
        if (session.machine()) {
            const auto instrument = [&](Owner owner, auto& window) {
                bool open = focus.out(owner);
                if (window.draw(open, session)) {
                    focus.clicked(owner);
                }
                if (!open) {
                    focus.closed(owner);
                }
            };
            instrument(Owner::Analyzer, analyzerWindow);
            instrument(Owner::Scope, scopeWindow);
            instrument(Owner::Meter, meterWindow);  // the smallest in front at start
        }
        // Also without a Board (a netlist-only Session, the bench).
        // One keyboard per Machine, either kind: the shelf's drawing shows or hides it.
        bool keyboardOpen = shelf.keyboardOut && !capture.reference.empty();
        bool matrixOpen = shelf.keyboardOut && !matrixCapture.reference.empty();
        keyboardWindow.draw(keyboardOpen, session, capture);
        matrixWindow.draw(matrixOpen, session, matrixCapture);
        if (shelf.keyboardOut && !keyboardOpen && !matrixOpen) {
            shelf.keyboardOut = false;  // closed by its window
        }
        DrawDebug(debugOut, session, stats, seed);
        switch (popup.draw()) {  // last: above every window
        case PopupAnswer::Ok:
            cycleNext = std::move(pending);
            pending.reset();
            break;
        case PopupAnswer::Cancel:
            pending.reset();
            break;
        case PopupAnswer::None:
            break;
        }
        // A text field takes the keys from the next event on (P6o).
        textField = ImGui::GetIO().WantTextInput;
        applyKeys();
        // Text input on the window with the keyboard focus: a popped-out keyboard's (P6j).
        SDL_Window* focused = SDL_GetKeyboardFocus();
        capture.sync(focused != nullptr ? focused : window);
        matrixCapture.sync(focused != nullptr ? focused : window);
        boardKeys.sync();

        ImGui::Render();
        // The UI's work: the frame built, not drawn (the GPU's waits for the windows' vsync
        // would count otherwise, P6j).
        uiMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart)
                .count();
        gpu.present(view);
        ++frames;
        const auto now = std::chrono::steady_clock::now();
        if (options.screenshot && now - start >= options.after) {
            std::println("bw_ui: t {:.3f} s  {:.2f}\u00d7  ui {:.1f} ms  {:.0f} fps",
                         static_cast<double>(status.simTime) / static_cast<double>(kPicosPerSecond),
                         speedRatio, uiMilliseconds,
                         static_cast<double>(frames) /
                             std::chrono::duration<double>(now - start).count());
            if (!gpu.screenshot(*options.screenshot)) {
                std::println(stderr, "bw_ui: {}", SDL_GetError());
                exitCode = kExitErrors;
            }
            break;
        }
    }
    return exitCode;
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = ParseOptions(argc, argv);
    if (!options) {
        std::println(stderr, "{}", kUsage);
        return kExitUsage;
    }
    Loaded loaded = Load(options->file, options->roms);
    if (!loaded.session) {
        return kExitErrors;
    }
    Session& session = *loaded.session;
    if (options->probe) {
        const std::string& pin = *options->probe;
        const std::size_t dot = pin.find('.');
        std::optional<Point> at;
        if (session.machine() && dot != std::string::npos) {
            at = session.machine()->padPosition(pin.substr(0, dot), pin.substr(dot + 1));
        }
        if (!at) {
            std::println(stderr, "bw_ui: unknown pin '{}'", pin);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
        session.touch(ProbeTip{*at, Layer::Front});
    }
    AnalyzerSettings analyzer;
    if (options->sample) {
        analyzer.period = *options->sample;
    }
    for (const std::string& item : options->clip) {
        const std::size_t colon = item.find(':');
        const std::string pin = item.substr(0, colon);
        const std::size_t dot = pin.find('.');
        const auto trigger = colon == std::string::npos
                                 ? std::optional(TriggerCondition::Any)
                                 : ParseTrigger(std::string_view(item).substr(colon + 1));
        const auto channel = dot == std::string::npos
                                 ? std::nullopt
                                 : session.clip(pin.substr(0, dot), pin.substr(dot + 1));
        if (!trigger || !channel) {
            std::println(stderr, "bw_ui: unknown pin '{}'", item);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
        analyzer.trigger[*channel] = *trigger;
    }
    if (!options->clip.empty() || !options->group.empty()) {
        session.setAnalyzer(analyzer);
    }
    for (const std::string& item : options->group) {
        const std::size_t colon = item.find(':');
        const std::string pin = item.substr(0, colon);
        const std::size_t dot = pin.find('.');
        const auto clipped =
            dot == std::string::npos
                ? std::nullopt
                : std::optional(session.clipGroup(pin.substr(0, dot), pin.substr(dot + 1)));
        const bool ok = clipped && *clipped &&
                        (colon == std::string::npos ||
                         ((*clipped)->group &&
                          session.setGroupTrigger(*(*clipped)->group, item.substr(colon + 1))));
        if (!ok) {
            std::println(stderr, "bw_ui: bad group '{}'{}", item,
                         clipped && !*clipped ? ": " + clipped->error() : "");
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
    }
    if (!options->clip.empty() || !options->group.empty()) {
        session.arm();
    }
    for (const std::string& item : options->hook) {
        const std::size_t dot = item.find('.');
        if (dot == std::string::npos || !session.hook(item.substr(0, dot), item.substr(dot + 1))) {
            std::println(stderr, "bw_ui: unknown pin '{}'", item);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
    }
    if (options->timebase || options->delay) {
        ScopeSettings scope = session.scopeSettings();
        scope.timebase = options->timebase.value_or(scope.timebase);
        scope.delay = options->delay.value_or(scope.delay);
        if (scope.delay > kMaxDelayDivisions * scope.timebase) {
            std::println(stderr, "bw_ui: delay over {} divisions", kMaxDelayDivisions);
            return kExitUsage;
        }
        session.setScope(scope);
    }
    if (options->meter) {
        const std::size_t dot = options->meter->find('.');
        const auto lead =
            dot == std::string::npos
                ? std::nullopt
                : session.padLead(options->meter->substr(0, dot), options->meter->substr(dot + 1));
        if (!lead) {
            std::println(stderr, "bw_ui: unknown pin '{}'", *options->meter);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
        const bool chip = std::ranges::any_of(
            session.machine()->designCircuit().parts, [&](const LoadedPart& part) {
                return part.reference == lead->reference && part.info->kind == PartKind::Chip;
            });
        session.placeLead({lead->at, Layer::Front, lead->label + (chip ? " leg" : "")});
    }
    for (const std::string& text : options->repair) {
        const auto repair = ParseRepair(text);
        std::optional<std::string> error;
        if (!repair) {
            error = repair.error();
        } else if (session.repair(*repair) != RepairResult::Done) {
            error = !session.machine() ? std::string("no Board to repair")
                                       : "cannot apply '" + text + "'";
        }
        if (error) {
            std::println(stderr, "bw_ui: {}", *error);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
    }
    if (options->ask && !session.machine()) {
        std::println(stderr, "bw_ui: no Board to repair");
        std::println(stderr, "{}", kUsage);
        return kExitUsage;
    }
    if (!options->mark.empty() && !session.machine()) {
        std::println(stderr, "bw_ui: no Board to mark");
        std::println(stderr, "{}", kUsage);
        return kExitUsage;
    }
    if (options->tv && session.tv() == nullptr) {
        std::println(stderr, "bw_ui: this Machine has no TV");
        std::println(stderr, "{}", kUsage);
        return kExitUsage;
    }
    for (const BenchObject object : options->pop) {
        const bool has = object == BenchObject::Tv ? session.tv() != nullptr
                         : object == BenchObject::Keyboard
                             ? !session.keyboards().empty() || !session.matrixKeyboards().empty()
                         : object == BenchObject::Knob   ? !session.knobs().empty()
                         : object == BenchObject::Manual ? true
                                                         : session.machine() != nullptr;
        if (!has) {
            std::println(stderr, "bw_ui: this Machine has no {}",
                         object == BenchObject::Tv         ? "TV"
                         : object == BenchObject::Keyboard ? "keyboard"
                         : object == BenchObject::Knob     ? "paddle controller"
                                                           : "Board for the instruments");
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
    }
    for (const KnobSetting& knob : options->knob) {
        if (!session.setKnob(knob.reference, knob.turn)) {
            std::println(stderr, "bw_ui: '{}' is not a paddle controller", knob.reference);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
    }
    for (const Typing& typing : options->type) {
        if (!std::ranges::contains(session.keyboards(), typing.reference) &&
            !std::ranges::contains(session.matrixKeyboards(), typing.reference)) {
            std::println(stderr, "bw_ui: '{}' is not a keyboard", typing.reference);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
        if (typing.untypable && std::ranges::contains(session.keyboards(), typing.reference)) {
            std::println(stderr, "bw_ui: cannot type the text on '{}', a keyboard",
                         typing.reference);
            std::println(stderr, "{}", kUsage);
            return kExitUsage;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        return kExitErrors;
    }
    const std::string title = "Bodgewire \u2014 " + loaded.title;
    SDL_Window* window = SDL_CreateWindow(title.c_str(), kWindowWidth, kWindowHeight,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        SDL_Quit();
        return kExitErrors;
    }
    auto gpu = std::make_unique<GpuRender>(window);
    if (!gpu->ready()) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        gpu.reset();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return kExitErrors;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // Multi-viewports, docking off (P6j): a popped-out object in its own desktop window, not tied
    // to the main one (ImGui's default), with no title bar.
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // ConfigViewportsNoDefaultParent: true
    ImGui_ImplSDL3_InitForSDLGPU(window);
    KeepFocusFromTooltips();
    ImGui_ImplSDLGPU3_InitInfo gpuInfo;
    gpuInfo.Device = gpu->device();
    gpuInfo.ColorTargetFormat = gpu->format();
    // The popped-out windows never wait for their display's vsync: one on a slower display (a
    // 30 Hz monitor) would pace the whole UI at its rate. The main window's vsync paces it.
    gpuInfo.PresentMode = gpu->unpacedMode();
    ImGui_ImplSDLGPU3_Init(&gpuInfo);
    // The UI font first, as NewFrame would pick it lazily (13 px at the framebuffer scale: vector
    // from 15 px); AddFontDefault() itself, before the first frame, assumes Retina on macOS.
    ImFontAtlas& fonts = *ImGui::GetIO().Fonts;
    if (SDL_GetWindowPixelDensity(window) * 13.0f >= 15.0f) {
        fonts.AddFontDefaultVector();
    } else {
        fonts.AddFontDefaultBitmap();
    }
    ImFontConfig legendConfig;
    legendConfig.FontDataOwnedByAtlas = false;  // static data, never written
    ImFont* legends =
        fonts.AddFontFromMemoryTTF(const_cast<unsigned char*>(kKeycapFont),
                                   static_cast<int>(kKeycapFontSize), 0.0f, &legendConfig);
    ImFont* petLegends = fonts.AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kPetFont), static_cast<int>(kPetFontSize), 0.0f, &legendConfig);

    const int exitCode = RunWindow(window, *gpu, loaded, session, *options, legends, petLegends);

    SDL_WaitForGPUIdle(gpu->device());
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
    gpu.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
