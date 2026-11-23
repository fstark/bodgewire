// Quiet Pins (P5z): EvalContext::quiet and EvalContext::lastChange.
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// Inputs A, B and a sampled S. At each evaluation it records the time and A's lastChange, then
// sets the quiet state of the Pins named in `quietPins` to `quietOn`, if set.
class QuietChip final : public Component {
public:
    static constexpr PinIndex kA = 0, kB = 1, kS = 2;
    explicit QuietChip(std::string name)
        : Component(std::move(name), {{"A", PinKind::Input, Family::Ttl},
                                      {"B", PinKind::Input, Family::Ttl},
                                      {"S", PinKind::Input, Family::Ttl, false, true}}) {}
    void evaluate(EvalContext& ctx) override {
        times.push_back(ctx.now());
        lastChanges.push_back(ctx.lastChange(kA));
        if (quietOn) {
            for (const PinIndex pin : quietPins) {
                ctx.quiet(pin, *quietOn);
            }
        }
    }
    std::vector<PinIndex> quietPins{kA};
    std::optional<bool> quietOn;
    std::vector<Time> times;
    std::vector<Time> lastChanges;
};

Stimulus& Source(Circuit& circuit, const std::string& name, std::vector<Stimulus::Step> steps) {
    return circuit.add<Stimulus>(name, Zero, std::move(steps));
}

}  // namespace

TEST_CASE("quiet: a quiet pin's changes do not wake its component; loud again, they do", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    auto& source = Source(circuit, "S1", {{10_ns, One}, {20_ns, Zero}, {40_ns, One}});
    auto& chip = circuit.add<QuietChip>("U1");
    circuit.connect(source, Stimulus::kOut, a);
    circuit.connect(chip, "A", a);
    circuit.finalize();
    Simulator sim(circuit);
    chip.quietOn = true;
    sim.powerOn(1);  // the power-on evaluation quiets A
    sim.runUntil(30_ns);
    CHECK(chip.times == std::vector<Time>{0});  // A at 10 and 20 ns: asleep
    CHECK(sim.value(a) == Zero);                // the Net itself moved as ever
    chip.quietOn = false;
    sim.wake(chip.id(), 0);  // an evaluation that makes A loud again
    sim.runUntil(50_ns);
    CHECK(chip.times == std::vector<Time>{0, 30_ns, 40_ns});
}

TEST_CASE("quiet: another component on the same net is still woken", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    auto& source = Source(circuit, "S1", {{10_ns, One}});
    auto& sleeper = circuit.add<QuietChip>("U1");
    auto& listener = circuit.add<QuietChip>("U2");
    circuit.connect(source, Stimulus::kOut, a);
    circuit.connect(sleeper, "A", a);
    circuit.connect(listener, "A", a);
    circuit.finalize();
    Simulator sim(circuit);
    sleeper.quietOn = true;
    sim.powerOn(1);
    sim.runUntil(20_ns);
    CHECK(sleeper.times == std::vector<Time>{0});
    CHECK(listener.times == std::vector<Time>{0, 10_ns});
}

TEST_CASE("quiet: pins of one component on one net wake it while any of them is loud", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    auto& source = Source(circuit, "S1", {{10_ns, One}, {30_ns, Zero}});
    auto& chip = circuit.add<QuietChip>("U1");
    circuit.connect(source, Stimulus::kOut, a);
    circuit.connect(chip, "A", a);
    circuit.connect(chip, "B", a);
    circuit.finalize();
    Simulator sim(circuit);
    chip.quietOn = true;  // A only: B still listens
    sim.powerOn(1);
    sim.runUntil(20_ns);
    CHECK(chip.times == std::vector<Time>{0, 10_ns});
    chip.quietPins = {QuietChip::kA, QuietChip::kB};  // quiet A again (no change) and B
    sim.wake(chip.id(), 0);
    sim.runUntil(40_ns);
    CHECK(chip.times == std::vector<Time>{0, 10_ns, 20_ns});  // the fall at 30 ns: asleep
}

TEST_CASE("quiet: a pin that wakes nobody anyway is left as it is", "[sim]") {
    Circuit circuit;
    const NetId s = circuit.addNet("S");
    auto& source = Source(circuit, "S1", {{10_ns, One}});
    auto& chip = circuit.add<QuietChip>("U1");
    circuit.connect(source, Stimulus::kOut, s);
    circuit.connect(chip, "S", s);
    circuit.finalize();
    Simulator sim(circuit);
    chip.quietPins = {QuietChip::kS};
    chip.quietOn = true;
    sim.powerOn(1);
    chip.quietOn = false;  // loud again: still a sampled Pin, still no wake
    sim.wake(chip.id(), 0);
    sim.runUntil(20_ns);
    CHECK(chip.times == std::vector<Time>{0, 0});
}

TEST_CASE("quiet: power-on makes every pin loud", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    auto& source = Source(circuit, "S1", {{10_ns, One}});
    auto& chip = circuit.add<QuietChip>("U1");
    circuit.connect(source, Stimulus::kOut, a);
    circuit.connect(chip, "A", a);
    circuit.finalize();
    Simulator sim(circuit);
    chip.quietOn = true;
    sim.powerOn(1);
    sim.runUntil(20_ns);
    CHECK(chip.times == std::vector<Time>{0});
    chip.quietOn.reset();  // this time nothing quiets A
    chip.times.clear();
    sim.powerOn(1);
    sim.runUntil(20_ns);
    CHECK(chip.times == std::vector<Time>{0, 10_ns});
}

TEST_CASE("quiet: lastChange is when the net's value last changed, -1 before any", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    const NetId b = circuit.addNet("B");
    auto& sourceA = Source(circuit, "S1", {{10_ns, One}, {20_ns, Zero}});
    auto& sourceB = Source(circuit, "S2", {{50_ns, One}});
    auto& chip = circuit.add<QuietChip>("U1");
    circuit.connect(sourceA, Stimulus::kOut, a);
    circuit.connect(sourceB, Stimulus::kOut, b);
    circuit.connect(chip, "A", a);
    circuit.connect(chip, "B", b);
    circuit.finalize();
    Simulator sim(circuit);
    chip.quietOn = true;  // A asleep: its changes are seen through lastChange at B's wake
    sim.powerOn(1);
    sim.runUntil(60_ns);
    CHECK(chip.times == std::vector<Time>{0, 50_ns});
    CHECK(chip.lastChanges == std::vector<Time>{-1, 20_ns});
}
