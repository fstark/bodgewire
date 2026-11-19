#include "view/power_cycle.hpp"

namespace bw {

std::optional<RepairStep> StepFor(bool wouldRepair, bool powered, bool dontAsk) {
    if (!wouldRepair) {
        return std::nullopt;
    }
    if (!powered) {
        return RepairStep::Apply;
    }
    return dontAsk ? RepairStep::Cycle : RepairStep::Ask;
}

}  // namespace bw
