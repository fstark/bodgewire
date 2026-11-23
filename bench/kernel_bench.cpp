// S7 kernel benchmark: 16-bit synchronous counter clocked at 14.318 MHz.
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

constexpr int kBits = 16;
constexpr Time kClockPeriod = 69'842;  // 14.318 MHz
constexpr double kMinAppliedEventsPerSecond = 20e6;

int Usage() {
    std::println(stderr, "usage: bw_kernel_bench [--seconds N]");
    return 2;
}

std::string Mega(double perSecond) { return std::format("{:.1f}e6", perSecond / 1e6); }

}  // namespace

int main(int argc, char** argv) {
    std::int64_t seconds = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg != "--seconds" || i + 1 >= argc) {
            return Usage();
        }
        const std::string_view value = argv[++i];
        const auto [end, error] =
            std::from_chars(value.data(), value.data() + value.size(), seconds);
        if (error != std::errc{} || end != value.data() + value.size() || seconds <= 0) {
            return Usage();
        }
    }

    Circuit circuit;
    const NetId vcc = circuit.addNet("VCC");
    circuit.tie(vcc, LogicValue::One, DriveStrength::Supply);
    const NetId clk = circuit.addNet("CLK");
    auto& clock = circuit.add<ClockSource>("X1", kClockPeriod);
    circuit.connect(clock, ClockSource::kOut, clk);

    std::vector<NetId> q;
    NetId carry = vcc;  // C_0
    for (int i = 0; i < kBits; ++i) {
        const std::string index = std::to_string(i);
        q.push_back(circuit.addNet("Q" + index));
        const NetId next = circuit.addNet("N" + index);

        auto& ff = circuit.add<DFlipFlop>("FF" + index, Delays{5_ns, 5_ns});
        circuit.connect(ff, "CLK", clk);
        circuit.connect(ff, "D", next);
        circuit.connect(ff, "Q", q.back());

        auto& sum = circuit.add<Gate>("XOR" + index, GateKind::Xor, 2, Delays{2_ns, 2_ns});
        circuit.connect(sum, "I0", q.back());
        circuit.connect(sum, "I1", carry);
        circuit.connect(sum, sum.output(), next);

        if (i + 1 < kBits) {
            const NetId nextCarry = circuit.addNet("C" + std::to_string(i + 1));
            auto& gate = circuit.add<Gate>("AND" + index, GateKind::And, 2, Delays{1_ns, 1_ns});
            circuit.connect(gate, "I0", carry);
            circuit.connect(gate, "I1", q.back());
            circuit.connect(gate, gate.output(), nextCarry);
            carry = nextCarry;
        }
    }
    circuit.finalize();

    Simulator sim(circuit);
    const auto start = std::chrono::steady_clock::now();
    sim.powerOn(1);
    sim.runFor(seconds * kPicosPerSecond);
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - start;

    unsigned counter = 0;
    for (int i = 0; i < kBits; ++i) {
        counter |= (sim.value(q[static_cast<std::size_t>(i)]) == LogicValue::One ? 1u : 0u) << i;
    }

    const double wallSeconds = wall.count();
    const double appliedPerSecond = static_cast<double>(sim.stats().appliedEvents) / wallSeconds;
    const double evaluationsPerSecond = static_cast<double>(sim.stats().evaluations) / wallSeconds;
    std::println("simulated_s {}", seconds);
    std::println("wall_s {:.2f}", wallSeconds);
    std::println("ratio {:.2f}", static_cast<double>(seconds) / wallSeconds);
    std::println("applied_events_per_s {}", Mega(appliedPerSecond));
    std::println("evaluations_per_s {}", Mega(evaluationsPerSecond));
    std::println("counter {:#06x}", counter);
    return appliedPerSecond < kMinAppliedEventsPerSecond ? 1 : 0;
}
