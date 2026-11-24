#pragma once

#include <cstdint>
#include <optional>

namespace bw {

// When the bench acknowledges a completed Puzzle (P8c): the post-it's tick, and the moment the
// Thank-you Card opens, once per bench. Counted in wall-clock time from a power-on, so the player
// sees the Machine run before being told.
class RepairAck {
public:
    static constexpr double kRunSeconds = 3;

    explicit RepairAck(bool puzzle) : puzzle_(puzzle) {}  // Session::puzzle(): false never ticks
    // Each frame: Session::completed(), powered() and powerOns(), and the wall clock in s. A new
    // power-on count starts a run (a power cycle may show no unpowered frame); a verdict "not
    // completed" takes the tick off at once.
    void update(bool completed, bool powered, std::uint64_t powerOns, double now);
    bool ticked() const { return ticked_; }
    // True once per bench: at the first call after the bench's first tick.
    bool takeCard();

private:
    bool puzzle_;
    std::uint64_t powerOns_ = 0;
    std::optional<double> since_;  // the current run's power-on; nullopt while off
    bool ticked_ = false;
    bool cardDue_ = false;
    bool cardGiven_ = false;
};

}  // namespace bw
