#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "parts/key_matrix.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr Time kMatrixHold = 40'000'000'000;  // ps: a BW-2001 press held 40 ms
inline constexpr Time kMatrixGap = 40'000'000'000;   // ps: then released 40 ms
inline constexpr std::size_t kMatrixWaiting = 4096;  // typed presses waiting at most

// Presses typed into a KeyMatrix (P5q, moved out of MatrixKeyboard in P5x): a lock-free
// single-producer queue, and the timing that holds each press `hold`, then releases it `gap`,
// in simulation time, so every press is seen however slow the Machine runs. The owner calls
// step() from its evaluate and reads current() for its keys.
class PressQueue {
public:
    PressQueue(Time hold, Time gap);

    // One producer thread. False (nothing queued) when kMatrixWaiting presses wait.
    bool push(MatrixPress press);
    // Producer side, any time: the presses waiting now are skipped.
    void dropWaiting();

    // Simulation thread, at the owner's power-on: Idle, nothing showing; waiting presses kept.
    void reset();
    // Simulation thread, at each evaluation: ends a hold or a gap due at `now`, takes the next
    // press when idle. The delay to the queue's own next wake, when a press starts or ends now.
    std::optional<Time> step(Time now);
    // Simulation thread: the press held now.
    std::optional<MatrixPress> current() const;
    // Any thread: the press held now includes `key`, `shift` being the key held with a shifted
    // press. For drawing.
    bool scripted(std::size_t key, std::size_t shift) const;

private:
    enum class Phase : std::uint8_t { Idle, Hold, Gap };
    static constexpr std::uint16_t kShiftBit = 0x8000;
    static constexpr std::uint32_t kNotShowing = 0xFFFF'FFFF;

    Time hold_, gap_;
    Phase phase_ = Phase::Idle;
    Time nextAt_ = 0;  // ps; outside Idle: when the phase's own wake is due
    MatrixPress current_{};
    std::atomic<std::uint32_t> showing_{kNotShowing};  // current_ while held: key | kShiftBit
    std::vector<std::uint16_t> waiting_;               // key | kShiftBit for SHIFT
    std::atomic<std::uint32_t> head_{0}, tail_{0};     // SPSC: push() writes head_, step() tail_
    std::atomic<std::uint32_t> dropBefore_{0};
};

}  // namespace bw
