#include "view/repair_ack.hpp"

namespace bw {

void RepairAck::update(bool completed, bool powered, std::uint64_t powerOns, double now) {
    if (!puzzle_) {
        return;
    }
    if (powerOns != powerOns_) {
        powerOns_ = powerOns;
        since_ = now;
    }
    if (!powered) {
        since_.reset();
    }
    if (!completed) {
        ticked_ = false;
        return;
    }
    if (!ticked_ && since_ && now - *since_ >= kRunSeconds) {
        ticked_ = true;
        if (!cardGiven_) {
            cardGiven_ = true;
            cardDue_ = true;
        }
    }
}

bool RepairAck::takeCard() {
    const bool due = cardDue_;
    cardDue_ = false;
    return due;
}

}  // namespace bw
