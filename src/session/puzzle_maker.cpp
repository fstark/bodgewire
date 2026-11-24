#include "session/puzzle_maker.hpp"

#include <exception>
#include <stop_token>
#include <utility>

#include "session/procedural_puzzle.hpp"

namespace bw {

PuzzleMaker::~PuzzleMaker() {
    retire();
    retired_.clear();  // each jthread's destructor asks again and joins
}

void PuzzleMaker::start(std::filesystem::path script, std::uint64_t seed) {
    retire();
    reap();
    auto run = std::make_shared<Run>();
    run_ = run;
    // The thread holds the run by a plain pointer: the run outlives it (its jthread is joined
    // before the run goes).
    run->thread =
        std::jthread([made = run.get(), script = std::move(script), seed](std::stop_token stop) {
            std::optional<std::expected<FaultDecl, std::string>> result;
            // A Check thrown by a load nobody expected: the making's error, not the program's end.
            // One making on its own thread: never around a frame loop (P8b).
            try {
                auto puzzle = MakeProceduralPuzzleFor(script, seed, std::nullopt, stop);
                if (!puzzle) {
                    result = std::unexpected(std::move(puzzle.error()));
                } else if (*puzzle) {
                    result = (*puzzle)->fault;
                }  // else stopped: nothing
            } catch (const std::exception& error) {
                result = std::unexpected(std::string(error.what()));
            }
            const std::scoped_lock lock(made->mutex);
            made->result = std::move(result);
            made->done = true;
        });
}

void PuzzleMaker::stop() {
    retire();
    reap();
}

bool PuzzleMaker::making() const {
    if (!run_) {
        return false;
    }
    const std::scoped_lock lock(run_->mutex);
    return !run_->done;
}

std::optional<std::expected<FaultDecl, std::string>> PuzzleMaker::take() {
    reap();
    if (!run_) {
        return std::nullopt;
    }
    std::optional<std::expected<FaultDecl, std::string>> result;
    {
        const std::scoped_lock lock(run_->mutex);
        if (!run_->done) {
            return std::nullopt;
        }
        result = std::move(run_->result);
    }
    run_.reset();  // its thread has set done: the join is immediate
    return result;
}

void PuzzleMaker::retire() {
    if (run_) {
        run_->thread.request_stop();
        retired_.push_back(std::move(run_));
        run_.reset();
    }
}

void PuzzleMaker::reap() {
    std::erase_if(retired_, [](const std::shared_ptr<Run>& run) {
        const std::scoped_lock lock(run->mutex);
        return run->done;
    });
}

}  // namespace bw
