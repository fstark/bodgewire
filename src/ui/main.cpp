#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "board/kicad_board.hpp"
#include "machine/knob_setting.hpp"
#include "machine/machine.hpp"
#include "machine/machine_list.hpp"
#include "machine/press.hpp"
#include "machine/typing.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "netlist/units.hpp"
#include "parts/catalogue.hpp"
#include "session/session.hpp"
#include "text/text.hpp"
#include "ui/analyzer_window.hpp"
#include "ui/bench.hpp"
#include "ui/board_keys.hpp"
#include "ui/board_tags.hpp"
#include "ui/board_view.hpp"
#include "ui/brief_window.hpp"
#include "ui/debug_window.hpp"
#include "ui/gpu_render.hpp"
#include "ui/keyboard_window.hpp"
#include "ui/knob_window.hpp"
#include "ui/launcher.hpp"
#include "ui/loaded.hpp"
#include "ui/manual_window.hpp"
#include "ui/matrix_keyboard_window.hpp"
#include "ui/meter_window.hpp"
#include "ui/question_popup.hpp"
#include "ui/scope_window.hpp"
#include "ui/shelf.hpp"
#include "ui/title.hpp"
#include "ui/tv_view.hpp"
#include "ui/tv_window.hpp"
#include "ui/window_order.hpp"
#include "view/bench_options.hpp"
#include "view/contents_defaults.hpp"
#include "view/focus.hpp"
#include "view/memory_contents.hpp"
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

// How a bench ended (P7a): the app quits, or the launcher shows again.
enum class BenchEnd : std::uint8_t { Quit, Back };

struct BenchResult {
    BenchEnd end;
    int exitCode;
};

