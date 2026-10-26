#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "sim/component.hpp"
#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

// Pin OUT. Test and benchmark stimulus: initial value, then steps at absolute times ≥ 1 ps.
// Values may be Z (tri-state driver); family selects contention rules.
class Stimulus final : public Component {
public:
    struct Step {
        Time time;
        LogicValue value;
    };
    Stimulus(std::string name, LogicValue initial, std::vector<Step> steps,
             Family family = Family::Ttl);  // steps sorted, distinct times
    static constexpr PinIndex kOut = 0;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    LogicValue initial_;
    std::vector<Step> steps_;
    std::size_t next_ = 0;  // index of the next step
};

}  // namespace bw
