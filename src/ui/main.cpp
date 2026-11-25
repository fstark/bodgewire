#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
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
#include "session/procedural_puzzle.hpp"
#include "session/session.hpp"
#include "text/text.hpp"
#include "ui/analyzer_window.hpp"
#include "ui/bench.hpp"
#include "ui/bench_input.hpp"
#include "ui/bench_setup.hpp"
#include "ui/board_keys.hpp"
#include "ui/board_tags.hpp"
#include "ui/board_view.hpp"
#include "ui/brief_window.hpp"
#include "ui/controls.hpp"
#include "ui/debug_window.hpp"
#include "ui/gpu_render.hpp"
#include "ui/keyboard_window.hpp"
#include "ui/knob_window.hpp"
#include "ui/launcher.hpp"
#include "ui/loaded.hpp"
#include "ui/manual_window.hpp"
#include "ui/matrix_keyboard_window.hpp"
#include "ui/meter_window.hpp"
#include "ui/progress_store.hpp"
#include "ui/question_popup.hpp"
#include "ui/scope_window.hpp"
#include "ui/shelf.hpp"
#include "ui/thank_you_card.hpp"
#include "ui/title.hpp"
#include "ui/tool_click.hpp"
#include "ui/touch.hpp"
#include "ui/tv_view.hpp"
#include "ui/tv_window.hpp"
#include "ui/window_order.hpp"
#include "view/bench_options.hpp"
#include "view/contents_defaults.hpp"
#include "view/focus.hpp"
#include "view/memory_contents.hpp"
#include "view/pencil_marks.hpp"
#include "view/power_cycle.hpp"
#include "view/repair_ack.hpp"
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

