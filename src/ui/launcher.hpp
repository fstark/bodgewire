#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
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
#include "machine/progress.hpp"
#include "runner/runner.hpp"
#include "session/puzzle_maker.hpp"
#include "ui/board_view.hpp"
#include "ui/loaded.hpp"
#include "ui/question_popup.hpp"
#include "ui/tv_view.hpp"
#include "view/board_scene.hpp"
#include "view/launcher_lanes.hpp"
#include "view/pace.hpp"

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

// What the launcher opens: an entry's file (a .bwn, a .bwp), or a Machine's Vetting Script (a
// .bwr) with the Random fault made for it (P8d).
struct LauncherChoice {
    std::filesystem::path file;
    std::optional<FaultDecl> fault = {};
};

// The launcher (P7a, its look P7b): two lanes. The top one shows the selected Machine live (its
// Board framed on its outputs, or its monitor with the Board's corner), its name and its User
// Manual's first paragraph and, lighter, how fast it runs on this computer (its headroom,
// 2026-11-24, which also colours its bar), the previous and next Machines' stills at the edges. The
// bottom one, the shelf's wood, shows its post-its: "Start the Machine", then its Puzzles, then,
// a gap apart, "Random fault" with its "?" (P8d). Arrow keys move in and between the lanes, as do
// clicks on the chevrons; Enter, Space or a click on a post-it opens it. "Random fault" first
// makes its Puzzle, on a thread of its own: meanwhile lines about breaking computers loop over
// the wood, and only Esc does anything (it stops the making).
class Launcher {
public:
    Launcher() = default;
    ~Launcher();
    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    void setList(MachineList list);  // read anew each time it shows; the selection clamped
    // An entry (a netlist, a Puzzle or a Vetting Script of the list): its Machine and post-it;
    // else kept.
    void select(const std::filesystem::path& file);
    // The selected "Random fault" starts making its Puzzle (the card's "Another random fault",
    // --make); `seed`: the Fault's, else a new one each time. Nothing on any other post-it.
    void make(std::optional<std::uint64_t> seed = std::nullopt);
    // --screenshot: no bench is ever opened; a making's lines stay up once its Fault is made.
    void holdChoices() { hold_ = true; }
    void setError(std::string text);  // "Could not load …: see the terminal", in red
    // Saved progress (P8e): the repaired Puzzles' post-its ticked, and "Delete: forget the
    // ticks" while one listed is; `forget` called at the question's OK (it empties and saves).
    // nullptr: no ticks, no hint.
    void setProgress(const Progress* progress, std::function<void()> forget);
    void askForget() { askForget_ = true; }  // --forget: the question open at the next draw
    const MachineList& list() const { return list_; }
    // Each frame, inside a window filling the main one with no padding: keys, clicks, the loads,
    // the live run, both lanes, a making. The entry to open.
    std::optional<LauncherChoice> draw(TvView& tv, ImFont* legends, ImFont* titleFont);
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
        std::future<OpenedMachine> loading;
        std::optional<OpenedMachine> loaded;  // powered on
        bool failed = false;
        std::optional<BoardView> view;  // on its Session's Board, when it has one
    };

    std::filesystem::path entryFile() const;
    // The entry that runs above: the selected one's file; the working Machine for "Random fault"
    // (no Fault exists until it is opened).
    std::filesystem::path liveFile() const;
    std::optional<LauncherChoice> updateMaking();  // the choice once the Fault is made
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
    Headroom headroom_;  // the live run's: the Machine's bar colour, its sentence
    std::vector<std::future<OpenedMachine>> abandoned_;  // loads of entries no longer selected
    PuzzleMaker maker_;                                  // a Random fault's making (P8d)
    bool making_ = false;     // from make() to the choice returned, Esc, Stop or an error
    bool stopAsked_ = false;  // the plate's Stop button (P8f), as Esc
    std::filesystem::path makingFile_;
    std::chrono::steady_clock::time_point makingSince_;
    std::size_t firstLine_ = 0;      // of the waiting lines: a new start each making
    std::optional<FaultDecl> made_;  // made, waiting for its line to be read
    bool hold_ = false;
    const Progress* progress_ = nullptr;
    std::function<void()> forget_;
    QuestionPopup forgetQuestion_{"##forget", "Forget every repaired Puzzle?", false};
    bool askForget_ = false;
    StillLoader loader_;  // last: its thread joined first
};

}  // namespace bw
