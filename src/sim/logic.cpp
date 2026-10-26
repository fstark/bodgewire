#include "sim/logic.hpp"

namespace bw {

Resolution Resolver::finish(bool hasTtlInput, LogicValue lastDriven) const {
    if (max_ == DriveStrength::None) {
        return {hasTtlInput ? LogicValue::One : lastDriven, true};
    }
    if (sawX_) {
        return {LogicValue::X, false};
    }
    if (conflict_) {
        const bool ttlContention = max_ == DriveStrength::Strong && allTtl_;
        return {ttlContention ? LogicValue::Zero : LogicValue::X, false};
    }
    return {value_, false};
}

}  // namespace bw
