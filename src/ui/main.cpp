#include <cstdio>
#include <filesystem>
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
    if (!ConnectorNet(*loaded.circuit, "VIDEO", "VIDEO") ||
        !ConnectorNet(*loaded.circuit, "VIDEO", "SYNC_N")) {
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
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::Text("Bodgewire");
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
