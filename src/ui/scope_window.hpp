#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "instruments/oscilloscope.hpp"
#include "session/session.hpp"

namespace bw {

class BoardView;

// The floating Oscilloscope: settings, the screen with the last Sweep, cursor readout.
class ScopeWindow {
public:
    void draw(bool& open, Session& session, BoardView& board);

private:
    std::optional<Sweep> shown_;  // kept across rebuilds and power-off
    std::optional<Time> marker_;  // ps since the Trigger
    std::array<char, 16> delayText_{};
    bool delayEditing_ = false, delayRefused_ = false;
    // Column cache, per Channel: lo/hi volts per pixel; rebuilt when serial, width or rail change.
    std::array<std::vector<std::pair<float, float>>, kScopeChannels> columns_;
    std::uint64_t columnsSerial_ = 0;
    float columnsWidth_ = 0;
    std::array<std::optional<double>, kScopeChannels> columnsRail_;
};

}  // namespace bw
