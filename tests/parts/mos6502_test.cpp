#include "parts/mos6502.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;
using namespace delays;

constexpr Time kCycle = 1'000'000;  // 1 MHz
constexpr Time kAccess = 200'000;   // BenchMemory: address to data

// 64 KiB of memory on the 6502's bus: drives the byte at the address while RW is high (200 ns
// after the address last moved), floats while RW is low and stores the data when PHI2 falls.
class BenchMemory final : public Component {
public:
    explicit BenchMemory(std::array<std::uint8_t, 0x10000>& bytes)
        : Component("MEM", Pins()), bytes_(bytes) {}
    static constexpr PinIndex kRw = 16, kPhi2 = 17, kData = 18;  // A0..A15 are 0..15

    void evaluate(EvalContext& ctx) override {
        if (ctx.fell(kPhi2) && ctx.readBefore(kRw) == Zero) {
            unsigned value = 0;
            for (PinIndex bit = 0; bit < 8; ++bit) {
                value |= (ctx.readBefore(kData + bit) == One ? 1u : 0u) << bit;
            }
            bytes_[address(ctx, true)] = static_cast<std::uint8_t>(value);
        }
        if (ctx.read(kRw) == One) {
            const std::uint8_t value = bytes_[address(ctx, false)];
            for (PinIndex bit = 0; bit < 8; ++bit) {
                ctx.drive(kData + bit, (value >> bit) & 1 ? One : Zero, kAccess);
            }
        } else {
            for (PinIndex bit = 0; bit < 8; ++bit) {
                ctx.drive(kData + bit, Z, 10'000);
            }
        }
    }

private:
    static std::vector<PinSpec> Pins() {
        std::vector<PinSpec> pins;
        for (int bit = 0; bit < 16; ++bit) {
            pins.push_back({"A" + std::to_string(bit), PinKind::Input, Family::Ttl});
        }
        pins.push_back({"RW", PinKind::Input, Family::Ttl});
        pins.push_back({"PHI2", PinKind::Input, Family::Ttl});
        for (int bit = 0; bit < 8; ++bit) {
            pins.push_back({"D" + std::to_string(bit), PinKind::Bidirectional, Family::Ttl});
        }
        return pins;
    }
    static std::uint16_t address(EvalContext& ctx, bool before) {
        unsigned value = 0;
        for (PinIndex bit = 0; bit < 16; ++bit) {
            const LogicValue v = before ? ctx.readBefore(bit) : ctx.read(bit);
            value |= (v == One ? 1u : 0u) << bit;
        }
        return static_cast<std::uint16_t>(value);
    }
    std::array<std::uint8_t, 0x10000>& bytes_;
};

using Steps = std::vector<Stimulus::Step>;

struct Options {
    std::map<std::string, std::pair<LogicValue, Steps>> inputs;  // RDY, IRQ_N, NMI_N, SO, RES_N
    bool memory = true;  // else the data Nets are driven by `data`, or tied to $EA
    std::optional<std::pair<std::uint8_t, std::vector<std::pair<Time, std::uint8_t>>>> data;
    LogicValue tied = One;  // without memory or data: One ties the data Nets to $EA; X drives X
};

constexpr std::uint16_t kProgram = 0x0200, kIrqHandler = 0x0400, kNmiHandler = 0x0410;

// The 6502 on a 1 MHz clock, every Pin on its own Net and recorded, memory or stimuli on the data.
struct Bench {
    Circuit circuit;
    Mos6502* cpu = nullptr;
    std::array<std::uint8_t, 0x10000> ram{};
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;

