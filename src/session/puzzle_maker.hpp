#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "machine/puzzle.hpp"

namespace bw {

// Makes a Procedural Puzzle off its caller's thread (P8d: the Launcher, while its lines loop):
// MakeProceduralPuzzleFor on a thread of its own. Its caller's calls come from one thread.
class PuzzleMaker {
public:
    PuzzleMaker() = default;
    ~PuzzleMaker();  // every making stopped and joined: a Machine's load, or 1 ms of simulation
    PuzzleMaker(const PuzzleMaker&) = delete;
    PuzzleMaker& operator=(const PuzzleMaker&) = delete;

    // A Machine's Vetting Script (puzzles/random.bwr) and the Fault's seed; a making under way is
    // stopped and forgotten.
    void start(std::filesystem::path script, std::uint64_t seed);
    void stop();  // asks and forgets: never waits, nothing to take afterwards
    bool making() const;
    // The Fault once made, or the error, once; nullopt while making, and when nothing was started.
    std::optional<std::expected<FaultDecl, std::string>> take();

private:
    struct Run {
        std::mutex mutex;
        bool done = false;  // its thread is about to end
        std::optional<std::expected<FaultDecl, std::string>> result;
        std::jthread thread;  // last: started once the rest is there
    };

    void retire();  // the current making asked to stop, kept until its thread ends
    void reap();    // the retired makings whose thread has ended, joined

    std::shared_ptr<Run> run_;                   // the making under way, or made and not taken
    std::vector<std::shared_ptr<Run>> retired_;  // stopped, their threads still ending
};

}  // namespace bw