BenchResult RunBench(SDL_Window* window, GpuRender& gpu, const OpenedMachine& loaded,
                     Session& session, const Options& options, ImFont* legends, ImFont* petLegends,
                     bool canLeave) {
    ResetBench();
    TvView view(gpu.tvTexture());
    Shelf shelf;
    TvWindow tvWindow;
    tvWindow.setLoupe(options.bench.loupe);
    if (options.bench.tv) {
        tvWindow.controls() = *options.bench.tv;
    }
    KnobWindow knobWindow;
    bool debugOut = options.bench.debug;
    const Machine* machine = session.machine();
    BoardView boardView(machine ? std::optional<BoardScene>(std::in_place, *session.drawnBoard(),
                                                            machine->pristineBoard(),
                                                            machine->designCircuit())
                                : std::nullopt);
    std::uint64_t seed = 1;
    std::optional<PinRef> wireStart;  // Wire tool: the first pin
    PencilMarks marks;                // the grease pencil's (P6h)
    for (const auto& [shape, at] : options.bench.mark) {
        marks.add({at, shape, boardView.scene()->bodyAt(at)});  // a Board: checked in main()
    }
    Point wireStartAt{};
    std::set<std::string> latched;  // Buttons kept pressed by a right-click
    std::set<std::string> slid;     // SLIDE switches at ON, kept across power off and on
    std::vector<std::size_t> typedKeys(options.bench.type.size(), 0);  // --type: keys sent so far
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
    if (const BoardScene* scene = boardView.scene(); scene && options.bench.probe) {
        if (const auto component = scene->componentIndex(
                options.bench.probe->substr(0, options.bench.probe->find('.')))) {
            boardView.select(*component);
        }
    }
    RunSwitch run = options.bench.run;  // the shelf's switch (P6o)
    shelf.canLeave = canLeave;
    shelf.manualOut = options.bench.manual.has_value();
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
    boardView.setPlainLook(options.bench.plainLook);
    if (options.bench.zoom) {
        boardView.zoomAtStart(*options.bench.zoom);
    }
    if (options.bench.back) {
        boardView.toggleLayer();
    }
    Focus focus;
    if (!options.bench.clip.empty() || !options.bench.group.empty()) {
        focus.bringOut(Owner::Analyzer);
    }
    if (!options.bench.hook.empty()) {
        focus.bringOut(Owner::Scope);
    }
    if (!options.bench.meter.empty()) {
        focus.bringOut(Owner::Meter);
    }
    if (options.bench.focus && IsInstrument(*options.bench.focus)) {
        focus.bringOutFocused(*options.bench.focus);
    } else if (options.bench.focus) {
        focus.take(*options.bench.focus);
    }
    for (const BenchObject object : options.bench.pop) {  // the Machine has each: checked in main()
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
    for (const BenchObject object : options.bench.away) {
        (object == BenchObject::Tv         ? shelf.tvOut
         : object == BenchObject::Keyboard ? shelf.keyboardOut
                                           : shelf.knobOut) = false;
    }
    // The Board fitted beside the TV and the knob box when they come out on the bench at start
    // (their default places: the TV top right, the knob box top left), not under them.
    {
        const auto onBench = [&](BenchObject object) {
            return std::ranges::find(options.bench.pop, object) == options.bench.pop.end();
        };
        const float covered = 2 * kBenchMargin;
        const float left = shelf.knobOut && onBench(BenchObject::Knob)
                               ? covered + FirstSize(BenchObject::Knob, {}, 1,
                                                     static_cast<int>(session.knobs().size()))
                                               .width
                               : 0;
        const float right = shelf.tvOut && onBench(BenchObject::Tv) ? covered + kTvFirstWidth : 0;
        boardView.setCovered(left, right);
    }
    Owner lastOwner = focus.owner();
    ManualWindows manualWindows;
    if (options.bench.manual) {
        manualWindows.show(*options.bench.manual);
    }
    manualWindows.setStretch(options.bench.binderHeight);
    // The Contents page's defaults (P6p): none without the file; a bad one is ignored.
    if (const auto text = ReadTextFile(MachineDir(*options.file) / "roms" / "contents.txt")) {
        if (auto defaults = ParseContentsDefaults(*text, "contents.txt")) {
            manualWindows.setContentsDefaults(std::move(*defaults));
        } else {
            std::println(stderr, "bw_ui: warning: {}", defaults.error());
        }
    }
    if (options.bench.contents) {  // a memory chip on the Board: checked in main()
        const BoardScene& scene = *boardView.scene();
        manualWindows.openContents(scene, *scene.componentIndex(options.bench.contents->first),
                                   options.bench.contents->second);
        shelf.manualOut = true;
    }
    BriefNote briefNote;
    if (options.bench.brief) {
        briefNote.offset = ImVec2{options.bench.brief->first, options.bench.brief->second};
    }
    AnalyzerWindow analyzerWindow;
    MeterWindow meterWindow;
    ScopeWindow scopeWindow;
    analyzerWindow.preset(options.bench.view, options.bench.marker, options.bench.cursor);
    scopeWindow.preset(options.bench.marker, options.bench.cursor);
    boardView.setTooltip([&loaded](const BoardScene& scene, const Hit& hit) {
        return Tooltip(scene, hit, loaded.manuals.manuals);
    });
    // --run pause: paused once a picture is drawn, the switch kept at `‖` (PowerOn would move it).
    constexpr Time kPauseAt = 200 * kPicosPerSecond / 1000;
    if (run == RunSwitch::Max) {
        session.setSpeed(Speed::Max);
    }
    const bool continuity =
        options.bench.meter.size() == 2;  // the power left off: Continuity needs it
    if (!continuity && run == RunSwitch::Pause) {
        session.powerOn(seed, kPauseAt);
    } else if (!continuity) {
        PowerOn(session, run, seed);
    }
    // The power-off tools with the power on (P6o): asked, then cycled at the next frame's clicks
    // (so a frame is unpowered, as in a manual power cycle), powered on at the frame after.
    QuestionPopup popup("##power cycle",
                        "This tool needs the power off. OK powers the Machine off, uses the tool "
                        "and powers it on again; the Machine restarts.",
                        true);
    std::optional<RepairPlan> pending;    // asked, not answered
    std::optional<RepairPlan> cycleNext;  // OK: cycled at the next frame's clicks
    bool powerOnNext = false;
    bool askAtStart = options.bench.ask.has_value();
    // Back to the launcher (P7a): at once, or asked first when the Board holds work.
    QuestionPopup leaveQuestion(
        "##leave", "Back to the Machines? The repairs and pencil marks on this Board are lost.",
        false);
    bool leaveAtStart = options.bench.leave;
    // The Reset: every repair undone, asked first; with the power on, power-cycled around it as
    // a tool's repair is.
    QuestionPopup resetQuestion("##reset",
                                "Undo every repair on this Board? The pencil marks stay. With the "
                                "power on, the Machine restarts.",
                                false);
    bool resetAtStart = options.bench.reset;
    bool leaving = false;  // Back at the end of this frame
    const auto start = std::chrono::steady_clock::now();
    SpeedRatio ratio;
    double uiMilliseconds = 0;
    int exitCode = 0;
    long frames = 0;  // drawn, for the frame rate --screenshot prints
    BenchEnd ending = BenchEnd::Quit;

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

        gpu.newFrame();
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
            RepairPlan plan{{*options.bench.ask}, std::nullopt, ""};
            if (const auto* swap = std::get_if<SwapRepair>(&*options.bench.ask);
                swap && boardView.scene()) {
                plan.swapped = boardView.scene()->componentIndex(swap->reference);
            }
            pending = std::move(plan);
            const BenchRect area = BenchArea();
            popup.open({(area.min.x + area.max.x) / 2, (area.min.y + area.max.y) / 2});
        }
        // A memory chip's window (P6p): the binder at its contents, whatever the tool.
        if (const auto chip = boardView.takeContentsClick()) {
            manualWindows.openContents(*boardView.scene(), *chip);
            shelf.manualOut = true;
            BringToFront(kManualWindowName);
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
            boardView.setLeds(LedTagsOf(*scene, session));
        }
        {
            // Buttons: held or latched on the Board, or scripted by --press (also without a
            // Board).
            const BoardScene* scene = boardView.scene();
            if (!session.powered()) {
                latched.clear();
            }
            const auto toggled = boardView.takeLatchToggle();
            for (const std::string& reference : session.switches()) {
                const auto component =
                    scene ? scene->componentIndex(reference) : std::optional<std::size_t>();
                const Switch* sw = SwitchOf(session.circuit(), reference);
                const bool slide = sw != nullptr && sw->kind() == SwitchKind::Slide;
                if (slide) {
                    // A slide stays where it is moved, powered or not.
                    if (component && toggled == component && !slid.erase(reference)) {
                        slid.insert(reference);
                    }
                } else if (component && toggled == component && session.powered() &&
                           !latched.erase(reference)) {
                    latched.insert(reference);
                }
                const bool scripted =
                    std::ranges::any_of(options.bench.press, [&](const ButtonPress& p) {
                        return p.reference == reference && p.down <= status.simTime &&
                               status.simTime < p.up;
                    });
                session.setPressed(reference, latched.contains(reference) ||
                                                  slid.contains(reference) || scripted ||
                                                  (component && boardView.held() == component) ||
                                                  boardKeys.holds(reference));
            }
            if (scene) {
                boardView.setSwitches(SwitchTagsOf(*scene, session));
            }
        }
        {
            // Keys scripted by --type. Not in the first frame powered: its status.simTime is the
            // previous run's (the power-on is only queued).
            if (!session.powered()) {
                std::ranges::fill(typedKeys, 0);
            } else if (wasPowered) {
                for (std::size_t t = 0; t < options.bench.type.size(); ++t) {
                    const Typing& typing = options.bench.type[t];
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
            ShowRepairs(boardView, *machine);
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
        shelf.running = status.state == RunState::Running;
        shelf.speedRatio = speedRatio;
        shelf.draw(session, boardView, focus, run, seed);
        if (shelf.leaveClicked || leaveAtStart) {
            // Opening upwards from above the arrow, so it stays inside the main window and
            // leaves the shelf seen.
            if (shelf.leaveClicked && session.log().empty() && marks.marks().empty()) {
                leaving = true;
            } else {
                leaveQuestion.open(shelf.leaveAt, {0, 1});
            }
            shelf.leaveClicked = leaveAtStart = false;
        }
        if (shelf.resetClicked || resetAtStart) {
            resetQuestion.open(shelf.resetAt, {0, 1});
            shelf.resetClicked = resetAtStart = false;
        }
        ImGui::End();
        // The bench, back to front at start: brief, TV, knob, manuals, instruments, keyboard,
        // Debug.
        knobWindow.keys(session, knobKeys, knobsOn);
        DrawBrief(shelf.briefOut, loaded, boardView.corner(), briefNote);
        tvWindow.draw(shelf.tvOut, view);
        knobWindow.draw(shelf.knobOut, session);
        manualWindows.draw(shelf.manualOut, loaded.manuals.manuals, boardView, DesignOf(session),
                           session.log(), session.machine());
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
            if (meterWindow.takePowerClick()) {     // its power switch, in Continuity
                if (session.powered()) {
                    session.powerOff();
                } else {
                    PowerOn(session, run, seed);
                }
            }
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
        if (leaveQuestion.draw() == PopupAnswer::Ok) {
            leaving = true;
        }
        if (resetQuestion.draw() == PopupAnswer::Ok) {
            if (session.powered()) {
                session.powerOff();
                powerOnNext = true;
            }
            session.undoRepairs();
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
        gpu.present(&view);
        ++frames;
        if (leaving) {
            ending = BenchEnd::Back;
            break;
        }
        const auto now = std::chrono::steady_clock::now();
        const bool visited = options.visitFor && now - start >= *options.visitFor;
        if (visited || (options.screenshot && now - start >= options.after)) {
            std::println("bw_ui: t {:.3f} s  {:.2f}\u00d7  ui {:.1f} ms  {:.0f} fps",
                         static_cast<double>(status.simTime) / static_cast<double>(kPicosPerSecond),
                         speedRatio, uiMilliseconds,
                         static_cast<double>(frames) /
                             std::chrono::duration<double>(now - start).count());
            if (visited) {  // --visit: no shot, back to the launcher (P7a)
                ending = BenchEnd::Back;
            } else if (!gpu.screenshot(*options.screenshot)) {
                std::println(stderr, "bw_ui: {}", SDL_GetError());
                exitCode = kExitErrors;
            }
            break;
        }
    }
    SDL_StopTextInput(window);  // the keyboard capture's, if any: not the launcher's
    return {ending, exitCode};
}

// Today's bench flags applied to a command-line bench's Session, checked before the window
// opens: an exit code on a usage error.
std::optional<int> PrepareBench(Session& session, const Options& options) {
    if (options.bench.probe) {
        const auto pin = ParsePinRef(*options.bench.probe);
        std::optional<Point> at;
        if (session.machine() && pin) {
            at = session.machine()->padPosition(pin->reference, pin->pin);
        }
        if (!at) {
            std::println(stderr, "bw_ui: unknown pin '{}'", *options.bench.probe);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
        session.touch(ProbeTip{*at, Layer::Front});
    }
    if (options.bench.contents) {
        const std::string& reference = options.bench.contents->first;
        if (!session.machine() || !session.machine()->contents(reference)) {  // a ROM on a Board
            std::println(stderr, "bw_ui: no memory chip '{}'", reference);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    AnalyzerSettings analyzer;
    if (options.bench.sample) {
        analyzer.period = *options.bench.sample;
    }
    for (const std::string& item : options.bench.clip) {
        const std::size_t colon = item.find(':');
        const auto pin = ParsePinRef(std::string_view(item).substr(0, colon));
        const auto trigger = colon == std::string::npos
                                 ? std::optional(TriggerCondition::Any)
                                 : ParseTrigger(std::string_view(item).substr(colon + 1));
        const auto channel = pin ? session.clip(pin->reference, pin->pin) : std::nullopt;
        if (!trigger || !channel) {
            std::println(stderr, "bw_ui: unknown pin '{}'", item);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
        analyzer.trigger[*channel] = *trigger;
    }
    if (!options.bench.clip.empty() || !options.bench.group.empty()) {
        session.setAnalyzer(analyzer);
    }
    for (const std::string& item : options.bench.group) {
        const std::size_t colon = item.find(':');
        const auto pin = ParsePinRef(std::string_view(item).substr(0, colon));
        const auto clipped =
            pin ? std::optional(session.clipGroup(pin->reference, pin->pin)) : std::nullopt;
        const bool ok = clipped && *clipped &&
                        (colon == std::string::npos ||
                         ((*clipped)->group &&
                          session.setGroupTrigger(*(*clipped)->group, item.substr(colon + 1))));
        if (!ok) {
            std::println(stderr, "bw_ui: bad group '{}'{}", item,
                         clipped && !*clipped ? ": " + clipped->error() : "");
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    if (!options.bench.clip.empty() || !options.bench.group.empty()) {
        session.arm();
    }
    for (const std::string& item : options.bench.hook) {
        const auto pin = ParsePinRef(item);
        if (!pin || !session.hook(pin->reference, pin->pin)) {
            std::println(stderr, "bw_ui: unknown pin '{}'", item);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    if (options.bench.timebase || options.bench.delay) {
        ScopeSettings scope = session.scopeSettings();
        scope.timebase = options.bench.timebase.value_or(scope.timebase);
        scope.delay = options.bench.delay.value_or(scope.delay);
        if (scope.delay > kMaxDelayDivisions * scope.timebase) {
            std::println(stderr, "bw_ui: delay over {} divisions", kMaxDelayDivisions);
            return kExitUsage;
        }
        session.setScope(scope);
    }
    if (options.bench.meter.size() == 2) {  // red then black, before the power-on: none for these
        session.setMeterMode(MeterMode::Continuity);
    }
    for (const std::string& item : options.bench.meter) {
        const auto pin = ParsePinRef(item);
        const auto lead = pin ? session.padLead(pin->reference, pin->pin) : std::nullopt;
        if (!lead) {
            std::println(stderr, "bw_ui: unknown pin '{}'", item);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
        const bool chip = std::ranges::any_of(
            session.machine()->designCircuit().parts, [&](const LoadedPart& part) {
                return part.reference == lead->reference && part.info->kind == PartKind::Chip;
            });
        session.placeLead({lead->at, Layer::Front, lead->label + (chip ? " leg" : "")});
    }
    for (const std::string& text : options.bench.repair) {
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
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    if (options.bench.ask && !session.machine()) {
        std::println(stderr, "bw_ui: no Board to repair");
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }
    if (!options.bench.mark.empty() && !session.machine()) {
        std::println(stderr, "bw_ui: no Board to mark");
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }
    if (options.bench.tv && session.tv() == nullptr) {
        std::println(stderr, "bw_ui: this Machine has no TV");
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }
    for (const BenchObject object : options.bench.pop) {
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
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    for (const KnobSetting& knob : options.bench.knob) {
        if (!session.setKnob(knob.reference, knob.turn)) {
            std::println(stderr, "bw_ui: '{}' is not a paddle controller", knob.reference);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    for (const Typing& typing : options.bench.type) {
        if (!std::ranges::contains(session.keyboards(), typing.reference) &&
            !std::ranges::contains(session.matrixKeyboards(), typing.reference)) {
            std::println(stderr, "bw_ui: '{}' is not a keyboard", typing.reference);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
        if (typing.untypable && std::ranges::contains(session.keyboards(), typing.reference)) {
            std::println(stderr, "bw_ui: cannot type the text on '{}', a keyboard",
                         typing.reference);
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
    }
    return std::nullopt;
}

// ImGui for one screen (P7a), the launcher's or a bench's: made anew for each, so a bench starts
// fresh (a context remembers every window's place and size by name for its life).
class ImGuiScreen {
public:
    ImGuiScreen(SDL_Window* window, GpuRender& gpu) : gpu_(gpu) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        gpu.initImGui();
        KeepFocusFromTooltips();
        // The UI font first, as NewFrame would pick it lazily (13 px at the framebuffer scale:
        // vector from 15 px); AddFontDefault() itself, before the first frame, assumes Retina on
        // macOS.
        ImFontAtlas& fonts = *ImGui::GetIO().Fonts;
        if (SDL_GetWindowPixelDensity(window) * 13.0f >= 15.0f) {
            fonts.AddFontDefaultVector();
        } else {
            fonts.AddFontDefaultBitmap();
        }
        ImFontConfig legendConfig;
        legendConfig.FontDataOwnedByAtlas = false;  // static data, never written
        legends_ =
            fonts.AddFontFromMemoryTTF(const_cast<unsigned char*>(kKeycapFont),
                                       static_cast<int>(kKeycapFontSize), 0.0f, &legendConfig);
        petLegends_ =
            fonts.AddFontFromMemoryTTF(const_cast<unsigned char*>(kPetFont),
                                       static_cast<int>(kPetFontSize), 0.0f, &legendConfig);
        title_ = fonts.AddFontFromMemoryTTF(const_cast<unsigned char*>(kTitleFont),
                                            static_cast<int>(kTitleFontSize), 0.0f, &legendConfig);
    }
    ~ImGuiScreen() {
        gpu_.shutdownImGui();
        ImGui::DestroyContext();
    }
    ImGuiScreen(const ImGuiScreen&) = delete;
    ImGuiScreen& operator=(const ImGuiScreen&) = delete;

    ImFont* legends() const { return legends_; }
    ImFont* petLegends() const { return petLegends_; }
    ImFont* title() const { return title_; }  // the launcher's (Audiowide)

private:
    GpuRender& gpu_;
    ImFont* legends_ = nullptr;
    ImFont* petLegends_ = nullptr;
    ImFont* title_ = nullptr;
};

struct LauncherResult {
    std::optional<std::filesystem::path> file;  // clicked; none: quit
    int exitCode = 0;
};

// The launcher's frames (P7a), until an entry is clicked, the window closed, or --screenshot
// taken --after it shows (with the live run's speed line, P7b).
LauncherResult RunLauncher(SDL_Window* window, GpuRender& gpu, const ImGuiScreen& screen,
                           Launcher& launcher, const Options& options) {
    SDL_SetWindowTitle(window, "Bodgewire");
    TvView view(gpu.tvTexture());
    SpeedRatio ratio;
    double speedRatio = 0;
    double uiMilliseconds = 0;
    long frames = 0;
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        const auto frameStart = std::chrono::steady_clock::now();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window))) {
                return {};
            }
        }
        gpu.newFrame();
        ImGui::NewFrame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        // No padding (the lanes reach the window's edges), no wheel scrolling (P7b).
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("Launcher", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        const auto chosen = launcher.draw(view, screen.legends(), screen.title());
        ImGui::End();
        ImGui::Render();
        uiMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart)
                .count();
        const auto status = launcher.liveStatus();
        if (status) {
            speedRatio = ratio.update(status->simTime);
        }
        gpu.present(&view);
        ++frames;
        if (chosen) {
            return {chosen};
        }
        const auto now = std::chrono::steady_clock::now();
        if (options.screenshot && now - start >= options.after) {
            if (status) {
                std::println("bw_ui: t {:.3f} s  {:.2f}\u00d7  ui {:.1f} ms  {:.0f} fps",
                             static_cast<double>(status->simTime) /
                                 static_cast<double>(kPicosPerSecond),
                             speedRatio, uiMilliseconds,
                             static_cast<double>(frames) /
                                 std::chrono::duration<double>(now - start).count());
            }
            if (!gpu.screenshot(*options.screenshot)) {
                std::println(stderr, "bw_ui: {}", SDL_GetError());
                return {std::nullopt, kExitErrors};
            }
            return {};
        }
    }
}

// Where the launcher reads the Machines (P7a): --machines; else the machines/ holding the
// file's Machine; else found from the working directory up, then from the executable's.
std::optional<std::filesystem::path> MachinesDirFor(const Options& options) {
    if (options.machines) {
        return options.machines;
    }
    if (options.file) {
        const std::filesystem::path dir = MachineDir(*options.file);
        std::error_code error;
        const auto found = FindMachinesDir(dir);
        if (found && *found == std::filesystem::weakly_canonical(dir.parent_path(), error)) {
            return found;
        }
    }
    std::error_code error;
    if (const auto here = std::filesystem::current_path(error); !error) {
        if (auto found = FindMachinesDir(here)) {
            return found;
        }
    }
    if (const char* base = SDL_GetBasePath()) {  // needs no SDL_Init
        return FindMachinesDir(base);
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    const auto parsed = ParseOptions(args);
    if (!parsed) {
        if (!parsed.error().empty()) {
            std::println(stderr, "bw_ui: {}", parsed.error());
        }
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }
    const Options& options = *parsed;
    std::optional<OpenedMachine> loaded;  // the bench's; its Session ends before the next Load
    if (options.file) {
        loaded.emplace(Load(*options.file, options.roms));
        if (!loaded->session) {
            return kExitErrors;
        }
        if (const auto code = PrepareBench(*loaded->session, options)) {
            return *code;
        }
    }
    const std::optional<std::filesystem::path> machinesDir = MachinesDirFor(options);
    if (options.bench.leave && !machinesDir) {
        std::println(stderr, "bw_ui: --leave: no machines/ to go back to");
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }
    Launcher launcher;
    if (machinesDir) {
        MachineList list = ListMachines(*machinesDir);
        if (options.machines && list.machines.empty()) {
            std::println(stderr, "bw_ui: no Machines in '{}'", options.machines->string());
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
        launcher.setList(std::move(list));
    } else if (!options.file) {
        std::println(stderr, "bw_ui: no machines/ directory found");
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }
    std::deque<std::filesystem::path> visits;
    for (const std::filesystem::path& file : options.visit) {
        if (!EntryOf(launcher.list(), file)) {
            std::println(stderr, "bw_ui: '{}' is not a Machine or Puzzle of '{}'", file.string(),
                         machinesDir->string());
            std::println(stderr, "{}", Usage());
            return kExitUsage;
        }
        visits.push_back(file);
    }
    if (options.select && !EntryOf(launcher.list(), *options.select)) {
        std::println(stderr, "bw_ui: '{}' is not a Machine or Puzzle of '{}'",
                     options.select->string(), machinesDir->string());
        std::println(stderr, "{}", Usage());
        return kExitUsage;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        return kExitErrors;
    }
    SDL_Window* window =
        SDL_CreateWindow("Bodgewire", kWindowWidth, kWindowHeight, GpuRender::windowFlags());
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

    int exitCode = 0;
    std::optional<Options> benchOptions;  // the bench's flags: the command line's for its file
    if (loaded) {
        benchOptions = options;
    }
    // The last bench's file: its entry selected; at start --select's (P7b).
    std::optional<std::filesystem::path> left = options.select;
    while (true) {
        if (!loaded) {
            std::filesystem::path file;
            const bool visiting = !visits.empty();
            if (visiting) {
                file = visits.front();
                visits.pop_front();
            } else {
                MachineList list = ListMachines(*machinesDir);
                PrintDiagnostics(list.problems);
                launcher.setList(std::move(list));
                if (left) {
                    launcher.select(*left);
                }
                ImGuiScreen screen(window, *gpu);
                const LauncherResult chosen = RunLauncher(window, *gpu, screen, launcher, options);
                launcher.stop();  // the live run's Session ends before the bench's Load
                if (!chosen.file) {
                    exitCode = chosen.exitCode != 0 ? chosen.exitCode : exitCode;
                    break;
                }
                file = *chosen.file;
            }
            loaded.emplace(Load(file, std::nullopt));
            left = file;
            if (!loaded->session) {
                loaded.reset();
                launcher.setError("Could not load " + file.filename().string() +
                                  ": see the terminal");
                if (visiting) {
                    exitCode = kExitErrors;
                }
                continue;
            }
            // The working Machine by its listed name ("Counter", not the netlist's "counter").
            if (const auto entry = EntryOf(launcher.list(), file); entry && !entry->puzzle) {
                loaded->title = launcher.list().machines[entry->machine].name;
            }
            benchOptions = Options{};
            benchOptions->file = file;
            benchOptions->machines = options.machines;
            if (visiting) {
                benchOptions->visitFor = options.after;
            }
        }
        SDL_SetWindowTitle(window, ("Bodgewire \u2014 " + loaded->title).c_str());
        BenchResult result{};
        {
            ImGuiScreen screen(window, *gpu);
            result = RunBench(window, *gpu, *loaded, *loaded->session, *benchOptions,
                              screen.legends(), screen.petLegends(), machinesDir.has_value());
        }
        left = *benchOptions->file;
        loaded.reset();
        exitCode = result.exitCode != 0 ? result.exitCode : exitCode;
        if (result.end == BenchEnd::Quit || !machinesDir) {
            break;
        }
    }

    gpu.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