// A screen's frame timing (the launcher's, a bench's): the UI's work per frame and the frame
// rate, for the line --screenshot prints.
class FrameClock {
public:
    void frameStarts() { frameStart_ = Clock::now(); }
    // After ImGui::Render, before presenting: the frame built, not drawn (the GPU's waits for the
    // windows' vsync would count otherwise, P6j).
    void built() {
        uiMilliseconds_ =
            std::chrono::duration<double, std::milli>(Clock::now() - frameStart_).count();
        ++frames_;
    }
    double uiMilliseconds() const { return uiMilliseconds_; }
    std::chrono::steady_clock::duration elapsed() const { return Clock::now() - start_; }
    // "bw_ui: t 0.993 s  1.00×  ui 0.5 ms  114 fps".
    void print(Time simTime, double speedRatio) const {
        std::println("bw_ui: t {:.3f} s  {:.2f}\u00d7  ui {:.1f} ms  {:.0f} fps",
                     static_cast<double>(simTime) / static_cast<double>(kPicosPerSecond),
                     speedRatio, uiMilliseconds_,
                     static_cast<double>(frames_) /
                         std::chrono::duration<double>(Clock::now() - start_).count());
    }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_ = Clock::now(), frameStart_ = start_;
    double uiMilliseconds_ = 0;
    long frames_ = 0;
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

// How a bench ended (P7a): the app quits, the launcher shows again, or (P8c) the Thank-you Card
// opens the Machine's next Puzzle.
enum class BenchEnd : std::uint8_t { Quit, Back, Next };

struct BenchResult {
    BenchEnd end;
    int exitCode;
    std::optional<std::filesystem::path> next = {};  // with Next: its file
};

BenchResult RunBench(SDL_Window* window, GpuRender& gpu, const OpenedMachine& loaded,
                     Session& session, const Options& options, ImFont* legends, ImFont* petLegends,
                     const MachineList* list,                // nullptr: no machines/ to go back to
                     const std::function<void()>& repaired,  // at the tick: saved (P8e)
                     std::span<const FingerScript> fingers) {  // scripted, this screen's (P8f)
    ResetBench();
    TouchStart(window, options.touch, fingers);
    TvView view(gpu.tvTexture());
    Shelf shelf;
    Focus focus;  // the focus owner, and what is out on the bench
    focus.outFlag(BenchObject::Debug) = options.bench.debug;
    TvWindow tvWindow;
    tvWindow.setLoupe(options.bench.loupe);
    if (options.bench.tv) {
        tvWindow.controls() = *options.bench.tv;
    }
    KnobWindow knobWindow;
    const Machine* machine = session.machine();
    BoardView boardView(machine ? std::optional<BoardScene>(std::in_place, *session.drawnBoard(),
                                                            machine->pristineBoard(),
                                                            machine->designCircuit())
                                : std::nullopt);
    std::uint64_t seed = 1;
    WireDraft wire;     // the Wire tool's first pin
    PencilMarks marks;  // the grease pencil's (P6h)
    for (const auto& [shape, at] : options.bench.mark) {
        marks.add({at, shape, boardView.scene()->bodyAt(at)});  // a Board: checked in main()
    }
    SwitchHands switchHands;
    ScriptedTyping scriptedTyping(options.bench.type);
    KeyboardCapture capture;
    if (const auto keyboards = session.inputs().keyboards(); !keyboards.empty()) {
        capture.reference = keyboards.front();
    }
    KeyboardWindow keyboardWindow(legends);
    MatrixCapture matrixCapture;  // the BW-2001 keyboard (P5q)
    if (const auto matrices = session.inputs().matrixKeyboards(); !matrices.empty()) {
        matrixCapture.reference = matrices.front();
        matrixCapture.kind =
            session.inputs().matrixKind(matrices.front()).value_or(KeyMatrixKind::Bw2001);
    }
    MatrixKeyboardWindow matrixWindow(legends, petLegends);
    BoardKeyCapture boardKeys;  // only without a keyboard: that one takes the typing
    if (capture.reference.empty() && matrixCapture.reference.empty()) {
        boardKeys.keys = session.inputs().switchKeys();
    }
    boardView.setLegendFont(legends);
    focus.outFlag(BenchObject::Keyboard) =
        !capture.reference.empty() || !matrixCapture.reference.empty();
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
    const bool canLeave = list != nullptr;
    shelf.canLeave = canLeave;
    // Repair Validation (P8c): the tick on the post-it, the Thank-you Card once.
    const PuzzleEntry* nextPuzzle =
        list && options.file ? NextPuzzle(*list, *options.file) : nullptr;
    // The card's Next: the Machine's next Puzzle; on a Random fault, another one (P8d), made by
    // the Launcher (so only with one to go back to).
    const bool randomFault = !loaded.fault.empty() && options.file.has_value();
    std::optional<std::string> nextLabel;
    if (nextPuzzle != nullptr) {
        nextLabel = "Next: " + nextPuzzle->title;
    } else if (randomFault && list != nullptr) {
        nextLabel = "Another random fault";
    }
    RepairAck ack(session.puzzle());
    bool wasTicked = false;
    ThankYouCard card;
    bool cardDue = false;                            // waits for any open question to be answered
    std::optional<std::filesystem::path> following;  // the card's Next
    const auto benchStart = std::chrono::steady_clock::now();
    focus.outFlag(BenchObject::Manual) = options.bench.manual.has_value();
    focus.outFlag(BenchObject::Tv) = session.tv() != nullptr;
    focus.outFlag(BenchObject::Knob) = !session.inputs().knobs().empty();
    // Every PC key goes to the Machine (P6o): its keyboard, its Board keys, its knobs, out or put
    // away; but while a text field is edited, a Cmd combination, Tab (the plain look) and ` (the
    // Board flipped). A Machine with both a keyboard and knobs (none yet) would give the knobs only
    // the keys the keyboard has no use for.
    KnobKeys knobKeys;
    const bool hasKnobs = !session.inputs().knobs().empty();
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
    shelf.hasBrief = focus.outFlag(BenchObject::Brief) = !loaded.brief.empty();
    boardView.setPlainLook(options.bench.plainLook);
    if (options.bench.zoom) {
        boardView.zoomAtStart(*options.bench.zoom);
    }
    if (options.bench.back) {
        boardView.toggleLayer();
    }
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
        focus.bringOut(object);
        PopOutAtStart(object);
    }
    for (const BenchObject object : options.bench.away) {
        focus.outFlag(object) = false;
    }
    // The Board fitted beside the TV and the knob box when they come out on the bench at start
    // (their default places: the TV top right, the knob box top left), not under them.
    {
        const auto onBench = [&](BenchObject object) {
            return std::ranges::find(options.bench.pop, object) == options.bench.pop.end();
        };
        const float covered = 2 * kBenchMargin;
        const float left =
            focus.out(BenchObject::Knob) && onBench(BenchObject::Knob)
                ? covered + FirstSize(BenchObject::Knob, {}, 1,
                                      static_cast<int>(session.inputs().knobs().size()))
                                .width
                : 0;
        const float right =
            focus.out(BenchObject::Tv) && onBench(BenchObject::Tv) ? covered + kTvFirstWidth : 0;
        boardView.setCovered(left, right);
    }
    Owner lastOwner = focus.owner();
    ManualWindows manualWindows;
    if (options.bench.manual) {
        manualWindows.show(*options.bench.manual);
    }
    manualWindows.setStretch(options.bench.binderHeight);
    if (options.bench.showFault) {
        manualWindows.showFault();
    }
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
        focus.bringOut(BenchObject::Manual);
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
    FrameClock clock;
    SpeedRatio ratio;
    int exitCode = 0;
    BenchEnd ending = BenchEnd::Quit;

