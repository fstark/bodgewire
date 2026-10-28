#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "runner/runner.hpp"
#include "runner/tv_consumer.hpp"
#include "ui/tv_view.hpp"

using namespace bw;

namespace {

constexpr int kExitErrors = 1;
constexpr int kExitUsage = 2;

struct Loaded {
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
    if (file.extension() == ".bwp") {
        auto machine = LoadPuzzleFile(file);
        if (!machine) {
            Print(machine.error());
            return {};
        }
        auto built = machine->build();
        if (!built) {
            Print(built.error());
            return {};
        }
        return {std::move(*built), std::string(machine->name())};
    }
    auto result = LoadNetlistFile(file);
    if (!result) {
        Print(result.error());
        return {};
    }
    Print((*result)->warnings);
    return {std::move(*result), file.stem().string()};
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

void RunWindow(SDL_Window* window, SDL_Renderer* renderer, LoadedCircuit& loaded, NetId video,
               NetId syncN) {
    TvView view(renderer);
    TvConsumer tv(video, syncN);
    Runner runner(loaded.circuit, {&tv});
    std::uint64_t seed = 1;
    Speed speed = Speed::RealTime;
    runner.powerOn(seed);
    SpeedRatio ratio;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(window))) {
                running = false;
            }
        }
        if (tv.pictures().update()) {
            view.show(tv.pictures().front());
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
        ImGui::SameLine();
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

        const ImVec2 available = ImGui::GetContentRegionAvail();
        view.draw({available.x, available.y - ImGui::GetFrameHeightWithSpacing()});
        const std::string line =
            std::format("t {:.3f} s  {:.2f}\u00d7  {}  {}  ring wait {:.0f}%",
                        static_cast<double>(status.simTime) / static_cast<double>(kPicosPerSecond),
                        speedRatio, tv.pictures().front().locked ? "locked" : "no lock",
                        StateName(status.state), status.ringWaitShare * 100.0);
        ImGui::TextUnformatted(line.c_str());
        ImGui::End();

        ImGui::Render();
        const ImGuiIO& io = ImGui::GetIO();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::println(stderr, "usage: bw_ui <file.bwn|file.bwp>");
        return kExitUsage;
    }
    Loaded loaded = Load(argv[1]);
    if (!loaded.circuit) {
        return kExitErrors;
    }
    const auto video = ConnectorNet(*loaded.circuit, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(*loaded.circuit, "VIDEO", "SYNC_N");
    if (!video || !syncN) {
        std::println(stderr, "bw_ui: netlist has no VIDEO connector");
        return kExitErrors;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "bw_ui: {}", SDL_GetError());
        return kExitErrors;
    }
    const std::string title = "Bodgewire \u2014 " + loaded.name;
    SDL_Window* window = SDL_CreateWindow(title.c_str(), 1024, 768,
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

    RunWindow(window, renderer, *loaded.circuit, *video, *syncN);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
