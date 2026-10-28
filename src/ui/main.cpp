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
#include "runner/runner.hpp"
#include "runner/tv_consumer.hpp"
#include "ui/board_view.hpp"
#include "ui/tv_view.hpp"

using namespace bw;

namespace {

constexpr int kExitErrors = 1;
constexpr int kExitUsage = 2;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr float kBoardShare = 2.0f / 3.0f;

struct Options {
    std::filesystem::path file;
    std::optional<std::filesystem::path> screenshot;
    std::chrono::nanoseconds after{std::chrono::seconds(1)};
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
        } else {
            return std::nullopt;
        }
    }
    if (after && !options.screenshot) {
        return std::nullopt;
    }
    return options;
}

struct Loaded {
    std::optional<Machine> machine;
    std::unique_ptr<LoadedCircuit> circuit;
    std::string name;
};

void Print(std::span<const Diagnostic> diagnostics) {
    for (const Diagnostic& d : diagnostics) {
        std::println(stderr, "{}", Format(d));
    }
}

// Diagnostics printed; no circuit on errors.
Loaded Load(const std::filesystem::path& file) {
    std::expected<Machine, std::vector<Diagnostic>> machine =
        std::unexpected(std::vector<Diagnostic>{});
    if (file.extension() == ".bwp") {
        machine = LoadPuzzleFile(file);
    } else if (HasBoard(file)) {
        machine = LoadNetlistMachine(file);
    } else {
        auto result = LoadNetlistFile(file);
        if (!result) {
            Print(result.error());
            return {};
        }
        Print((*result)->warnings);
        return {std::nullopt, std::move(*result), file.stem().string()};
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
    std::string name(machine->name());
    return {std::move(*machine), std::move(*built), std::move(name)};
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

// Saves the whole framebuffer; false with SDL's error on failure.
bool SaveScreenshot(SDL_Renderer* renderer, const std::filesystem::path& path) {
    SDL_SetRenderScale(renderer, 1, 1);
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    const bool ok = surface != nullptr && SDL_SavePNG(surface, path.string().c_str());
    SDL_DestroySurface(surface);
    return ok;
}

int RunWindow(SDL_Window* window, SDL_Renderer* renderer, Loaded& loaded, const Options& options) {
    TvView view(renderer);
    const auto video = ConnectorNet(*loaded.circuit, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(*loaded.circuit, "VIDEO", "SYNC_N");
    std::optional<TvConsumer> tv;
    if (video && syncN) {
        tv.emplace(*video, *syncN);
    }
    Runner runner(loaded.circuit->circuit,
                  tv ? std::vector<Consumer*>{&*tv} : std::vector<Consumer*>{});
    BoardView boardView(loaded.machine
                            ? std::optional<BoardScene>(std::in_place, loaded.machine->board(),
                                                        loaded.machine->pristineBoard(),
                                                        loaded.machine->designCircuit())
                            : std::nullopt);
    std::uint64_t seed = 1;
    Speed speed = Speed::RealTime;
    runner.powerOn(seed);
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
        if (tv && tv->pictures().update()) {
            view.show(tv->pictures().front());
        }
        const RunnerStatus status = runner.status();
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

        const ImVec2 available = ImGui::GetContentRegionAvail();
        boardView.draw({available.x * kBoardShare, available.y});
        ImGui::SameLine();
        ImGui::BeginChild("side", {0, available.y});
        const float sideWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 tvSize{sideWidth, sideWidth * 0.75f};
        if (tv) {
            view.draw(tvSize);
        } else {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImDrawList& list = *ImGui::GetWindowDrawList();
            list.AddRectFilled(at, {at.x + tvSize.x, at.y + tvSize.y}, IM_COL32_BLACK);
            const char* text = "no video";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            list.AddText({at.x + (tvSize.x - textSize.x) / 2, at.y + (tvSize.y - textSize.y) / 2},
                         IM_COL32_WHITE, text);
            ImGui::Dummy(tvSize);
        }

        boardView.controls();

        if (status.state == RunState::Off) {
            if (ImGui::Button("Power on")) {
                runner.powerOn(seed);
            }
        } else if (ImGui::Button("Power off")) {
            runner.powerOff();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(status.state == RunState::Off);
        if (status.state == RunState::Paused) {
            if (ImGui::Button("Resume")) {
                runner.resume();
            }
        } else if (ImGui::Button("Pause")) {
            runner.pause();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar("Seed", ImGuiDataType_U64, &seed);
        ImGui::TextUnformatted("Speed");
        ImGui::SameLine();
        if (ImGui::RadioButton("1\u00d7", speed == Speed::RealTime)) {
            speed = Speed::RealTime;
            runner.setSpeed(speed);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Max", speed == Speed::Max)) {
            speed = Speed::Max;
            runner.setSpeed(speed);
        }

        const std::string_view lock =
            !tv ? "no video" : (tv->pictures().front().locked ? "locked" : "no lock");
        const std::string line = std::format(
            "t {:.3f} s  {:.2f}\u00d7  {}  {}  ring wait {:.0f}%  ui {:.1f} ms",
            static_cast<double>(status.simTime) / static_cast<double>(kPicosPerSecond), speedRatio,
            lock, StateName(status.state), status.ringWaitShare * 100.0, uiMilliseconds);
        ImGui::TextWrapped("%s", line.c_str());
        ImGui::EndChild();
        ImGui::End();

        ImGui::Render();
        const ImGuiIO& io = ImGui::GetIO();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        const auto now = std::chrono::steady_clock::now();
        uiMilliseconds = std::chrono::duration<double, std::milli>(now - frameStart).count();
        if (options.screenshot && now - start >= options.after) {
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
        std::println(stderr,
                     "usage: bw_ui <file.bwn|file.bwp> [--screenshot OUT.png [--after 1s]]");
        return kExitUsage;
    }
    Loaded loaded = Load(options->file);
    if (!loaded.circuit) {
        return kExitErrors;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        return kExitErrors;
    }
    const std::string title = "Bodgewire \u2014 " + loaded.name;
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

    const int exitCode = RunWindow(window, renderer, loaded, *options);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