    bool running = true;
    while (running) {
        clock.frameStarts();
        SDL_Event event;
        if (powerOnNext) {  // before session.tv(): a repaired Machine's power-on rebuilds it
            PowerOn(session, run, seed);
            powerOnNext = false;
        }
        ack.update(
            session.completed(), session.powered(), session.powerOns(),
            std::chrono::duration<double>(std::chrono::steady_clock::now() - benchStart).count());
        cardDue = ack.takeCard() || cardDue;
        if (ack.ticked() && !wasTicked && repaired) {
            repaired();
        }
        wasTicked = ack.ticked();
        applyKeys();
        while (SDL_PollEvent(&event)) {
            if (TouchEvent(event)) {
                continue;  // a finger's: the one pointer comes from TouchNewFrame (P8f)
            }
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
        const RunnerStatus status = session.status();
        const double speedRatio = ratio.update(status.simTime);

        gpu.newFrame();
        TouchNewFrame();
        ImGui::NewFrame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        OnBench();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Bodgewire", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (focus.owner() != lastOwner) {
            wire.start.reset();  // a wire is started with one tool only
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
            focus.bringOut(BenchObject::Manual);
            BringToFront(kManualWindowName);
        }
        if (auto click = boardView.takeClick()) {
            // The analyzer's pin / bus switch on bus (P8f): as a Shift-click.
            click->shift =
                click->shift || (focus.owner() == Owner::Analyzer && analyzerWindow.bus());
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
            const std::optional<RepairPlan> plan =
                ToolClick(focus.owner(), *click, session, boardView, marks, wire, refuse);
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
        const auto tip = session.probe().tip();
        boardView.setProbeTip(tip ? std::optional(tip->at) : std::nullopt);
        boardView.setBlobs(session.blobs());
        boardView.setClips(ClipTagsOf(session));
        boardView.setHooks(HookTagsOf(session));
        boardView.setLeads(LeadTagsOf(session));
        if (const BoardScene* scene = boardView.scene()) {
            boardView.setLeds(LedTagsOf(*scene, session));
        }
        switchHands.update(session, boardView, options.bench.press, boardKeys, status.simTime);
        scriptedTyping.update(session, status.simTime, matrixCapture.reference, capture.reference,
                              keyboardWindow);
        if (machine) {
            ShowRepairs(boardView, *machine);
        }
        boardView.setWireStart(wire.start ? std::optional(wire.at) : std::nullopt);
        shelf.wireStarted = wire.start.has_value();
        const std::string_view lock =
            !tv ? "no video" : (tv->pictures().front().locked ? "locked" : "no lock");
        const DebugStats stats{static_cast<double>(status.simTime) /
                                   static_cast<double>(kPicosPerSecond),
                               speedRatio,
                               status.ringWaitShare,
                               clock.uiMilliseconds(),
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
        DrawBrief(focus.outFlag(BenchObject::Brief), loaded, boardView.corner(), briefNote,
                  ack.ticked());
        tvWindow.draw(focus.outFlag(BenchObject::Tv), view);
        knobWindow.draw(focus.outFlag(BenchObject::Knob), session);
        manualWindows.draw(focus.outFlag(BenchObject::Manual), loaded.manuals.manuals, boardView,
                           DesignOf(session), session.log(), session.machine(),
                           loaded.fault.empty() ? nullptr : &loaded.fault);
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
        bool keyboardOpen = focus.out(BenchObject::Keyboard) && !capture.reference.empty();
        bool matrixOpen = focus.out(BenchObject::Keyboard) && !matrixCapture.reference.empty();
        keyboardWindow.draw(keyboardOpen, session, capture);
        matrixWindow.draw(matrixOpen, session, matrixCapture);
        if (focus.out(BenchObject::Keyboard) && !keyboardOpen && !matrixOpen) {
            focus.outFlag(BenchObject::Keyboard) = false;  // closed by its window
        }
        DrawDebug(focus.outFlag(BenchObject::Debug), session, stats, seed);
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
        // Opening a popup closes any other: the card waits for an open question's answer.
        if (cardDue && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
            card.open();
            cardDue = false;
        }
        switch (card.draw(loaded.title, nextLabel ? &*nextLabel : nullptr, canLeave)) {
        case CardAnswer::Next:
            following = nextPuzzle != nullptr ? nextPuzzle->file : *options.file;
            break;
        case CardAnswer::Machines:
            leaving = true;  // no leave question: the repairs were the point
            break;
        case CardAnswer::Board:
        case CardAnswer::None:
            break;
        }
        // A text field takes the keys from the next event on (P6o).
        textField = ImGui::GetIO().WantTextInput || NumberPadOpen();  // the pad's keys (P8f)
        applyKeys();
        // Text input on the window with the keyboard focus: a popped-out keyboard's (P6j).
        SDL_Window* focused = SDL_GetKeyboardFocus();
        capture.sync(focused != nullptr ? focused : window);
        matrixCapture.sync(focused != nullptr ? focused : window);
        boardKeys.sync();

        ImGui::Render();
        clock.built();
        gpu.present(&view);
        if (leaving) {
            ending = BenchEnd::Back;
            break;
        }
        if (following) {
            ending = BenchEnd::Next;
            break;
        }
        const auto elapsed = clock.elapsed();
        const bool visited = options.visitFor && elapsed >= *options.visitFor;
        if (visited || (options.screenshot && elapsed >= options.after)) {
            clock.print(status.simTime, speedRatio);
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
    return {ending, exitCode, following};
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
        ApplyStyle();  // one size of controls (P8f)
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
    std::optional<LauncherChoice> choice;  // clicked (a Random fault: made); none: quit
    int exitCode = 0;
};

// The launcher's frames (P7a), until an entry is clicked, the window closed, or --screenshot
// taken --after it shows (with the live run's speed line, P7b).
LauncherResult RunLauncher(SDL_Window* window, GpuRender& gpu, const ImGuiScreen& screen,
                           Launcher& launcher, const Options& options,
                           std::span<const FingerScript> fingers) {
    SDL_SetWindowTitle(window, "Bodgewire");
    TouchStart(window, options.touch, fingers);
    TvView view(gpu.tvTexture());
    SpeedRatio ratio;
    double speedRatio = 0;
    FrameClock clock;
    if (options.screenshot) {
        launcher.holdChoices();  // the shot is the Launcher's: no bench opens before it
    }
    if (options.forget) {
        launcher.askForget();  // --forget: the question open for the shot (P8e)
    }
    if (options.make) {
        launcher.make(options.seed);  // --make: the selected Random fault's making, at once
    }
    while (true) {
        clock.frameStarts();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (TouchEvent(event)) {
                continue;
            }
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window))) {
                return {};
            }
        }
        gpu.newFrame();
        TouchNewFrame();
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
        auto chosen = launcher.draw(view, screen.legends(), screen.title());
        ImGui::End();
        ImGui::Render();
        clock.built();
        const auto status = launcher.liveStatus();
        if (status) {
            speedRatio = ratio.update(status->simTime);
        }
        gpu.present(&view);
        if (chosen && !options.screenshot) {
            return {std::move(chosen)};
        }
        if (options.screenshot && clock.elapsed() >= options.after) {
            if (status) {
                clock.print(status->simTime, speedRatio);
            }
            if (!gpu.screenshot(*options.screenshot)) {
                std::println(stderr, "bw_ui: {}", SDL_GetError());
                return {std::nullopt, kExitErrors};
            }
            return {};
        }
    }
}

