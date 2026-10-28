#include "devices/tv.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace bw {

void Tv::setVideo(Time time, LogicValue value) {
    advanceTo(time);
    video_ = value;
}

void Tv::setSyncN(Time time, LogicValue value) {
    advanceTo(time);
    if (syncN_ == LogicValue::One && value == LogicValue::Zero) {
        lowSince_ = time;
        verticalSyncSeen_ = false;
        if (time - lineStart_ >= kLineAccept) {
            startLine(time, true);
        }
    }
    syncN_ = value;
}

// Internal events strictly before `time`: an input change at `time` comes first (TV_DESIGN §5.1).
void Tv::advanceTo(Time time) {
    assert(time >= now_);
    while (true) {
        const Time lineEnd = lineStart_ + kLineFreeRun;
        const Time frameEnd = frameStart_ + kFrameFreeRun;
        const Time verticalSyncAt = syncN_ == LogicValue::Zero && !verticalSyncSeen_
                                        ? lowSince_ + kVerticalSyncLow
                                        : std::numeric_limits<Time>::max();
        const Time next = std::min({lineEnd, frameEnd, verticalSyncAt});
        if (next >= time) {
            break;
        }
        paint(next);
        if (next == verticalSyncAt) {
            verticalSyncSeen_ = true;
            if (next - frameStart_ >= kFrameAccept) {
                startFrame(next, true);
            }
        } else if (next == lineEnd) {
            startLine(next, false);
        } else {
            startFrame(next, false);
        }
    }
    paint(time);
}

void Tv::paint(Time to) { now_ = to; }

void Tv::startLine(Time time, bool synced) {
    lineStart_ = time;
    ++row_;
    ++stats_.lines;
    stats_.syncedLines += synced ? 1 : 0;
}

void Tv::startFrame(Time time, bool synced) {
    locked_ = synced && stats_.verticalSynced && stats_.syncedLines * 10 >= stats_.lines * 9;
    std::swap(drawing_, completed_);
    std::ranges::fill(drawing_.cells, 0.0f);
    completedStats_ = stats_;
    ++completedFrames_;
    frameStart_ = time;
    row_ = 0;
    stats_ = {time, synced, 0, 0};
}

}  // namespace bw