    explicit Bench(const Options& options = {}) {
        ram.fill(0xEA);
        setVector(0xFFFA, kNmiHandler);
        setVector(0xFFFC, kProgram);
        setVector(0xFFFE, kIrqHandler);
        cpu = &circuit.add<Mos6502>("U1");
        for (const PinSpec& pin : cpu->pins()) {
            if (pin.kind == PinKind::Power || pin.kind == PinKind::NoConnect) {
                continue;
            }
            nets[pin.name] = circuit.addNet(pin.name);
            circuit.connect(*cpu, pin.name, nets[pin.name]);
        }
        auto& clock = circuit.add<ClockSource>("X1", kCycle);
        circuit.connect(clock, ClockSource::kOut, nets["PHI0"]);
        for (const char* input : {"RDY", "IRQ_N", "NMI_N", "SO", "RES_N"}) {
            const auto it = options.inputs.find(input);
            const auto [initial, steps] =
                it == options.inputs.end() ? std::pair{One, Steps{}} : it->second;
            auto& source = circuit.add<Stimulus>(std::string("S_") + input, initial, steps);
            circuit.connect(source, Stimulus::kOut, nets[input]);
        }
        if (options.memory) {
            auto& memory = circuit.add<BenchMemory>(ram);
            for (int bit = 0; bit < 16; ++bit) {
                circuit.connect(memory, "A" + std::to_string(bit), nets["A" + std::to_string(bit)]);
            }
            circuit.connect(memory, "RW", nets["RW"]);
            circuit.connect(memory, "PHI2", nets["PHI2"]);
            for (int bit = 0; bit < 8; ++bit) {
                circuit.connect(memory, "D" + std::to_string(bit), nets["D" + std::to_string(bit)]);
            }
        } else if (options.data) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                Steps steps;
                for (const auto& [time, value] : options.data->second) {
                    steps.push_back({time, (value >> bit) & 1 ? One : Zero});
                }
                auto& source =
                    circuit.add<Stimulus>("S_D" + std::to_string(bit),
                                          (options.data->first >> bit) & 1 ? One : Zero, steps);
                circuit.connect(source, Stimulus::kOut, nets["D" + std::to_string(bit)]);
            }
        } else {
            for (unsigned bit = 0; bit < 8; ++bit) {
                const NetId net = nets["D" + std::to_string(bit)];
                if (options.tied == X) {  // a tie is 0 or 1: X comes from a Stimulus
                    auto& source = circuit.add<Stimulus>("S_D" + std::to_string(bit), X, Steps{});
                    circuit.connect(source, Stimulus::kOut, net);
                } else {
                    circuit.tie(net, (0xEA >> bit) & 1 ? One : Zero, DriveStrength::Weak);
                }
            }
        }
        circuit.finalize();
    }

    void setVector(std::uint16_t at, std::uint16_t target) {
        ram[at] = static_cast<std::uint8_t>(target & 0xFF);
        ram[at + 1] = static_cast<std::uint8_t>(target >> 8);
    }
    void load(std::uint16_t at, std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            ram[at++] = b;
        }
    }
    void run(Time until, std::uint64_t seed = 1) {
        sim.emplace(circuit);
        sim->powerOn(seed);
        for (const auto& [name, net] : nets) {
            sim->record(net);
        }
        sim->runUntil(until);
    }
    std::span<const Sample> samples(const std::string& name) const {
        return sim->recorder().samples(nets.at(name));
    }
    // PHI0's edges after power-on, falling (value 0) or rising (1).
    std::vector<Time> edges(LogicValue value) const {
        std::vector<Time> times;
        const auto phi0 = samples("PHI0");
        for (std::size_t i = 1; i < phi0.size(); ++i) {
            if (phi0[i].value == value) {
                times.push_back(phi0[i].time);
            }
        }
        return times;
    }
    // The last PHI0 edge strictly before `time`.
    std::pair<Time, LogicValue> edgeBefore(Time time) const {
        const auto phi0 = samples("PHI0");
        std::pair<Time, LogicValue> last{-1, X};
        for (std::size_t i = 1; i < phi0.size() && phi0[i].time < time; ++i) {
            last = {phi0[i].time, phi0[i].value};
        }
        return last;
    }
    Sample at(const std::string& name, Time time) const {
        Sample last{};
        for (const Sample& s : samples(name)) {
            if (s.time > time) {
                break;
            }
            last = s;
        }
        return last;
    }
    std::uint16_t address(Time time) const {
        unsigned value = 0;
        for (int bit = 0; bit < 16; ++bit) {
            value |= (at("A" + std::to_string(bit), time).value == One ? 1u : 0u) << bit;
        }
        return static_cast<std::uint16_t>(value);
    }
};

