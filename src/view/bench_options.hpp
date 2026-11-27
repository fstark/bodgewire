#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "board/board.hpp"
#include "machine/knob_setting.hpp"
#include "machine/press.hpp"
#include "machine/repair.hpp"
#include "machine/typing.hpp"
#include "sim/time.hpp"
#include "view/bench_places.hpp"
#include "view/binder.hpp"
#include "view/fingers.hpp"
#include "view/focus.hpp"
#include "view/memory_contents.hpp"
#include "view/pencil_marks.hpp"
#include "view/run_switch.hpp"
#include "view/time_view.hpp"
#include "view/tv_controls.hpp"
#include "view/viewport.hpp"

namespace bw {

// How a bench starts (bw_ui's flags past the file): the state a player would reach by hand, set
// up for tests and screenshots. Applied by the bench (ui/bench_setup.hpp).
struct BenchOptions {
    std::optional<std::string> probe;  // REF.PIN
    std::vector<std::string> repair;
    std::vector<std::string> clip;   // REF.PIN[:T]
    std::vector<std::string> group;  // REF.PIN[:PATTERN]
    std::optional<Time> sample;      // Analyzer sample period, ps
    std::optional<TimeView> view;    // Analyzer view at its first Capture
    std::optional<Time> marker;      // ps from the Trigger, both instrument windows
    std::optional<Time> cursor;      // a fake hover, ps from the Trigger
    // REF.PIN: red Test Lead, V mode; two: red and black, Continuity, the power off.
    std::vector<std::string> meter;
    std::vector<std::string> hook;  // REF.PIN, at most kScopeChannels
    std::optional<Time> timebase;   // Oscilloscope ps per division
    std::optional<Time> delay;      // Oscilloscope Trigger delay, ps
    std::vector<ButtonPress> press;
    std::vector<KnobSetting> knob;
    std::vector<Typing> type;
    std::optional<Divider> manual;        // --manual: the binder out at this divider (P6e)
    std::optional<TvControls> tv;         // --tv: the monitor's controls at start (P6f)
    std::optional<ScreenFraction> loupe;  // --loupe: the TV's loupe there (screenshots)
    std::optional<Bounds> zoom;           // --zoom: the Board framed on this box at start
    bool plainLook = false;      // --look plain: the specialist's Board, no parts drawn (P6)
    std::optional<Owner> focus;  // --focus: the owner at start (P6c)
    bool debug = false;          // --debug: the Debug window out (P6d)
    std::vector<std::pair<MarkShape, Point>> mark;  // --mark: pencil marks at start (P6h)
    std::vector<BenchObject> pop;   // --pop: out in their own desktop windows at start (P6j)
    std::vector<BenchObject> away;  // --away: the Machine slot's objects put away at start (P6o)
    RunSwitch run = RunSwitch::RealTime;  // --run: the run switch at start (P6o)
    bool back = false;                    // --side back: the Board flipped at start (P6o)
    std::optional<RepairAction> ask;      // --ask: the power-cycle popup open at start (P6o)
    bool leave = false;      // --leave: the question going back to the launcher open at start (P7a)
    bool reset = false;      // --reset: the Reset's question open at start
    float binderHeight = 1;  // --binder-height: the binder's stretch (P6o)
    std::optional<std::pair<float, float>> brief;  // --brief: the note's offset on the Board, px
    // --contents: the binder out at a memory chip's Contents, at a view (P6p).
    std::optional<std::pair<std::string, std::optional<ContentsView>>> contents;
    bool showFault = false;  // --show-fault: the binder's Give up page, its Fault shown (P8d)
};

// bw_ui's command line.
struct Options {
    // None: the launcher (P7a); a bench flag needs one. A Machine's Vetting Script
    // (puzzles/random.bwr): a Random fault's bench, the Fault made at start (P8d).
    std::optional<std::filesystem::path> file;
    std::optional<std::filesystem::path> machines;     // --machines DIR: the launcher's (P7a)
    std::vector<std::filesystem::path> visit;          // --visit: opened in turn from the launcher
    std::optional<std::filesystem::path> select;       // --select: the launcher's entry at start
    std::optional<std::chrono::nanoseconds> visitFor;  // a visited bench: Back after this long
    std::optional<std::filesystem::path> screenshot;
    std::chrono::nanoseconds after{std::chrono::seconds(1)};
    std::optional<std::filesystem::path> roms;  // --roms DIR: ROM files named by reference
    // --seed N: the Random fault made for a .bwr given as the file or visited (1 without it),
    // or by --make (a new one each time without it).
    std::optional<std::uint64_t> seed;
    // --make, with --select of a .bwr: the Launcher starts making its Random fault at once.
    bool make = false;
    // --progress FILE: the player's saved progress kept there (P8e), not in SDL's pref path; a
    // --screenshot run without it reads and writes none.
    std::optional<std::filesystem::path> progress;
    // --text-size 16|20|24: the pages' reading size for this run (the binder's aA key's), not the
    // one saved beside the Progress; a --screenshot run without it reads and writes none (16).
    std::optional<int> textSize;
    // --forget, the launcher only: its question "Forget every repaired Puzzle?" open at start.
    bool forget = false;
    // --touch: the mouse stands in for one finger (P8f), on the Launcher and every bench.
    bool touch = false;
    // --tap, --double-tap, --hold, --drag, --pinch: scripted fingers on the first screen, in the
    // flags' order (window px; ScriptEvents gives their times); each also sets `touch`.
    std::vector<FingerScript> fingers;
    // --window W:H: the main window's size (a tablet's: 1180:820), not 1280 × 800.
    std::optional<std::pair<int, int>> window;
    // --one-window: no desktop windows, as the web build: a double-click on an object fills the
    // bench with it.
    bool oneWindow = false;
    BenchOptions bench;  // the file's bench; a visited one starts plain
};

// "usage: bw_ui [file.bwn|file.bwp|random.bwr] [--machines DIR] ...", from the flags' table.
std::string Usage();

// `args` without the program's name. The error: a message to print before the usage, or "".
std::expected<Options, std::string> ParseOptions(std::span<const std::string_view> args);

}  // namespace bw
