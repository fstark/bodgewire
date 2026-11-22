#include "view/launcher_lanes.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace bw {

void LauncherLanes::setPuzzles(std::vector<std::size_t> counts) {
    puzzles_ = std::move(counts);
    machine_ = puzzles_.empty() ? 0 : std::min(machine_, puzzles_.size() - 1);
    postIt_ = std::min(postIt_, postIts() - 1);
}

std::size_t LauncherLanes::postIts() const { return puzzles_.empty() ? 1 : puzzles_[machine_] + 1; }

void LauncherLanes::left() {
    if (lane_ == Lane::Machines) {
        if (machine_ > 0) {
            --machine_;
            postIt_ = 0;
        }
    } else if (postIt_ > 0) {
        --postIt_;
    }
}

void LauncherLanes::right() {
    if (lane_ == Lane::Machines) {
        if (machine_ + 1 < puzzles_.size()) {
            ++machine_;
            postIt_ = 0;
        }
    } else if (postIt_ + 1 < postIts()) {
        ++postIt_;
    }
}

void LauncherLanes::down() { lane_ = Lane::PostIts; }

void LauncherLanes::up() {
    lane_ = Lane::Machines;
    postIt_ = 0;
}

void LauncherLanes::selectMachine(std::size_t machine) {
    machine_ = puzzles_.empty() ? 0 : std::min(machine, puzzles_.size() - 1);
    lane_ = Lane::Machines;
    postIt_ = 0;
}

void LauncherLanes::selectPostIt(std::size_t postIt) {
    lane_ = Lane::PostIts;
    postIt_ = std::min(postIt, postIts() - 1);
}

std::optional<std::size_t> LauncherLanes::puzzle() const {
    return postIt_ == 0 ? std::nullopt : std::optional(postIt_ - 1);
}

void Glide::jump(double to) { value = target = to; }

void Glide::update(double seconds) {
    value += (target - value) * (1 - std::exp(-std::max(0.0, seconds) / kGlideTau));
    if (std::abs(target - value) <= snap) {
        value = target;
    }
}

double PictureLeft(double d, double w) {
    const double atMinus = kHintLeft - w;                       // d = -1
    const double atZero = (kPictureFrom + kPictureTo - w) / 2;  // d = 0
    const double atPlus = kHintRight;                           // d = 1
    return d < 0 ? atZero + d * (atZero - atMinus) : atZero + d * (atPlus - atZero);
}

}  // namespace bw
