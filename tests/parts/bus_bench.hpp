#pragma once

// A bus peripheral (6520, 6522) driven through a script of 1 MHz bus cycles (P5n; the 6530's test
// bench made general). Cycle i: PHI2 low for its first half, high for its second (rise at
// i µs + 500 ns, fall at (i + 1) µs). Address, RW, selects and RES_N change 100 ns into the cycle;
// a write's data is on the bus from 150 ns after the rise to 30 ns after the fall. Every other
// signal Pin is on a Net of its own, with an optional outside Stimulus (MOS, Strong).

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

namespace bw {

struct BusCycle {
    unsigned reg = 0;  // on the register select Pins, RS0 the lowest bit
    bool read = true;
    std::uint8_t data = 0;  // written
    bool selected = true;
    bool reset = false;  // RES_N low
};

inline BusCycle BusRead(unsigned reg) { return {reg, true}; }
inline BusCycle BusWrite(unsigned reg, std::uint8_t data) { return {reg, false, data}; }
inline BusCycle BusIdle() { return {0, true, 0, false}; }

inline Time BusRise(std::size_t cycle) { return static_cast<Time>(cycle) * 1'000'000 + 500'000; }
inline Time BusFall(std::size_t cycle) { return static_cast<Time>(cycle + 1) * 1'000'000; }

using BusSteps = std::vector<Stimulus::Step>;
using BusOutside = std::map<std::string, std::pair<LogicValue, BusSteps>>;

struct BusPins {
    PinIndex phi2, rw, resN;
    std::array<PinIndex, 8> data;                    // D0..D7
    std::vector<PinIndex> registerSelect;            // RS0 first
    std::vector<std::pair<PinIndex, bool>> selects;  // each select Pin and its active level
};

struct BusBench {
    Circuit circuit;
    Component* chip = nullptr;
    std::array<NetId, 8> data{};
    std::map<std::string, NetId, std::less<>> nets;

    template <typename Chip>
    Chip& build(const BusPins& pins, const std::vector<BusCycle>& script,
                const BusOutside& outside = {}) {
        using enum LogicValue;
        auto& made = circuit.add<Chip>("U1");
        chip = &made;
        const std::size_t n = script.size();
        std::vector<bool> done(made.pins().size(), false);
        const auto input = [&](PinIndex pin, LogicValue initial, BusSteps steps) {
            const std::string name(made.pins()[pin].name);
            const NetId net = circuit.addNet(name);
            nets[name] = net;
            auto& source = circuit.add<Stimulus>("S" + name, initial, std::move(steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(made, pin, net);
            done[pin] = true;
        };
        const auto control = [&](PinIndex pin, auto value) {
            const auto level = [&](std::size_t i) { return value(script[i]) ? One : Zero; };
            const LogicValue initial = level(0);
            LogicValue last = initial;
            BusSteps steps;
            for (std::size_t i = 1; i < n; ++i) {
                if (level(i) != last) {
                    last = level(i);
                    steps.push_back({static_cast<Time>(i) * 1'000'000 + 100'000, last});
                }
            }
            input(pin, initial, std::move(steps));
        };
        BusSteps phi2;
        for (std::size_t i = 0; i < n; ++i) {
            phi2.push_back({BusRise(i), One});
            phi2.push_back({BusFall(i), Zero});
        }
        input(pins.phi2, Zero, phi2);
        control(pins.rw, [](const BusCycle& c) { return c.read; });
        control(pins.resN, [](const BusCycle& c) { return !c.reset; });
        for (std::size_t bit = 0; bit < pins.registerSelect.size(); ++bit) {
            control(pins.registerSelect[bit],
                    [bit](const BusCycle& c) { return ((c.reg >> bit) & 1) != 0; });
        }
        for (const auto& [pin, active] : pins.selects) {
            control(pin, [active](const BusCycle& c) { return c.selected == active; });
        }
        for (unsigned bit = 0; bit < 8; ++bit) {
            BusSteps steps;
            for (std::size_t i = 0; i < n; ++i) {
                if (!script[i].read && script[i].selected) {
                    steps.push_back(
                        {BusRise(i) + 150'000, ((script[i].data >> bit) & 1) ? One : Zero});
                    steps.push_back({BusFall(i) + 30'000, Z});
                }
            }
            const PinIndex pin = pins.data[bit];
            const std::string name(made.pins()[pin].name);
            data[bit] = circuit.addNet(name);
            nets[name] = data[bit];
            auto& source = circuit.add<Stimulus>("S" + name, Z, std::move(steps), Family::Mos);
            circuit.connect(source, Stimulus::kOut, data[bit]);
            circuit.connect(made, pin, data[bit]);
            done[pin] = true;
        }
        for (PinIndex pin = 0; pin < made.pins().size(); ++pin) {
            const PinSpec& spec = made.pins()[pin];
            if (done[pin] || spec.kind == PinKind::Power || spec.kind == PinKind::NoConnect) {
                continue;
            }
            const NetId net = circuit.addNet(spec.name);
            nets[spec.name] = net;
            circuit.connect(made, pin, net);
            if (const auto it = outside.find(spec.name); it != outside.end()) {
                auto& source = circuit.add<Stimulus>("S" + spec.name, it->second.first,
                                                     it->second.second, Family::Mos);
                circuit.connect(source, Stimulus::kOut, net);
            }
        }
        circuit.finalize();
        return made;
    }

    // The data bus: its byte, or nullopt if any line floats or is X.
    std::optional<unsigned> bus(const Simulator& sim) const {
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const LogicValue v = sim.value(data[bit]);
            if (sim.isFloating(data[bit]) || (v != LogicValue::Zero && v != LogicValue::One)) {
                return std::nullopt;
            }
            value |= (v == LogicValue::One ? 1u : 0u) << bit;
        }
        return value;
    }

    // A Pin's Net: its value, Z when floating.
    LogicValue level(const Simulator& sim, std::string_view name) const {
        const NetId net = nets.find(name)->second;
        return sim.isFloating(net) ? LogicValue::Z : sim.value(net);
    }
};

// Runs the script; the bus just before each selected read cycle's PHI2 fall (nullopt: floating).
inline std::vector<std::optional<unsigned>> RunReads(BusBench& bench, Simulator& sim,
                                                     const std::vector<BusCycle>& script) {
    std::vector<std::optional<unsigned>> reads;
    for (std::size_t i = 0; i < script.size(); ++i) {
        if (BusFall(i) - 1 > sim.now()) {
            sim.runUntil(BusFall(i) - 1);
        }
        if (script[i].read && script[i].selected) {
            reads.push_back(bench.bus(sim));
        }
    }
    return reads;
}

}  // namespace bw
