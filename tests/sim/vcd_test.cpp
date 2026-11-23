#include "sim/vcd.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

using Steps = std::vector<Stimulus::Step>;

void AddStimulus(Circuit& circuit, NetId net, LogicValue initial, Steps steps) {
    auto& stimulus = circuit.add<Stimulus>(std::string(circuit.netName(net)) + "_drv", initial,
                                           std::move(steps));
    circuit.connect(stimulus, Stimulus::kOut, net);
}

std::string Vcd(const Simulator& sim, const Circuit& circuit) {
    std::ostringstream out;
    WriteVcd(out, circuit, sim.recorder(), sim.now());
    return out.str();
}

}  // namespace

TEST_CASE("vcd: two nets exact text", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("a");
    const NetId b = circuit.addNet("bus[1]");
    AddStimulus(circuit, a, Zero, {{10_ns, One}, {30_ns, Zero}});
    AddStimulus(circuit, b, One, {{10_ns, Zero}, {20_ns, One}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(a);
    sim.record(b);
    sim.runUntil(50_ns);

    CHECK(Vcd(sim, circuit) == R"($timescale 1ps $end
$scope module bodgewire $end
$var wire 1 ! a $end
$var wire 1 " bus_1_ $end
$upscope $end
$enddefinitions $end
#0
0!
1"
#10000
1!
0"
#20000
1"
#30000
0!
#50000
)");
}

TEST_CASE("vcd: X written as x", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("a");
    AddStimulus(circuit, a, Zero, {{5_ps, X}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(a);
    sim.runUntil(10_ps);
    const std::string text = Vcd(sim, circuit);
    CHECK(text.find("#5\nx!\n#10\n") != std::string::npos);
}

TEST_CASE("vcd: manual export", "[.manual]") {
    {
        Circuit circuit;
        const NetId en = circuit.addNet("en");
        const NetId n1 = circuit.addNet("n1");
        const NetId n2 = circuit.addNet("n2");
        const NetId n3 = circuit.addNet("n3");
        AddStimulus(circuit, en, Zero, {{100_ns, One}});
        auto& nand = circuit.add<Gate>("U1", GateKind::Nand, 2, Delays{5_ns, 5_ns});
        auto& inv1 = circuit.add<Gate>("U2", GateKind::Not, 1, Delays{5_ns, 5_ns});
        auto& inv2 = circuit.add<Gate>("U3", GateKind::Not, 1, Delays{5_ns, 5_ns});
        circuit.connect(nand, "I0", en);
        circuit.connect(nand, "I1", n3);
        circuit.connect(nand, nand.output(), n1);
        circuit.connect(inv1, "I0", n1);
        circuit.connect(inv1, inv1.output(), n2);
        circuit.connect(inv2, "I0", n2);
        circuit.connect(inv2, inv2.output(), n3);
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(1);
        for (const NetId net : {en, n1, n2, n3}) {
            sim.record(net);
        }
        sim.runUntil(500_ns);
        std::ofstream file("ring_oscillator.vcd");
        WriteVcd(file, circuit, sim.recorder(), sim.now());
    }
    {
        Circuit circuit;
        const NetId clk = circuit.addNet("clk");
        auto& source = circuit.add<ClockSource>("X1", 100_ns);
        circuit.connect(source, ClockSource::kOut, clk);
        std::vector<NetId> recorded{clk};
        NetId stageClock = clk;
        for (int i = 0; i < 4; ++i) {
            const std::string index = std::to_string(i);
            const NetId q = circuit.addNet("q" + index);
            const NetId qn = circuit.addNet("qn" + index);
            auto& ff = circuit.add<DFlipFlop>("U" + index, Delays{5_ns, 5_ns});
            circuit.connect(ff, "CLK", stageClock);
            circuit.connect(ff, "D", qn);
            circuit.connect(ff, "Q", q);
            circuit.connect(ff, "Q_N", qn);
            recorded.push_back(q);
            stageClock = qn;
        }
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(1);
        for (const NetId net : recorded) {
            sim.record(net);
        }
        sim.runUntil(4_us);
        std::ofstream file("ripple_counter.vcd");
        WriteVcd(file, circuit, sim.recorder(), sim.now());
    }
    SUCCEED();
}
