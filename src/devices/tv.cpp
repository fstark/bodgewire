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
    assert(sync_ == TvSync::Composite);
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

void Tv::setHorizontalDrive(Time time, LogicValue value) {
    assert(sync_ == TvSync::Separate);
    advanceTo(time);
    if (hdrive_ == LogicValue::Zero && value == LogicValue::One &&
        time - lineStart_ >= kLineAccept) {
        startLine(time, true);
    }
    hdrive_ = value;
}

void Tv::setVerticalDrive(Time time, LogicValue value) {
    assert(sync_ == TvSync::Separate);
    advanceTo(time);
    if (vdrive_ == LogicValue::One && value == LogicValue::Zero &&
        time - frameStart_ >= kFrameAccept) {
        startFrame(time, true);
    }
    vdrive_ = value;
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

// [now_, to) lies inside one line: every line start paints first.
void Tv::paint(Time to) {
    const float weight = video_ == LogicValue::One ? 1.0f : video_ == LogicValue::X ? 0.5f : 0.0f;
    if (weight > 0.0f && row_ < kTvRows) {
        const std::size_t rowStart = static_cast<std::size_t>(row_) * kTvColumns;
        for (Time t = now_; t < to;) {
            const Time column = (t - lineStart_) / kColumnTime;
            if (column >= kTvColumns) {
                break;
            }
            const Time end = std::min(to, lineStart_ + (column + 1) * kColumnTime);
            drawing_.cells[rowStart + static_cast<std::size_t>(column)] +=
                weight * static_cast<float>(end - t) / static_cast<float>(kColumnTime);
            t = end;
        }
    }
    now_ = to;
}

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
