#include "parts/z80.hpp"

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

constexpr Time kCycle = 308'000;   // 3.25 MHz, rounded to an even half
constexpr Time kAccess = 150'000;  // BenchMemory: RD_N low to data

// 64 KiB of memory and a port on the Z80's bus: drives the byte at the address while RD_N and
// MREQ_N are low (150 ns after RD_N fell), the port byte while RD_N and IORQ_N are low, $FF
// during an interrupt acknowledge (M1_N and IORQ_N low); stores at WR_N's rise.
class BenchMemory final : public Component {
public:
    explicit BenchMemory(std::array<std::uint8_t, 0x10000>& bytes)
        : Component("MEM", Pins()), bytes_(bytes) {}
    static constexpr PinIndex kMreqN = 16, kIorqN = 17, kRdN = 18, kWrN = 19, kM1N = 20,
                              kData = 21;  // A0..A15 are 0..15

    std::uint8_t port = 0x5A, vector = 0xFF;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> outs;

    void evaluate(EvalContext& ctx) override {
        // A write ends when its strobes rise: WR_N before MREQ_N, IORQ_N before WR_N (UM0080
        // delays), so each is taken at the first of the two to rise.
        if (ctx.rose(kWrN) && ctx.readBefore(kMreqN) == Zero) {
            bytes_[address(ctx)] = data(ctx);
        }
        if (ctx.rose(kIorqN) && ctx.readBefore(kWrN) == Zero) {
            outs.emplace_back(address(ctx), data(ctx));
        }
        const bool rd = ctx.read(kRdN) == Zero;
        std::optional<std::uint8_t> drive;
        if (rd && ctx.read(kMreqN) == Zero) {
            drive = bytes_[address(ctx)];
        } else if (rd && ctx.read(kIorqN) == Zero) {
            drive = port;
        } else if (ctx.read(kM1N) == Zero && ctx.read(kIorqN) == Zero) {
            drive = vector;
        }
        for (PinIndex bit = 0; bit < 8; ++bit) {
            if (drive) {
                ctx.drive(kData + bit, (*drive >> bit) & 1 ? One : Zero, kAccess);
            } else {
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
        for (const char* name : {"MREQ_N", "IORQ_N", "RD_N", "WR_N", "M1_N"}) {
            pins.push_back({name, PinKind::Input, Family::Ttl});
        }
        for (int bit = 0; bit < 8; ++bit) {
            pins.push_back({"D" + std::to_string(bit), PinKind::Bidirectional, Family::Ttl});
        }
        return pins;
    }
    static std::uint8_t data(EvalContext& ctx) {
        unsigned value = 0;
        for (PinIndex bit = 0; bit < 8; ++bit) {
            value |= (ctx.readBefore(kData + bit) == One ? 1u : 0u) << bit;
        }
        return static_cast<std::uint8_t>(value);
    }
    static std::uint16_t address(EvalContext& ctx) {
        unsigned value = 0;
        for (PinIndex bit = 0; bit < 16; ++bit) {
            value |= (ctx.read(bit) == One ? 1u : 0u) << bit;
        }
        return static_cast<std::uint16_t>(value);
    }
    std::array<std::uint8_t, 0x10000>& bytes_;
};

using Steps = std::vector<Stimulus::Step>;

struct Options {
    // INT_N, NMI_N, WAIT_N, BUSRQ_N, RESET_N; RESET_N low for 4 cycles unless given.
    std::map<std::string, std::pair<LogicValue, Steps>> inputs;
    bool memory = true;  // else the data Nets tied to `tied` (X from a Stimulus)
    std::uint8_t tied = 0x00;
    bool tiedX = false;
    // No memory: D from a Stimulus, a byte, then bytes at times.
    std::optional<std::pair<std::uint8_t, std::vector<std::pair<Time, std::uint8_t>>>> dataStep;
};

// The Z80 on a 3.25 MHz clock, every Pin on its own Net and recorded, memory on the data.
struct Bench {
    Circuit circuit;
    Z80* cpu = nullptr;
    BenchMemory* memory = nullptr;
    std::array<std::uint8_t, 0x10000> ram{};
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;

    explicit Bench(const Options& options = {}) {
        cpu = &circuit.add<Z80>("U1");
        for (const PinSpec& pin : cpu->pins()) {
            if (pin.kind == PinKind::Power) {
                continue;
            }
            nets[pin.name] = circuit.addNet(pin.name);
            circuit.connect(*cpu, pin.name, nets[pin.name]);
        }
        auto& clock = circuit.add<ClockSource>("X1", kCycle);
        circuit.connect(clock, ClockSource::kOut, nets["CLK"]);
        for (const char* input : {"INT_N", "NMI_N", "WAIT_N", "BUSRQ_N", "RESET_N"}) {
            const auto it = options.inputs.find(input);
            std::pair<LogicValue, Steps> drive{One, Steps{}};
            if (it != options.inputs.end()) {
                drive = it->second;
            } else if (std::string(input) == "RESET_N") {
                drive = {Zero, Steps{{4 * kCycle, One}}};
            }
            auto& source =
                circuit.add<Stimulus>(std::string("S_") + input, drive.first, drive.second);
            circuit.connect(source, Stimulus::kOut, nets[input]);
        }
        if (options.memory) {
            memory = &circuit.add<BenchMemory>(ram);
            for (int bit = 0; bit < 16; ++bit) {
                circuit.connect(*memory, "A" + std::to_string(bit),
                                nets["A" + std::to_string(bit)]);
            }
            for (const char* name : {"MREQ_N", "IORQ_N", "RD_N", "WR_N", "M1_N"}) {
                circuit.connect(*memory, name, nets[name]);
            }
            for (int bit = 0; bit < 8; ++bit) {
                circuit.connect(*memory, "D" + std::to_string(bit),
                                nets["D" + std::to_string(bit)]);
            }
        } else {
            for (unsigned bit = 0; bit < 8; ++bit) {
                const NetId net = nets["D" + std::to_string(bit)];
                if (options.dataStep) {
                    Steps steps;
                    for (const auto& [time, value] : options.dataStep->second) {
                        steps.push_back({time, (value >> bit) & 1 ? One : Zero});
                    }
                    auto& source = circuit.add<Stimulus>(
                        "S_D" + std::to_string(bit),
                        (options.dataStep->first >> bit) & 1 ? One : Zero, steps);
                    circuit.connect(source, Stimulus::kOut, net);
                } else if (options.tiedX) {
                    auto& source = circuit.add<Stimulus>("S_D" + std::to_string(bit), X, Steps{});
                    circuit.connect(source, Stimulus::kOut, net);
                } else {
                    circuit.tie(net, (options.tied >> bit) & 1 ? One : Zero, DriveStrength::Weak);
                }
            }
        }
        circuit.finalize();
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
    // The last CLK edge strictly before `time`.
    std::pair<Time, LogicValue> edgeBefore(Time time) const {
        const auto clk = samples("CLK");
        std::pair<Time, LogicValue> last{-1, X};
        for (std::size_t i = 1; i < clk.size() && clk[i].time < time; ++i) {
            last = {clk[i].time, clk[i].value};
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
    // Times `name` changed to `value` after `from`.
    std::vector<Time> changes(const std::string& name, LogicValue value, Time from = 0) const {
        std::vector<Time> times;
        const auto s = samples(name);
        for (std::size_t i = 1; i < s.size(); ++i) {
            if (s[i].time > from && s[i].value == value && !s[i].floating) {
                times.push_back(s[i].time);
            }
        }
        return times;
    }
    // Each change of `name` after `from` comes `delay` after a CLK edge of the given direction.
    void checkDelays(const std::string& name, LogicValue value, LogicValue clockEdge, Time delay,
                     Time from) const {
        const auto times = changes(name, value, from);
        CHECK_FALSE(times.empty());
        for (const Time t : times) {
            INFO(name << " → " << (value == One ? 1 : 0) << " at " << t);
            const auto [edge, direction] = edgeBefore(t);
            CHECK(t - edge == delay);
            CHECK(direction == clockEdge);
        }
    }
};

constexpr Time kAfterReset = 6 * kCycle;

}  // namespace

TEST_CASE("Z80: pins", "[parts]") {
    Z80 cpu("U1");
    const auto pins = cpu.pins();
    REQUIRE(pins.size() == 40);
    CHECK(pins[10].name == "VCC");
    CHECK(pins[28].name == "GND");
    CHECK(pins[10].family == Family::Ttl);
    CHECK(pins[Z80::kClk].name == "CLK");
    CHECK_FALSE(pins[Z80::kClk].sampled);
    CHECK_FALSE(pins[Z80::kClk].risingEdgeOnly);
    for (const PinIndex pin : {Z80::kIntN, Z80::kNmiN, Z80::kWaitN, Z80::kBusrqN, Z80::kResetN}) {
        CHECK(pins[pin].kind == PinKind::Input);
        CHECK(pins[pin].sampled);
        CHECK(pins[pin].family == Family::Mos);
    }
    for (std::size_t bit = 0; bit < 8; ++bit) {
        CHECK(pins[Z80::kData[bit]].name == "D" + std::to_string(bit));
        CHECK(pins[Z80::kData[bit]].kind == PinKind::Bidirectional);
        CHECK(pins[Z80::kData[bit]].sampled);
    }
    for (std::size_t bit = 0; bit < 16; ++bit) {
        CHECK(pins[Z80::kAddress[bit]].name == "A" + std::to_string(bit));
        CHECK(pins[Z80::kAddress[bit]].kind == PinKind::Output);
    }
    for (const PinIndex pin : {Z80::kM1N, Z80::kMreqN, Z80::kIorqN, Z80::kRdN, Z80::kWrN,
                               Z80::kRfshN, Z80::kHaltN, Z80::kBusakN}) {
        CHECK(pins[pin].kind == PinKind::Output);
    }
}

TEST_CASE("Z80: M1 timing", "[parts]") {
    Bench bench;  // NOPs from $0000 after reset
    bench.run(40 * kCycle);
    bench.checkDelays("M1_N", Zero, One, kZ80M1, kAfterReset);
    bench.checkDelays("M1_N", One, One, kZ80M1, kAfterReset);
    bench.checkDelays("RD_N", Zero, Zero, kZ80Rd, kAfterReset);
    bench.checkDelays("RD_N", One, One, kZ80Rd, kAfterReset);
    bench.checkDelays("MREQ_N", Zero, Zero, kZ80Mreq, kAfterReset);
    bench.checkDelays("RFSH_N", Zero, One, kZ80Rfsh, kAfterReset);
    bench.checkDelays("RFSH_N", One, One, kZ80Rfsh, kAfterReset);
    bench.checkDelays("A0", One, One, kZ80Address, kAfterReset);
    // A fetch is 4 clocks: M1_N falls every 4.
    const auto m1 = bench.changes("M1_N", Zero, kAfterReset);
    REQUIRE(m1.size() >= 4);
    for (std::size_t i = 1; i < m1.size(); ++i) {
        CHECK(m1[i] - m1[i - 1] == 4 * kCycle);
    }
    // The fetches count up from $0000; the refresh address shows I = 0 and R counting.
    const auto rfsh = bench.changes("RFSH_N", Zero, kAfterReset);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(bench.address(m1[i] + kZ80Address) == i);
        CHECK(bench.address(rfsh[i] + kZ80Address) == i);
    }
}

TEST_CASE("Z80: read, write, in, out timing", "[parts]") {
    Bench bench;
    bench.load(0x0000, {0x3A, 0x00, 0x20,  // LD A,($2000)
                        0x32, 0x01, 0x20,  // LD ($2001),A
                        0xD3, 0x10,        // OUT ($10),A
                        0xDB, 0x11,        // IN A,($11)
                        0x32, 0x02, 0x20,  // LD ($2002),A
                        0x18, 0xFE});      // JR $
    bench.ram[0x2000] = 0x77;
    bench.run(120 * kCycle);
    CHECK(bench.ram[0x2001] == 0x77);
    CHECK(bench.ram[0x2002] == 0x5A);
    REQUIRE(bench.memory->outs.size() == 1);
    CHECK(bench.memory->outs[0] == std::pair<std::uint16_t, std::uint8_t>{0x7710, 0x77});
    // WR_N falls at T2's fall for a memory write, at T2's rise for an OUT; both rise at T3's
    // fall. IORQ_N falls at T2's rise, rises at T3's fall.
    const auto wrFalls = bench.changes("WR_N", Zero, kAfterReset);
    REQUIRE(wrFalls.size() == 3);
    for (const Time t : wrFalls) {
        const auto [edge, direction] = bench.edgeBefore(t);
        CHECK(t - edge == kZ80Wr);
        const bool io = bench.at("IORQ_N", t + kZ80Iorq).value == Zero;
        CHECK(direction == (io ? One : Zero));
    }
    bench.checkDelays("WR_N", One, Zero, kZ80Wr, kAfterReset);
    bench.checkDelays("IORQ_N", Zero, One, kZ80Iorq, kAfterReset);
    bench.checkDelays("IORQ_N", One, Zero, kZ80Iorq, kAfterReset);
    // Written data: driven at T1's fall, released at the next T1's rise.
    const auto wr = bench.changes("WR_N", Zero, kAfterReset);
    REQUIRE(wr.size() >= 1);
    const Time t1Fall = wr[0] - kZ80Wr - kCycle;  // WR_N falls at T2's fall
    CHECK(bench.at("D0", t1Fall + kZ80Data).value == One);
    CHECK_FALSE(bench.at("D0", t1Fall + kZ80Data).floating);
    CHECK(bench.at("D0", t1Fall + kZ80Data - 1).floating);
}

TEST_CASE("Z80: reads before the edge", "[parts]") {
    // No memory: the data Nets give NOPs, then HALT ($76) from a chosen time. The opcode is taken
    // at T3's rising edge: a change at that edge is not seen, one just before it is.
    Options probe;  // the same circuit, the step far away: the same power-on draws and timing
    probe.memory = false;
    probe.dataStep = {std::uint8_t{0x00}, {{1'000 * kCycle, std::uint8_t{0x00}}}};
    Bench first(probe);
    first.run(40 * kCycle);
    const auto rfsh = first.changes("RFSH_N", Zero, kAfterReset);
    REQUIRE(rfsh.size() >= 3);
    const Time t3 = rfsh[2] - kZ80Rfsh;  // the third fetch's T3 rise
    for (const auto& [at, halts] : {std::pair{t3, false}, std::pair{t3 - 1, true}}) {
        Options options;
        options.memory = false;
        options.dataStep = {std::uint8_t{0x00},
                            {{at, std::uint8_t{0x76}}, {t3 + kCycle, std::uint8_t{0x00}}}};
        Bench bench(options);
        bench.run(t3 + 10 * kCycle);
        INFO("data at " << at << ", T3 at " << t3);
        CHECK(bench.changes("HALT_N", Zero).empty() != halts);
    }
}

TEST_CASE("Z80: runs a program", "[parts]") {
    Bench bench;
    bench.load(0x0000, {0x31, 0x00, 0x80,                      // LD SP,$8000
                        0x3E, 0x42,                            // LD A,$42
                        0x32, 0x00, 0x40,                      // LD ($4000),A
                        0x01, 0x34, 0x12,                      // LD BC,$1234
                        0xC5,                                  // PUSH BC
                        0xCD, 0x40, 0x00,                      // CALL $0040
                        0x21, 0x00, 0x30,                      // LD HL,$3000
                        0x11, 0x10, 0x40,                      // LD DE,$4010
                        0x01, 0x04, 0x00,                      // LD BC,4
                        0xED, 0xB0,                            // LDIR
                        0x06, 0x03,                            // LD B,3
                        0x3C,                                  // loop: INC A
                        0x10, 0xFD,                            // DJNZ loop
                        0xD3, 0x20,                            // OUT ($20),A
                        0x76});                                // HALT
    bench.load(0x0040, {0x3E, 0x99, 0x32, 0x01, 0x40, 0xC9});  // LD A,$99; LD ($4001),A; RET
    bench.load(0x3000, {1, 2, 3, 4});
    bench.run(400 * kCycle);
    CHECK(bench.ram[0x4000] == 0x42);
    CHECK(bench.ram[0x4001] == 0x99);
    CHECK(bench.ram[0x7FFF] == 0x12);  // PUSH BC
    CHECK(bench.ram[0x7FFE] == 0x34);
    CHECK(bench.ram[0x4010] == 1);
    CHECK(bench.ram[0x4013] == 4);
    REQUIRE(bench.memory->outs.size() == 1);
    CHECK(bench.memory->outs[0].second == 0x9C);
    CHECK_FALSE(bench.changes("HALT_N", Zero).empty());
}

TEST_CASE("Z80: WAIT", "[parts]") {
    Bench first;
    first.run(30 * kCycle);
    const auto rd = first.changes("RD_N", Zero, kAfterReset);
    REQUIRE(rd.size() >= 2);
    const Time t2Fall = rd[1] - kZ80Rd + kCycle;  // the second fetch's T2 fall
    Options options;
    options.inputs["WAIT_N"] = {One,
                                Steps{{t2Fall - 50'000, Zero}, {t2Fall + kCycle + 50'000, One}}};
    Bench bench(options);
    bench.run(30 * kCycle);
    const auto falls = bench.changes("RD_N", Zero, kAfterReset);
    const auto rises = bench.changes("RD_N", One, kAfterReset);
    REQUIRE(falls.size() >= 3);
    REQUIRE(rises.size() >= 3);
    CHECK(rises[0] - falls[0] == 3 * kCycle / 2);  // T1↓ to T3↑
    CHECK(rises[1] - falls[1] == 7 * kCycle / 2);  // two wait states seen
    CHECK(falls[2] - falls[1] == 6 * kCycle);      // the fetch is 6 clocks
}

TEST_CASE("Z80: BUSRQ", "[parts]") {
    Options options;
    options.inputs["BUSRQ_N"] = {One, Steps{{20 * kCycle, Zero}, {30 * kCycle, One}}};
    Bench bench(options);
    bench.load(0x0000, {0x3E, 0x42, 0x32, 0x00, 0x40, 0x3C, 0x32, 0x01, 0x40, 0x18, 0xFE});
    bench.run(80 * kCycle);
    const auto busak = bench.changes("BUSAK_N", Zero);
    REQUIRE(busak.size() == 1);
    const Time granted = busak[0] + kZ80Busak;
    CHECK(granted < 30 * kCycle);
    CHECK(bench.at("A0", granted).floating);
    CHECK(bench.at("MREQ_N", granted).floating);
    CHECK(bench.at("RD_N", granted).floating);
    CHECK_FALSE(bench.at("M1_N", granted).floating);
    CHECK_FALSE(bench.at("RFSH_N", granted).floating);
    CHECK(bench.changes("BUSAK_N", One).size() == 1);
    CHECK(bench.ram[0x4000] == 0x42);
    CHECK(bench.ram[0x4001] == 0x43);
}

TEST_CASE("Z80: RESET_N", "[parts]") {
    Bench bench;
    bench.load(0x0000, {0x3E, 0x42, 0x32, 0x00, 0x40, 0x18, 0xFE});
    bench.run(60 * kCycle);
    const Time during = 3 * kCycle;
    CHECK(bench.at("A0", during).floating);
    CHECK(bench.at("D0", during).floating);
    for (const char* pin : {"MREQ_N", "IORQ_N", "RD_N", "WR_N", "M1_N", "RFSH_N", "HALT_N"}) {
        INFO(pin);
        CHECK(bench.at(pin, during).value == One);
        CHECK_FALSE(bench.at(pin, during).floating);
    }
    const auto m1 = bench.changes("M1_N", Zero, 4 * kCycle);
    REQUIRE_FALSE(m1.empty());
    CHECK(bench.address(m1[0] - kZ80M1 + kZ80Address) == 0x0000);
    CHECK(bench.ram[0x4000] == 0x42);
}

TEST_CASE("Z80: INT and NMI through the pins", "[parts]") {
    SECTION("IM 1") {
        Options options;
        options.inputs["INT_N"] = {One, Steps{{60 * kCycle, Zero}, {70 * kCycle, One}}};
        Bench bench(options);
        bench.load(0x0000, {0x31, 0x00, 0x80, 0xED, 0x56, 0xFB, 0x18, 0xFE});  // SP; IM 1; EI; JR $
        bench.load(0x0038, {0x3E, 0x55, 0x32, 0x00, 0x40, 0x18, 0xFE});        // LD ($4000),$55
        bench.run(150 * kCycle);
        CHECK(bench.ram[0x4000] == 0x55);
        // The acknowledge: IORQ_N low (after Tw1's fall) while M1_N is low.
        const auto iorq = bench.changes("IORQ_N", Zero);
        REQUIRE(iorq.size() == 1);
        CHECK(bench.at("M1_N", iorq[0]).value == Zero);
        const auto [edge, direction] = bench.edgeBefore(iorq[0]);
        CHECK(iorq[0] - edge == kZ80Iorq);
        CHECK(direction == Zero);
        const auto m1 = bench.changes("M1_N", Zero);
        Time ackStart = 0;
        for (const Time t : m1) {
            if (t < iorq[0]) {
                ackStart = t;
            }
        }
        CHECK(iorq[0] - ackStart == 2 * kCycle + kCycle / 2 - kZ80M1 + kZ80Iorq);
        CHECK(bench.ram[0x7FFF] == 0x00);  // the pushed PC: the JR $ at $0006
        CHECK(bench.ram[0x7FFE] == 0x06);
    }
    SECTION("NMI") {
        Options options;
        options.inputs["NMI_N"] = {One, Steps{{40 * kCycle, Zero}, {41 * kCycle, One}}};
        Bench bench(options);
        bench.load(0x0000, {0x31, 0x00, 0x80, 0x18, 0xFE});
        bench.load(0x0066, {0x3E, 0x66, 0x32, 0x00, 0x40, 0x18, 0xFE});
        bench.run(120 * kCycle);
        CHECK(bench.ram[0x4000] == 0x66);
    }
}

TEST_CASE("Z80: HALT_N", "[parts]") {
    Options options;
    options.inputs["INT_N"] = {One, Steps{{80 * kCycle, Zero}, {90 * kCycle, One}}};
    Bench bench(options);
    bench.load(0x0000, {0x31, 0x00, 0x80, 0xED, 0x56, 0xFB, 0x76, 0x18, 0xFE});
    bench.load(0x0038, {0x18, 0xFE});
    bench.run(150 * kCycle);
    const auto low = bench.changes("HALT_N", Zero);
    const auto high = bench.changes("HALT_N", One, kAfterReset);
    REQUIRE(low.size() == 1);
    REQUIRE(high.size() == 1);
    // 300 ns: longer than half a clock, so the edge is looked up at the drive's start.
    const Time driven = low[0] - kZ80Halt;
    const auto [edge, direction] = bench.edgeBefore(driven + 1);
    CHECK(edge == driven);
    CHECK(direction == Zero);
    CHECK(high[0] > 80 * kCycle);
    CHECK(high[0] < 90 * kCycle);
}

TEST_CASE("Z80: X on the data bus", "[parts]") {
    Options options;
    options.memory = false;
    options.tiedX = true;
    Bench bench(options);
    bench.run(100 * kCycle);
    for (const char* name : {"A0", "A7", "A15", "M1_N", "MREQ_N", "RD_N", "WR_N", "RFSH_N"}) {
        for (const Sample& s : bench.samples(name)) {
            CHECK(s.value != X);
        }
    }
}

TEST_CASE("Z80: wakes only on CLK", "[parts]") {
    Options options;
    options.memory = false;  // data Nets tied to $00 (NOP)
    options.inputs["RESET_N"] = {One, Steps{}};
    // Waits and a bus grant on NOPs: no interrupt (its push would drive the data Nets).
    options.inputs["WAIT_N"] = {One, Steps{{20 * kCycle + 1000, Zero}, {25 * kCycle + 1000, One}}};
    options.inputs["BUSRQ_N"] = {One, Steps{{50 * kCycle + 1000, Zero}, {60 * kCycle + 1000, One}}};
    Bench bench(options);
    bench.run(10 * kCycle);  // past the power-on evaluations
    const auto before = bench.sim->stats().evaluations;
    const std::size_t edgesBefore = bench.samples("CLK").size();
    bench.sim->runUntil(100 * kCycle);
    const std::size_t edges = bench.samples("CLK").size() - edgesBefore;
    REQUIRE(edges == 180);
    // The clock evaluates once per edge, the CPU once per edge, each Stimulus once a step (4).
    CHECK(bench.sim->stats().evaluations - before == 2 * edges + 4);
}