// Reset held for the first 3 µs, then released.
Options WithReset() {
    Options options;
    options.inputs["RES_N"] = {Zero, Steps{{3 * kCycle, One}}};
    return options;
}

}  // namespace

TEST_CASE("6502: pins", "[parts]") {
    Mos6502 cpu("U1");
    const auto pins = cpu.pins();
    REQUIRE(pins.size() == 40);
    CHECK(pins[0].name == "GND");
    CHECK(pins[7].name == "VCC");
    CHECK(pins[20].name == "GND2");
    CHECK(pins[Mos6502::kPhi0].name == "PHI0");
    CHECK_FALSE(pins[Mos6502::kPhi0].sampled);
    CHECK_FALSE(pins[Mos6502::kPhi0].risingEdgeOnly);
    for (const PinIndex pin :
         {Mos6502::kRdy, Mos6502::kIrqN, Mos6502::kNmiN, Mos6502::kSo, Mos6502::kResN}) {
        CHECK(pins[pin].kind == PinKind::Input);
        CHECK(pins[pin].sampled);
    }
    for (std::size_t bit = 0; bit < 8; ++bit) {
        CHECK(pins[Mos6502::kData[bit]].name == "D" + std::to_string(bit));
        CHECK(pins[Mos6502::kData[bit]].kind == PinKind::Bidirectional);
        CHECK(pins[Mos6502::kData[bit]].sampled);
    }
    for (std::size_t bit = 0; bit < 16; ++bit) {
        CHECK(pins[Mos6502::kAddress[bit]].name == "A" + std::to_string(bit));
        CHECK(pins[Mos6502::kAddress[bit]].kind == PinKind::Output);
    }
}

TEST_CASE("6502: clocks and bus timing", "[parts]") {
    Bench bench;
    bench.run(40 * kCycle);
    const auto delayAfter = [&](const Sample& s) {
        const auto [edge, value] = bench.edgeBefore(s.time);
        return std::pair{s.time - edge, value};
    };
    for (const char* name : {"PHI1", "PHI2", "A0", "SYNC"}) {
        const auto samples = bench.samples(name);
        int changes = 0;
        for (std::size_t i = 1; i < samples.size(); ++i) {
            if (samples[i].time < 2 * kCycle) {
                continue;
            }
            ++changes;
            const auto [delay, edge] = delayAfter(samples[i]);
            INFO(name << " at " << samples[i].time);
            const std::string n = name;
            if (n == "PHI1") {
                CHECK(edge == (samples[i].value == One ? Zero : One));
                CHECK(delay == DelayFor(kMos6502Phi1, samples[i].value));
            } else if (n == "PHI2") {
                CHECK(edge == samples[i].value);
                CHECK(delay == DelayFor(kMos6502Phi2, samples[i].value));
            } else if (n == "A0") {
                CHECK(edge == Zero);
                CHECK(delay == kMos6502Address);
            } else {
                CHECK(edge == Zero);
                CHECK(delay == kMos6502Sync);
            }
        }
        CHECK(changes > 10);
    }
    for (const Sample& s : bench.samples("PHI1")) {
        CHECK_FALSE((s.value == One && bench.at("PHI2", s.time).value == One));
    }
    for (const Sample& s : bench.samples("PHI2")) {
        CHECK_FALSE((s.value == One && bench.at("PHI1", s.time).value == One));
    }
}

