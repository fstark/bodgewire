#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <imgui.h>

#include "machine/machine_list.hpp"
#include "runner/runner.hpp"
#include "ui/board_view.hpp"
#include "ui/loaded.hpp"
#include "ui/tv_view.hpp"
#include "view/board_scene.hpp"
#include "view/launcher_lanes.hpp"

namespace bw {

// A Machine's Board drawn unpowered in the launcher (P7b), made on the loader's thread: its scene,
// its tags (every lamp dark, every switch released), whether it has a TV, its output frame.
struct StillData {
    BoardScene scene;
    std::vector<LedTag> leds;
    std::vector<SwitchTag> switches;
    bool tv = false;
    Bounds frame{};
};

// Loads every Machine's still on a thread of its own, in the order given (P7b): loading a Machine
// with its Board takes up to a quarter of a second (the BW-2001), never on the UI thread.
class StillLoader {
public:
    void start(std::vector<std::filesystem::path> netlists);  // once; later calls ignored
    // The stills finished since the last call: a null one for a netlist without a Board or that
    // failed to load (its diagnostics on stderr).
    std::vector<std::pair<std::filesystem::path, std::unique_ptr<StillData>>> take();

private:
    std::mutex mutex_;
    std::vector<std::pair<std::filesystem::path, std::unique_ptr<StillData>>> done_;
    bool started_ = false;
    std::jthread thread_;  // last: stopped and joined first
};

// The launcher (P7a, its look P7b): two lanes. The top one shows the selected Machine live (its
// Board framed on its outputs, or its monitor with the Board's corner), its name and its User
// Manual's first paragraph, the previous and next Machines' stills at the edges. The bottom one,
// the shelf's wood, shows its post-its: "Start the Machine", then its Puzzles. Arrow keys move in
// and between the lanes; Enter, Space or a click on a post-it opens it.
class Launcher {
public:
    Launcher() = default;
    ~Launcher();
    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    void setList(MachineList list);  // read anew each time it shows; the selection clamped
    // An entry (a netlist or a Puzzle of the list): its Machine and post-it; else kept.
    void select(const std::filesystem::path& file);
    void setError(std::string text);  // "Could not load …: see the terminal", in red
    const MachineList& list() const { return list_; }
    // Each frame, inside a window filling the main one with no padding: keys, clicks, the loads,
    // the live run, both lanes. The entry to open (a .bwn or a .bwp).
    std::optional<std::filesystem::path> draw(TvView& tv, ImFont* legends);
    // The live run stopped and every load waited for: before a bench loads, and on quitting.
    void stop();
    std::optional<RunnerStatus> liveStatus();

private:
    struct Still {
        bool tv = false;
        Bounds frame{};
        BoardView view;
    };
    struct Live {
        std::filesystem::path file;  // the entry selected
        std::chrono::steady_clock::time_point since;
        std::future<Loaded> loading;
        std::optional<Loaded> loaded;  // powered on
        bool failed = false;
        std::optional<BoardView> view;  // on its Session's Board, when it has one
    };

    std::filesystem::path entryFile() const;
    void updateLive(TvView& tv);
    void takeStills();
    Still* stillOf(std::size_t machine);
    bool liveOn(std::size_t machine) const;

    MachineList list_;
    LauncherLanes lanes_;
    Glide machines_;
    Glide postIts_{0, 0, 0.5};
    std::size_t rowMachine_ = 0;     // whose post-its the row's scroll is for
    std::vector<float> noteScroll_;  // each post-it's text scrolled, px (a long brief)
    bool jump_ = true;  // the glides jump at the next draw (shown, selected from outside)
    std::string error_;
    std::map<std::filesystem::path, Still> stills_;
    Live live_;
    std::vector<std::future<Loaded>> abandoned_;  // loads of entries no longer selected
    StillLoader loader_;                          // last: its thread joined first
};

}  // namespace bw