// A Random fault for the Vetting Script `file` (P8d), made on the spot for a bench opened without
// the Launcher's making (the terminal, --visit): the window waits. The error printed; nullopt.
std::optional<FaultDecl> MakeFault(const std::filesystem::path& file, std::uint64_t seed,
                                   const std::optional<std::filesystem::path>& roms) {
    const auto made = MakeProceduralPuzzleFor(file, seed, roms);
    if (!made) {
        std::println(stderr, "bw_ui: {}: {}", file.filename().string(), made.error());
        return std::nullopt;
    }
    return (*made)->fault;
}

bool IsVettingScript(const std::filesystem::path& file) { return file.extension() == ".bwr"; }

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
        std::optional<FaultDecl> fault;
        if (IsVettingScript(*options.file)) {
            fault = MakeFault(*options.file, options.seed.value_or(1), options.roms);
            if (!fault) {
                return kExitErrors;
            }
        }
        loaded.emplace(Load(*options.file, options.roms, fault ? &*fault : nullptr));
        if (!loaded->session) {
            return kExitErrors;
        }
        if (const auto applied = ApplyBenchOptions(*loaded->session, options.bench); !applied) {
            std::println(stderr, "bw_ui: {}", applied.error());
            std::println(stderr, "{}", Usage());
            return kExitUsage;
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

    TouchInit();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        return kExitErrors;
    }
    SDL_Window* window = SDL_CreateWindow(
        "Bodgewire", options.window ? options.window->first : kWindowWidth,
        options.window ? options.window->second : kWindowHeight, GpuRender::windowFlags());
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

    // Saved progress (P8e): the Puzzles repaired, kept across launches.
    ProgressStore store = ProgressStore::For(options);
    Progress progress = store.load();
    launcher.setProgress(&progress, [&] { store.clear(progress); });

    int exitCode = 0;
    std::optional<Options> benchOptions;  // the bench's flags: the command line's for its file
    if (loaded) {
        benchOptions = options;
    }
    // The last bench's file: its entry selected; at start --select's (P7b).
    std::optional<std::filesystem::path> left = options.select;
    std::optional<std::filesystem::path> next;  // the Thank-you Card's Next (P8c)
    // The scripted fingers (P8f) are the first screen's: taken once.
    std::span<const FingerScript> fingers = options.fingers;
    while (true) {
        if (!loaded) {
            std::filesystem::path file;
            std::optional<FaultDecl> fault;
            // The card's "Another random fault" (P8d): back through the Launcher, which makes it.
            const bool another = next.has_value() && IsVettingScript(*next);
            const bool following = next.has_value() && !another;
            const bool visiting = !following && !another && !visits.empty();
            if (another) {
                left = *next;
                next.reset();
            }
            if (following) {
                file = *next;
                next.reset();
            } else if (visiting) {
                file = visits.front();
                visits.pop_front();
            } else {
                MachineList list = ListMachines(*machinesDir);
                PrintDiagnostics(list.problems);
                launcher.setList(std::move(list));
                if (left) {
                    launcher.select(*left);
                }
                if (another) {
                    launcher.make();
                }
                ImGuiScreen screen(window, *gpu);
                LauncherResult chosen = RunLauncher(window, *gpu, screen, launcher, options,
                                                    std::exchange(fingers, {}));
                launcher.stop();  // the live run's Session ends before the bench's Load
                if (!chosen.choice) {
                    exitCode = chosen.exitCode != 0 ? chosen.exitCode : exitCode;
                    break;
                }
                file = std::move(chosen.choice->file);
                fault = std::move(chosen.choice->fault);
            }
            // A Vetting Script with no Fault made for it (a visit): one made here.
            if (IsVettingScript(file) && !fault) {
                fault = MakeFault(file, options.seed.value_or(1), std::nullopt);
            }
            loaded.emplace(Load(file, std::nullopt, fault ? &*fault : nullptr));
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
            if (const auto entry = EntryOf(launcher.list(), file);
                entry && !entry->puzzle && !entry->random) {
                loaded->title = launcher.list().machines[entry->machine].name;
            }
            benchOptions = Options{};
            benchOptions->file = file;
            benchOptions->machines = options.machines;
            benchOptions->touch = options.touch;  // a finger stays a finger (P8f)
            if (visiting) {
                benchOptions->visitFor = options.after;
            }
        }
        SDL_SetWindowTitle(window, ("Bodgewire \u2014 " + loaded->title).c_str());
        BenchResult result{};
        const std::optional<std::string> key =
            machinesDir ? ProgressKey(launcher.list(), *benchOptions->file) : std::nullopt;
        const auto repaired = [&] {
            if (key && !progress.has(*key)) {
                store.add(progress, *key);
            }
        };
        {
            ImGuiScreen screen(window, *gpu);
            result =
                RunBench(window, *gpu, *loaded, *loaded->session, *benchOptions, screen.legends(),
                         screen.petLegends(), machinesDir ? &launcher.list() : nullptr, repaired,
                         std::exchange(fingers, {}));
        }
        left = *benchOptions->file;
        loaded.reset();
        exitCode = result.exitCode != 0 ? result.exitCode : exitCode;
        if (result.end == BenchEnd::Next) {
            next = result.next;
        }
        if (result.end == BenchEnd::Quit || !machinesDir) {
            break;
        }
    }

    gpu.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