TEST_CASE("6502: reads before the edge", "[parts]") {
    // The data bus at $EA, with a JAM ($02) on it around two consecutive PHI0 falls: one of
    // them ends an opcode fetch. On the falls themselves, the CPU still reads the NOP; from
    // 100 ns before, it reads the JAM.
    Options probe;
    probe.memory = false;
    Bench first(probe);
    first.run(8 * kCycle);
    const auto falls = first.edges(Zero);
    REQUIRE(falls.size() > 6);
    const Time t0 = falls[4], t1 = falls[5];

    SECTION("changed at the edge: not seen") {
        Options options = probe;
        options.data = {0xEA, {{t0, 0x02}, {t0 + 400'000, 0xEA}, {t1, 0x02}, {t1 + 400'000, 0xEA}}};
        Bench bench(options);
        bench.run(20 * kCycle);
        CHECK_FALSE(bench.cpu->core().jammed());
    }
    SECTION("changed before the edge: seen") {
        Options options = probe;
        options.data = {0xEA,
                        {{t0 - 100'000, 0x02},
                         {t0 + 400'000, 0xEA},
                         {t1 - 100'000, 0x02},
                         {t1 + 400'000, 0xEA}}};
        Bench bench(options);
        bench.run(20 * kCycle);
        CHECK(bench.cpu->core().jammed());
    }
}

TEST_CASE("6502: runs a program", "[parts]") {
    Bench bench(WithReset());
    bench.load(kProgram, {0xA9, 0x42,              // LDA #$42
                          0x8D, 0x00, 0x03,        // STA $0300
                          0x48,                    // PHA
                          0x20, 0x10, 0x02,        // JSR $0210
                          0x8D, 0x01, 0x03,        // STA $0301
                          0x4C, 0x0C, 0x02});      // JMP *
    bench.load(0x0210, {0x18, 0x69, 0x01, 0x60});  // CLC, ADC #1, RTS
    bench.run(60 * kCycle);
    CHECK(bench.ram[0x0300] == 0x42);
    CHECK(bench.ram[0x0301] == 0x43);
    CHECK(bench.cpu->core().registers().pc >= 0x020C);
    CHECK(bench.cpu->core().registers().pc <= 0x020E);
    const std::uint8_t s = bench.cpu->core().registers().s;
    CHECK(bench.ram[0x0100 | static_cast<std::uint8_t>(s + 1)] == 0x42);  // pushed by PHA
}

TEST_CASE("6502: writes", "[parts]") {
    Bench bench(WithReset());
    bench.load(kProgram, {0xA9, 0x42, 0x8D, 0x00, 0x03, 0x4C, 0x05, 0x02});  // LDA, STA, JMP *
    bench.run(30 * kCycle);
    REQUIRE(bench.ram[0x0300] == 0x42);
    // The one write: RW low from a fall + 130 ns to the next fall + 130 ns.
    const auto rw = bench.samples("RW");
    std::vector<Time> lows, highs;
    for (std::size_t i = 1; i < rw.size(); ++i) {
        (rw[i].value == Zero ? lows : highs).push_back(rw[i].time);
    }
    REQUIRE(lows.size() == 1);
    const auto [fall, fallValue] = bench.edgeBefore(lows[0]);
    CHECK(fallValue == Zero);
    CHECK(lows[0] - fall == kMos6502ReadWrite);
    CHECK(bench.address(lows[0] + 1) == 0x0300);
    const Time rise = fall + kCycle / 2;
    const Time end = fall + kCycle;
    CHECK(std::ranges::find(highs, end + kMos6502ReadWrite) != highs.end());
    // D1 (0x42 has bit 1 set): floating until the CPU drives it, released after the hold.
    CHECK(bench.at("D1", rise + kMos6502DataOut - 1).floating);
    CHECK_FALSE(bench.at("D1", rise + kMos6502DataOut).floating);
    CHECK(bench.at("D1", rise + kMos6502DataOut).value == One);
    CHECK_FALSE(bench.at("D1", end + kMos6502DataHold - 1).floating);
    CHECK(bench.at("D1", end + kMos6502DataHold).floating);
}

TEST_CASE("6502: RDY", "[parts]") {
    Bench plain(WithReset());
    plain.load(kProgram, {0xA9, 0x42, 0x8D, 0x00, 0x03, 0x4C, 0x05, 0x02});
    plain.run(30 * kCycle);
    const auto falls = plain.edges(Zero);
    // The write cycle, from the RW-low edge.
    Time writeStart = 0;
    for (const Sample& s : plain.samples("RW")) {
        if (s.time > 0 && s.value == Zero) {
            writeStart = s.time - kMos6502ReadWrite;
        }
    }
    REQUIRE(writeStart > 0);

    SECTION("low on a read cycle holds it") {
        const Time from = falls[8] + 100'000;
        Options options = WithReset();
        options.inputs["RDY"] = {One, Steps{{from, Zero}, {from + 3 * kCycle, One}}};
        Bench bench(options);
        bench.load(kProgram, {0xA9, 0x42, 0x8D, 0x00, 0x03, 0x4C, 0x05, 0x02});
        bench.run(30 * kCycle);
        // RDY is low at the next three falls: the cycle driven at the fall before `from` runs
        // four times, its address still; at the fourth fall it moves on.
        const std::uint16_t held = bench.address(from + 100'000);
        for (int i = 1; i <= 3; ++i) {
            CHECK(bench.address(from + i * kCycle + 100'000) == held);
        }
        CHECK(bench.address(from + 4 * kCycle + 100'000) != held);
        CHECK(bench.ram[0x0300] == 0x42);
    }
    SECTION("low on a write cycle is ignored; the next read stalls") {
        const Time from = writeStart + 100'000;
        Options options = WithReset();
        options.inputs["RDY"] = {One, Steps{{from, Zero}, {from + 3 * kCycle, One}}};
        Bench bench(options);
        bench.load(kProgram, {0xA9, 0x42, 0x8D, 0x00, 0x03, 0x4C, 0x05, 0x02});
        bench.run(30 * kCycle);
        const auto rw = bench.samples("RW");
        std::vector<Time> lows;
        for (std::size_t i = 1; i < rw.size(); ++i) {
            if (rw[i].value == Zero) {
                lows.push_back(rw[i].time);
            }
        }
        REQUIRE(lows.size() == 1);
        CHECK(lows[0] == writeStart + kMos6502ReadWrite);  // the write went ahead
        const Time next = writeStart + kCycle;             // the read after it
        const std::uint16_t held = bench.address(next + kMos6502Address);
        CHECK(bench.address(next + 2 * kCycle + kMos6502Address) == held);
        CHECK(bench.address(next + 3 * kCycle + kMos6502Address) != held);
        CHECK(bench.ram[0x0300] == 0x42);
    }
}

TEST_CASE("6502: reset and interrupts through the pins", "[parts]") {
    Options options = WithReset();
    options.inputs["IRQ_N"] = {One, Steps{{30 * kCycle, Zero}, {40 * kCycle, One}}};
    options.inputs["NMI_N"] = {One, Steps{{60 * kCycle, Zero}, {61 * kCycle, One}}};
    Bench bench(options);
    bench.load(kProgram, {0x58, 0x4C, 0x01, 0x02});                       // CLI, JMP *
    bench.load(kIrqHandler, {0xA9, 0x77, 0x8D, 0x02, 0x03, 0x58, 0x40});  // LDA, STA, CLI, RTI
    bench.load(kNmiHandler, {0xA9, 0x55, 0x8D, 0x03, 0x03, 0x40});        // LDA, STA, RTI
    bench.run(90 * kCycle);
    CHECK(bench.ram[0x0302] == 0x77);
    CHECK(bench.ram[0x0303] == 0x55);
    const auto pc = bench.cpu->core().registers().pc;
    CHECK((pc >= 0x0201 && pc <= 0x0203));
}

TEST_CASE("6502: X on the data bus", "[parts]") {
    Options options;
    options.memory = false;
    options.tied = X;
    Bench bench(options);
    bench.run(50 * kCycle);
    for (const char* name : {"A0", "A7", "A15", "RW", "SYNC"}) {
        for (const Sample& s : bench.samples(name)) {
            CHECK(s.value != X);
        }
    }
}

TEST_CASE("6502: wakes only on PHI0", "[parts]") {
    Options options;
    options.memory = false;  // data Nets tied to $EA
    Bench bench(options);
    bench.run(10 * kCycle);  // past the power-on evaluations
    const auto before = bench.sim->stats().evaluations;
    const std::size_t edgesBefore = bench.samples("PHI0").size();
    bench.sim->runUntil(100 * kCycle);
    const std::size_t edges = bench.samples("PHI0").size() - edgesBefore;
    REQUIRE(edges == 180);
    // The clock evaluates once per edge, the CPU once per edge, nothing else.
    CHECK(bench.sim->stats().evaluations - before == 2 * edges);
}
