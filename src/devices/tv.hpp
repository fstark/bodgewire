#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr int kTvColumns = 512;
inline constexpr int kTvRows = 320;

struct TvFrame {
    std::vector<float> cells =  // row-major; 0 black .. 1 white
        std::vector<float>(static_cast<std::size_t>(kTvRows) * kTvColumns);
    float at(int row, int column) const {
        return cells[static_cast<std::size_t>(row) * kTvColumns + static_cast<std::size_t>(column)];
    }
};

struct TvFrameStats {
    Time start = 0;
    bool verticalSynced = false;  // started on an accepted vertical sync
    int lines = 0;                // lines started during the frame
    int syncedLines = 0;          // of which on an accepted horizontal sync
};

// Monitor on a VIDEO connector (TV.md). Time only moves forward; at t = 0 both inputs are Z.
class Tv {
public:
    static constexpr Time kColumnTime = 125'000;           // ps
    static constexpr Time kLineFreeRun = 66'000'000;       // ps
    static constexpr Time kLineAccept = 56'000'000;        // ps
    static constexpr Time kFrameFreeRun = 21'000'000'000;  // ps
    static constexpr Time kFrameAccept = 16'000'000'000;   // ps
    static constexpr Time kVerticalSyncLow = 16'000'000;   // ps

    void setVideo(Time time, LogicValue value);
    void setSyncN(Time time, LogicValue value);
    void advanceTo(Time time);  // runs deflection and paints up to `time`
    const TvFrame& completedFrame() const { return completed_; }  // black before the first
    const TvFrameStats& completedStats() const { return completedStats_; }
    std::uint64_t completedFrames() const { return completedFrames_; }
    bool isLocked() const { return locked_; }

private:
    void paint(Time to);
    void startLine(Time time, bool synced);
    void startFrame(Time time, bool synced);

    LogicValue video_ = LogicValue::Z, syncN_ = LogicValue::Z;
    Time lowSince_ = 0;             // last 1 → 0 of SYNC_N
    bool verticalSyncSeen_ = true;  // current low period already produced a vertical sync
    Time now_ = 0;                  // painted up to here
    Time lineStart_ = 0, frameStart_ = 0;
    int row_ = 0;  // lines since frame start
    TvFrameStats stats_;
    TvFrame drawing_, completed_;
    TvFrameStats completedStats_;
    std::uint64_t completedFrames_ = 0;
    bool locked_ = false;
};

}  // namespace bw
