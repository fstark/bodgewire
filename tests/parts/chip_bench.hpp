#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

namespace bw {

// One chip in a Circuit (P5n): every Input Pin on its own Net driven by a Stimulus (the listed
// levels and steps, else `idle`), every other signal Pin on its own Net. `level(sim, "Q")`,
// `word(sim, {"QA", ...})` read them back.
struct ChipInput {
    LogicValue initial;
    std::vector<Stimulus::Step> steps = {};
};

struct ChipBench {
    Circuit circuit;
    std::map<std::string, NetId, std::less<>> nets;
    Component* chip = nullptr;

    // Builds the bench once: `ChipBench b; b.build<Ls154>({{"A", {One}}}, Zero);`.
    template <typename Chip, typename... Args>
    Chip& build(std::map<std::string, ChipInput> inputs, LogicValue idle, Args&&... args) {
        ChipBench& b = *this;
        auto& made = b.circuit.add<Chip>("U1", std::forward<Args>(args)...);
        b.chip = &made;
        for (PinIndex i = 0; i < made.pins().size(); ++i) {
            const PinSpec& pin = made.pins()[i];
            if (pin.kind == PinKind::Power || pin.kind == PinKind::NoConnect) {
                continue;
            }
            const NetId net = b.circuit.addNet(pin.name);
            b.nets[pin.name] = net;
            b.circuit.connect(made, i, net);
            if (pin.kind == PinKind::Input) {
                ChipInput input{idle};
                if (const auto it = inputs.find(pin.name); it != inputs.end()) {
                    input = it->second;
                }
                auto& source =
                    b.circuit.add<Stimulus>("S" + pin.name, input.initial, std::move(input.steps));
                b.circuit.connect(source, Stimulus::kOut, net);
            }
        }
        b.circuit.finalize();
        return made;
    }

    LogicValue level(const Simulator& sim, std::string_view pin) const {
        return sim.value(nets.find(pin)->second);
    }
    // The Pins as a number, the first the lowest bit; -1 if any is not 0 or 1.
    int word(const Simulator& sim, std::initializer_list<std::string_view> pins) const {
        int value = 0, bit = 0;
        for (const std::string_view pin : pins) {
            const LogicValue v = level(sim, pin);
            if (v != LogicValue::Zero && v != LogicValue::One) {
                return -1;
            }
            value |= (v == LogicValue::One ? 1 : 0) << bit++;
        }
        return value;
    }
};

}  // namespace bw
