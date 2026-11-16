#include <algorithm>
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
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

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
#include "ui/board_keys.hpp"
#include "ui/board_view.hpp"
#include "ui/keyboard_window.hpp"
#include "ui/loaded.hpp"
#include "ui/manual_view.hpp"
#include "ui/matrix_keyboard_window.hpp"
#include "ui/meter_window.hpp"
#include "ui/scope_window.hpp"
#include "ui/side_panel.hpp"
#include "ui/toolbar.hpp"
#include "ui/tv_view.hpp"
#include "view/time_view.hpp"

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
    "[--type REF:START:TEXT] [--roms DIR] [--manual user] [--screenshot OUT.png [--after 1s]]";

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
    bool userManual = false;
};

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
            if (std::string_view(argv[++i]) != "user") {
                return std::nullopt;
            }
            options.userManual = true;
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

// Saves the whole framebuffer; false with SDL's error on failure.
bool SaveScreenshot(SDL_Renderer* renderer, const std::filesystem::path& path) {
    SDL_SetRenderScale(renderer, 1, 1);
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    const bool ok = surface != nullptr && SDL_SavePNG(surface, path.string().c_str());
    SDL_DestroySurface(surface);
    return ok;
}

int RunWindow(SDL_Window* window, SDL_Renderer* renderer, const Loaded& loaded, Session& session,
              const Options& options, ImFont* legends) {
    TvView view(renderer);
    Toolbar toolbar;
    SidePanel sidePanel;
    const Machine* machine = session.machine();
    BoardView boardView(machine ? std::optional<BoardScene>(std::in_place, *session.drawnBoard(),
                                                            machine->pristineBoard(),
                                                            machine->designCircuit())
                                : std::nullopt);
    std::uint64_t seed = 1;
    std::optional<PinRef> wireStart;  // Wire tool: the first pin
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
    MatrixKeyboardWindow matrixWindow(legends);
    bool matrixOpen = !matrixCapture.reference.empty();
    BoardKeyCapture boardKeys;  // only without a keyboard: that one takes the typing
    if (capture.reference.empty() && matrixCapture.reference.empty()) {
        boardKeys.keys = session.switchKeys();
    }
    boardView.setLegendFont(legends);
    bool keyboardOpen = !capture.reference.empty();
    if (const BoardScene* scene = boardView.scene(); scene && options.probe) {
        if (const auto component =
                scene->componentIndex(options.probe->substr(0, options.probe->find('.')))) {
            boardView.select(*component);
        }
    }
    Speed speed = Speed::RealTime;
    toolbar.userManualOpen = options.userManual;
    toolbar.analyzerOpen = !options.clip.empty() || !options.group.empty();
    toolbar.meterOpen = options.meter.has_value();
    toolbar.scopeOpen = !options.hook.empty();
    ManualWindows manualWindows;
    AnalyzerWindow analyzerWindow;
    MeterWindow meterWindow;
    ScopeWindow scopeWindow;
    analyzerWindow.preset(options.view, options.marker, options.cursor);
    scopeWindow.preset(options.marker, options.cursor);
    boardView.setTooltip([&loaded](const BoardScene& scene, const Hit& hit) {
        return Tooltip(scene, hit, loaded.manuals.manuals);
    });
    session.powerOn(seed);
    const auto start = std::chrono::steady_clock::now();
    SpeedRatio ratio;
    double uiMilliseconds = 0;
    int exitCode = 0;

    bool running = true;
    while (running) {
        const auto frameStart = std::chrono::steady_clock::now();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (capture.handle(event, session) || matrixCapture.handle(event, session) ||
                boardKeys.handle(event)) {
                continue;
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

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Bodgewire", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        toolbar.draw(session, boardView, speed, seed);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        boardView.draw({available.x * kBoardShare, available.y});
        // Esc in an instrument window clears its marker; it is not the Board's.
        if (boardView.takeEscape() && !analyzerWindow.focused() && !scopeWindow.focused()) {
            if (boardView.tool() == Tool::Meter) {
                session.liftLeads();
            } else if (boardView.tool() == Tool::Wire) {
                wireStart.reset();
            } else {
                session.touch(std::nullopt);
            }
        }
        const auto refuse = [&](std::string reason) {
            boardView.refuse(reason);
            toolbar.refused(std::move(reason));
        };
        if (const auto click = boardView.takeClick()) {
            const auto& pick = click->pick;
            if (click->component) {
                boardView.select(*click->component);
            }
            RepairResult result = RepairResult::Done;
            switch (boardView.tool()) {
            case Tool::Probe:
                if (pick && pick->hit.kind != HitKind::Component) {
                    session.touch(ProbeTip{pick->contact, pick->layer});
                }
                break;
            case Tool::Swap:
                if (click->component) {
                    result = session.repair(
                        SwapRepair{session.drawnBoard()->components[*click->component].reference});
                }
                break;
            case Tool::Mend:
                if (pick && pick->hit.kind == HitKind::Trace) {
                    result = session.repair(MendRepair{pick->layer, pick->contact});
                } else {
                    result = session.repair(MendRepair{click->active, click->at});
                }
                break;
            case Tool::Cut:
                if (session.powered()) {
                    result = RepairResult::Refused;
                    break;
                }
                if (const auto wire = boardView.wireAt(click->at)) {
                    const WireEnds& w = machine->wires()[*wire];
                    result = session.repair(UnwireRepair{w.a, w.b});
                } else if (const auto bridge = boardView.bridgeAt(click->at)) {
                    const WireEnds& b = machine->bridges()[*bridge];
                    result = session.repair(UnbridgeRepair{b.a, b.b});
                } else if (!pick || pick->hit.kind != HitKind::Trace) {
                    refuse("Cut: click a trace or a wire");
                } else if (boardView.scene()->board().traces[pick->hit.index].mid) {
                    refuse("Cut: straight traces only");
                } else {
                    result = session.repair(CutRepair{pick->layer, pick->contact});
                }
                if (result == RepairResult::Unknown) {
                    refuse("Cut: click a trace or a wire");
                }
                break;
            case Tool::Wire: {
                if (session.powered()) {
                    result = RepairResult::Refused;
                    break;
                }
                if (!pick || pick->hit.kind != HitKind::Pad) {
                    refuse("Wire: solder to pins only");
                    break;
                }
                const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
                const Pad& pad = part.pads[pick->hit.index];
                const PinRef end{part.reference, pad.number};
                if (!wireStart) {
                    wireStart = end;
                    wireStartAt = pad.position;
                    break;
                }
                if (wireStart->reference != end.reference || wireStart->pin != end.pin) {
                    result = session.repair(WireRepair{*wireStart, end});
                }
                wireStart.reset();
                break;
            }
            case Tool::Resolder: {
                if (session.powered()) {
                    result = RepairResult::Refused;
                    break;
                }
                if (!pick || pick->hit.kind != HitKind::Pad) {
                    refuse("Resolder: chip pins only");
                    break;
                }
                const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
                result = session.repair(
                    ResolderRepair{{part.reference, part.pads[pick->hit.index].number}});
                if (result == RepairResult::Unknown) {
                    refuse("Resolder: chip pins only");
                }
                break;
            }
            case Tool::Clip:
            case Tool::Hook: {
                const bool isClip = boardView.tool() == Tool::Clip;
                if (!pick || pick->hit.kind == HitKind::Component) {
                    break;
                }
                if (!AcceptsLead(LeadKind::Attached, pick->hit.kind)) {
                    refuse(std::format("{}: attaches to pins only", isClip ? "Clip" : "Hook"));
                    break;
                }
                const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
                const std::string& pin = part.pads[pick->hit.index].number;
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
            case Tool::Meter:
                if (pick && AcceptsLead(LeadKind::Held, pick->hit.kind) &&
                    !session.placeLead({pick->contact, pick->layer,
                                        LeadLabel(*boardView.scene(), pick->hit, pick->layer)})) {
                    refuse("Meter: power off for continuity");
                }
                break;
            }
            if (result == RepairResult::Refused) {
                toolbar.refused();
                boardView.refuse("Power off first");
            }
        }
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
        toolbar.wireStarted = wireStart.has_value();
        ImGui::SameLine();
        ImGui::BeginChild("side", {0, available.y});
        const std::string_view lock =
            !tv ? "no video" : (tv->pictures().front().locked ? "locked" : "no lock");
        const DebugStats stats{static_cast<double>(status.simTime) /
                                   static_cast<double>(kPicosPerSecond),
                               speedRatio,
                               status.ringWaitShare,
                               uiMilliseconds,
                               lock,
                               StateName(status.state)};
        sidePanel.draw(session, view, boardView, loaded, stats, seed);
        ImGui::EndChild();
        ImGui::End();
        manualWindows.draw(toolbar.userManualOpen, loaded.manuals.manuals, boardView,
                           DesignOf(session));
        if (session.machine()) {
            analyzerWindow.draw(toolbar.analyzerOpen, session, boardView);
            meterWindow.draw(toolbar.meterOpen, session, boardView);
            scopeWindow.draw(toolbar.scopeOpen, session, boardView);
        }
        // Also without a Board (a netlist-only Session, the bench).
        keyboardWindow.draw(keyboardOpen, session, capture);
        capture.sync(window);
        matrixWindow.draw(matrixOpen, session, matrixCapture);
        matrixCapture.sync(window);
        if (!boardKeys.keys.empty() && !boardKeys.on && !ImGui::GetIO().WantTextInput &&
            ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
            boardKeys.on = true;
        }
        boardKeys.sync();
        toolbar.typing = capture.on         ? capture.reference
                         : matrixCapture.on ? matrixCapture.reference
                         : boardKeys.on     ? std::string("Board keys")
                                            : std::string();
        if (const auto picked = boardView.takePicked()) {
            wireStart.reset();
            toolbar.analyzerOpen |= *picked == Tool::Clip;
            toolbar.scopeOpen |= *picked == Tool::Hook;
            toolbar.meterOpen |= *picked == Tool::Meter;
        }

        ImGui::Render();
        const ImGuiIO& io = ImGui::GetIO();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        const auto now = std::chrono::steady_clock::now();
        uiMilliseconds = std::chrono::duration<double, std::milli>(now - frameStart).count();
        if (options.screenshot && now - start >= options.after) {
            std::println("bw_ui: t {:.3f} s  {:.2f}\u00d7  ui {:.1f} ms",
                         static_cast<double>(status.simTime) / static_cast<double>(kPicosPerSecond),
                         speedRatio, uiMilliseconds);
            if (!SaveScreenshot(renderer, *options.screenshot)) {
                std::println(stderr, "bw_ui: {}", SDL_GetError());
                exitCode = kExitErrors;
            }
            break;
        }
        SDL_RenderPresent(renderer);
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
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        SDL_Quit();
        return kExitErrors;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
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

    const int exitCode = RunWindow(window, renderer, loaded, session, *options, legends);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
