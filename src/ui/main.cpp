#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "netlist/units.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"
#include "ui/loaded.hpp"
#include "ui/manual_view.hpp"
#include "ui/side_panel.hpp"
#include "ui/toolbar.hpp"
#include "ui/tv_view.hpp"

using namespace bw;

namespace {

constexpr int kExitErrors = 1;
constexpr int kExitUsage = 2;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr float kBoardShare = 2.0f / 3.0f;
constexpr std::string_view kUsage =
    "usage: bw_ui <file.bwn|file.bwp> [--probe REF.PIN] [--repair swap:REF,mend:LAYER:X:Y] "
    "[--manual user|service] [--screenshot OUT.png [--after 1s]]";

struct Options {
    std::filesystem::path file;
    std::optional<std::filesystem::path> screenshot;
    std::chrono::nanoseconds after{std::chrono::seconds(1)};
    std::optional<std::string> probe;  // REF.PIN
    std::vector<std::string> repair;
    bool userManual = false, serviceManual = false;
};

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
        if (arg == "--screenshot") {
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
            const std::string_view which = argv[++i];
            if (which == "user") {
                options.userManual = true;
            } else if (which == "service") {
                options.serviceManual = true;
            } else {
                return std::nullopt;
            }
        } else if (arg == "--repair") {
            const std::string_view list = argv[++i];
            for (std::size_t start = 0;;) {
                const std::size_t comma = list.find(',', start);
                options.repair.emplace_back(list.substr(start, comma - start));
                if (comma == std::string_view::npos) {
                    break;
                }
                start = comma + 1;
            }
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
Loaded Load(const std::filesystem::path& file) {
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
        machine = LoadMachine(*puzzle, MachineRoot(file));
    } else if (HasBoard(file)) {
        machine = LoadNetlistMachine(file);
    } else {
        auto result = LoadNetlistFile(file);
        if (!result) {
            Print(result.error());
            return {};
        }
        Print((*result)->warnings);
        LoadedManuals manuals = LoadManuals(MachineRoot(file), file.stem().string(), **result);
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
    LoadedManuals manuals =
        LoadManuals(MachineRoot(file), machine->name(), machine->designCircuit());
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

// Board hover text plus the Component's role, or the pin's Datasheet line.
std::string Tooltip(const BoardScene& scene, const Hit& hit, const Manuals& manuals) {
    std::string text = scene.hoverText(hit);
    if (hit.kind == HitKind::Component && manuals.service) {
        if (const auto role = ComponentRole(*manuals.service,
                                            scene.board().components[hit.component].reference)) {
            text += " \u00b7 " + role->name;
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
              const Options& options) {
    TvView view(renderer);
    Toolbar toolbar;
    SidePanel sidePanel;
    const Machine* machine = session.machine();
    BoardView boardView(machine ? std::optional<BoardScene>(std::in_place, *session.drawnBoard(),
                                                            machine->pristineBoard(),
                                                            machine->designCircuit())
                                : std::nullopt);
    std::uint64_t seed = 1;
    if (const BoardScene* scene = boardView.scene(); scene && options.probe) {
        if (const auto component =
                scene->componentIndex(options.probe->substr(0, options.probe->find('.')))) {
            boardView.select(*component);
        }
    }
    Speed speed = Speed::RealTime;
    toolbar.userManualOpen = options.userManual;
    toolbar.serviceManualOpen = options.serviceManual;
    ManualWindows manualWindows;
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
        if (boardView.takeEscape()) {
            session.touch(std::nullopt);
        }
        if (const auto click = boardView.takeClick()) {
            if (click->component) {
                boardView.select(*click->component);
            }
            const auto& pick = click->pick;
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
            }
            if (result == RepairResult::Refused) {
                toolbar.refused();
            }
        }
        const auto tip = session.probeTip();
        boardView.setProbeTip(tip ? std::optional(tip->at) : std::nullopt);
        boardView.setBlobs(session.blobs());
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
        manualWindows.draw(toolbar.userManualOpen, toolbar.serviceManualOpen,
                           loaded.manuals.manuals, boardView, DesignOf(session));

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
    Loaded loaded = Load(options->file);
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
    for (const std::string& text : options->repair) {
        const auto repair = ParseRepair(text);
        std::optional<std::string> error;
        if (!repair) {
            error = repair.error();
        } else if (session.repair(*repair) != RepairResult::Done) {
            error = !session.machine()
                        ? std::string("no Board to repair")
                        : "unknown reference '" + std::get<SwapRepair>(*repair).reference + "'";
        }
        if (error) {
            std::println(stderr, "bw_ui: {}", *error);
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

    const int exitCode = RunWindow(window, renderer, loaded, session, *options);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
