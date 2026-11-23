#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "sim/ids.hpp"
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

// How a Tv takes its sync: one composite SYNC_N (the VIDEO connector), or separate horizontal and
// vertical drives (the MONITOR connector, the PET 2001's built-in monitor, P5q).
enum class TvSync : std::uint8_t { Composite, Separate };

// The Nets of a MONITOR connector: the dot lit while `videoN` is low, a line at each rise of
// `hdrive`, a frame at each fall of `vdrive`.
struct SeparateDrives {
    NetId videoN, hdrive, vdrive;
};

// The Nets of a VIDEO connector: the dot lit while `video` is high, composite sync on `syncN`.
struct CompositeVideo {
    NetId video, syncN;
};

// What a Tv plugs into (FindTvNets, session/session.hpp).
using TvNets = std::variant<CompositeVideo, SeparateDrives>;

inline TvSync SyncOf(const TvNets& nets) {
    return std::holds_alternative<SeparateDrives>(nets) ? TvSync::Separate : TvSync::Composite;
}

// VIDEO, SYNC_N; or VIDEO_N, HDRIVE, VDRIVE.
inline std::vector<NetId> NetsOf(const TvNets& nets) {
    if (const auto* drives = std::get_if<SeparateDrives>(&nets)) {
        return {drives->videoN, drives->hdrive, drives->vdrive};
    }
    const auto& composite = std::get<CompositeVideo>(nets);
    return {composite.video, composite.syncN};
}

// Monitor on a VIDEO or MONITOR connector (TV.md). Time only moves forward; at t = 0 every input
// is Z.
class Tv {
public:
    explicit Tv(TvSync sync = TvSync::Composite) : sync_(sync) {}

    static constexpr Time kColumnTime = 125'000;           // ps
    static constexpr Time kLineFreeRun = 66'000'000;       // ps
    static constexpr Time kLineAccept = 56'000'000;        // ps
    static constexpr Time kFrameFreeRun = 21'000'000'000;  // ps
    static constexpr Time kFrameAccept = 16'000'000'000;   // ps
    static constexpr Time kVerticalSyncLow = 16'000'000;   // ps

    void setVideo(Time time, LogicValue value);
    void setSyncN(Time time, LogicValue value);            // Composite only
    void setHorizontalDrive(Time time, LogicValue value);  // Separate: a line at an accepted rise
    void setVerticalDrive(Time time, LogicValue value);    // Separate: a frame at an accepted fall
    TvSync sync() const { return sync_; }
    void advanceTo(Time time);  // runs deflection and paints up to `time`
    const TvFrame& completedFrame() const { return completed_; }  // black before the first
    const TvFrameStats& completedStats() const { return completedStats_; }
    std::uint64_t completedFrames() const { return completedFrames_; }
    bool isLocked() const { return locked_; }

private:
    void paint(Time to);
    void startLine(Time time, bool synced);
    void startFrame(Time time, bool synced);

    TvSync sync_;
    LogicValue video_ = LogicValue::Z, syncN_ = LogicValue::Z;
    LogicValue hdrive_ = LogicValue::Z, vdrive_ = LogicValue::Z;
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
