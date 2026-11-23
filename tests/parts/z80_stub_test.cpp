#include "parts/z80_stub.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
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
using Kind = Z80Stub::Kind;

constexpr Time kT = 307'692;  // 3.25 MHz
constexpr Time kLine = Z80Stub::kLineTStates * kT;
constexpr Time kFrame = Z80Stub::kFrameLines * kLine;

// The stub on a 3.25 MHz clock, every Pin on a Net of its own, all recorded.
struct Bench {
    Circuit circuit;
    Z80Stub* stub = nullptr;
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;
    Time start = 0;  // the first CLK rise: T1 of cycle 0

    explicit Bench(Time run) {
        stub = &circuit.add<Z80Stub>("U1");
        auto& clock = circuit.add<ClockSource>("X1", kT);
        for (const PinSpec& pin : stub->pins()) {
            if (pin.kind == PinKind::Power) {
                continue;
            }
            nets[pin.name] = circuit.addNet(pin.name);
            circuit.connect(*stub, pin.name, nets[pin.name]);
        }
        circuit.connect(clock, ClockSource::kOut, nets["CLK"]);
        circuit.finalize();
        sim.emplace(circuit);
        sim->powerOn(1);
        for (const auto& [name, net] : nets) {
            sim->record(net);
        }
        sim->runUntil(run);
        const auto clk = sim->recorder().samples(nets["CLK"]);
        for (std::size_t i = 1; i < clk.size(); ++i) {
            if (clk[i].value == One) {
                start = clk[i].time;
                break;
            }
        }
        REQUIRE(start > 0);
    }

    // The Net's Sample in force at `time`.
    Sample at(const std::string& name, Time time) const {
        const auto samples = sim->recorder().samples(nets.at(name));
        const auto it = std::ranges::upper_bound(samples, time, {}, &Sample::time);
        REQUIRE(it != samples.begin());
        return *(it - 1);
    }
    LogicValue value(const std::string& name, Time time) const { return at(name, time).value; }

    // A bus read at `time`; nullopt if a bit is not 0 or 1.
    std::optional<unsigned> bus(char prefix, int bits, Time time) const {
        unsigned word = 0;
        for (int bit = 0; bit < bits; ++bit) {
            const LogicValue v = value(prefix + std::to_string(bit), time);
            if (v != Zero && v != One) {
                return std::nullopt;
            }
            word |= (v == One ? 1u : 0u) << bit;
        }
        return word;
    }
    std::optional<unsigned> address(Time time) const { return bus('A', 16, time); }
    std::optional<unsigned> data(Time time) const { return bus('D', 8, time); }

    // T1 rising edge of a cycle of the first frame.
    Time t1(const Z80Stub::Cycle& c) const {
        return start + (c.line * Z80Stub::kLineTStates + c.start) * kT;
    }

    // Times at which the Net went to `value`.
    std::vector<Time> edges(const std::string& name, LogicValue value) const {
        std::vector<Time> times;
        const auto samples = sim->recorder().samples(nets.at(name));
        for (std::size_t i = 1; i < samples.size(); ++i) {
            if (samples[i].value == value && samples[i - 1].value != value) {
                times.push_back(samples[i].time);
            }
        }
        return times;
    }
};

// Index of the first cycle of `kind` on `line`.
std::size_t Find(const std::vector<Z80Stub::Cycle>& frame, int line, Kind kind) {
    const auto it = std::ranges::find_if(
        frame, [&](const Z80Stub::Cycle& c) { return c.line == line && c.kind == kind; });
    REQUIRE(it != frame.end());
    return static_cast<std::size_t>(it - frame.begin());
}

// R as the refresh of cycle `index` shows it: the M1 cycles before it in the frame, mod 128.
unsigned RefreshOf(const std::vector<Z80Stub::Cycle>& frame, std::size_t index) {
    const auto m1 = std::count_if(frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(index),
                                  [](const Z80Stub::Cycle& c) {
                                      return c.kind == Kind::Fetch || c.kind == Kind::HaltFetch ||
                                             c.kind == Kind::InterruptAcknowledge;
                                  });
    return static_cast<unsigned>(m1) & 0x7F;
}

}  // namespace

TEST_CASE("parts: Z80STUB frame is 312 lines of 207 T-states", "[parts]") {
    Circuit circuit;
    const auto& frame = circuit.add<Z80Stub>("U1").frame();
    REQUIRE(!frame.empty());
    CHECK(frame.front().line == 0);
    CHECK(frame.front().kind == Kind::In);
    CHECK(frame.back().line == Z80Stub::kFrameLines - 1);
    // Display lines start with the row's first character, from the display file's mirror.
    const auto& first = frame[Find(frame, Z80Stub::kFirstDisplayLine, Kind::Fetch)];
    CHECK(first.start == 0);
    CHECK(first.address == 0xC000);
    const auto& last = frame[Find(frame, Z80Stub::kFirstDisplayLine + 191, Kind::Fetch)];
    CHECK(last.address == 0xC000 + 23 * Z80Stub::kRowBytes);
}

TEST_CASE("parts: Z80STUB fetch", "[parts]") {
    const Time line = Z80Stub::kFirstDisplayLine * kLine;
    Bench b(line + 2 * kLine);
    const auto& frame = b.stub->frame();
    const std::size_t index = Find(frame, Z80Stub::kFirstDisplayLine, Kind::Fetch);
    const Time t1 = b.t1(frame[index]);

    CHECK(b.address(t1 + kZ80Address) == 0xC000u);
    CHECK(b.value("M1_N", t1 + kZ80M1 - 1) == One);
    CHECK(b.value("M1_N", t1 + kZ80M1) == Zero);
    CHECK(b.value("RFSH_N", t1 + kZ80Rfsh) == One);
    CHECK(b.value("MREQ_N", t1 + kT / 2 + kZ80Mreq - 1) == One);
    CHECK(b.value("MREQ_N", t1 + kT / 2 + kZ80Mreq) == Zero);
    CHECK(b.value("RD_N", t1 + kT / 2 + kZ80Rd) == Zero);
    CHECK(b.value("IORQ_N", t1 + 2 * kT) == One);
    CHECK(b.value("WR_N", t1 + 2 * kT) == One);

    // T3: the refresh, MREQ_N again from the falling edge, released at T4's falling edge.
    const Time t3 = t1 + 2 * kT;
    const unsigned r = RefreshOf(frame, index);
    CHECK(b.value("M1_N", t3 + kZ80M1) == One);
    CHECK(b.value("RD_N", t3 + kZ80Rd) == One);
    CHECK(b.value("MREQ_N", t3 + kZ80Mreq) == One);
    CHECK(b.address(t3 + kZ80M1) == 0xC000u);  // M1_N rises before the address moves
    CHECK(b.address(t3 + kZ80Address) == (0x0E00u | r));
    CHECK(b.value("RFSH_N", t3 + kZ80Rfsh) == Zero);
    CHECK(b.value("MREQ_N", t3 + kT / 2 + kZ80Mreq) == Zero);
    CHECK(b.value("MREQ_N", t3 + 3 * kT / 2 + kZ80Mreq) == One);
    CHECK(b.value("RFSH_N", t3 + 2 * kT + kZ80Rfsh) == One);

    // The next fetch: the next character, R one more.
    CHECK(b.address(t1 + 4 * kT + kZ80Address) == 0xC001u);
    CHECK(b.address(t3 + 4 * kT + kZ80Address) == (0x0E00u | ((r + 1) & 0x7F)));
    // The data pins are never driven by a fetch.
    CHECK(b.at("D0", t1 + 2 * kT).floating);
}

TEST_CASE("parts: Z80STUB read and write", "[parts]") {
    Bench b(5 * kLine);
    const auto& frame = b.stub->frame();

    const auto& read = frame[Find(frame, 4, Kind::Read)];
    const Time r1 = b.t1(read);
    CHECK(b.address(r1 + kZ80Address) == read.address);
    CHECK((read.address & 0xFC00u) == 0x4000u);
    CHECK(b.value("MREQ_N", r1 + kT / 2 + kZ80Mreq) == Zero);
    CHECK(b.value("RD_N", r1 + kT / 2 + kZ80Rd) == Zero);
    CHECK(b.value("M1_N", r1 + kT) == One);
    CHECK(b.at("D0", r1 + kT).floating);
    CHECK(b.value("MREQ_N", r1 + 5 * kT / 2 + kZ80Mreq) == One);
    CHECK(b.value("RD_N", r1 + 5 * kT / 2 + kZ80Rd) == One);

    const auto& write = frame[Find(frame, 4, Kind::Write)];
    const Time w1 = b.t1(write);
    CHECK(write.address == 0x4000u);
    CHECK(write.data == Z80Stub::Page()[0]);
    CHECK(b.address(w1 + kZ80Address) == 0x4000u);
    CHECK(b.value("MREQ_N", w1 + kT / 2 + kZ80Mreq) == Zero);
    CHECK(b.at("D0", w1 + kT / 2 + kZ80Data - 1).floating);
    CHECK(b.data(w1 + kT / 2 + kZ80Data) == unsigned{write.data});
    CHECK(b.value("WR_N", w1 + 3 * kT / 2 + kZ80Wr - 1) == One);
    CHECK(b.value("WR_N", w1 + 3 * kT / 2 + kZ80Wr) == Zero);
    CHECK(b.value("WR_N", w1 + 5 * kT / 2 + kZ80Wr) == One);
    CHECK(b.value("MREQ_N", w1 + 5 * kT / 2 + kZ80Mreq) == One);
    CHECK(b.data(w1 + 3 * kT) == unsigned{write.data});  // held to the next cycle
    CHECK(b.at("D0", w1 + 3 * kT + kZ80Data).floating);
}

TEST_CASE("parts: Z80STUB lines and frames", "[parts]") {
    Bench b(2 * kFrame);
    const auto lineOf = [&](Time time) { return (time - b.start) / kLine; };

    // Interrupt acknowledges: IORQ_N falls while M1_N is low, at T-state 148 + 2.5 of lines 4-311.
    std::vector<Time> acks;
    for (const Time t : b.edges("IORQ_N", Zero)) {
        if (b.value("M1_N", t) == Zero) {
            acks.push_back(t);
        }
    }
    REQUIRE(acks.size() == 2 * 308);
    for (const Time t : acks) {
        const Time line = lineOf(t);
        CHECK(line % Z80Stub::kFrameLines >= 4);
        CHECK(t - b.start - line * kLine == 148 * kT + 5 * kT / 2 + kZ80Iorq);
    }

    // IN from port FE once a frame, at the start of line 0; OUT at the end of line 3.
    std::vector<Time> ins, outs;
    for (const Time t : b.edges("IORQ_N", Zero)) {
        if (b.value("RD_N", t + kZ80Rd) == Zero && b.value("M1_N", t) == One) {
            ins.push_back(t);
        }
        if (b.value("WR_N", t + kZ80Wr) == Zero) {
            outs.push_back(t);
        }
    }
    REQUIRE(ins.size() == 2);
    CHECK(ins[1] - ins[0] == kFrame);
    CHECK(ins[0] == b.start + kT + kZ80Iorq);
    REQUIRE(outs.size() == 2);
    CHECK(outs[0] == b.start + 3 * kLine + 204 * kT + kZ80Iorq);
    CHECK(b.address(outs[0]) == 0xFFFEu);

    // HALT_N low only on display lines, from the 4 HALT fetches to the interrupt.
    const auto halts = b.edges("HALT_N", Zero);
    CHECK(halts.size() == 2 * 192);
    for (const Time t : halts) {
        const Time line = lineOf(t) % Z80Stub::kFrameLines;
        CHECK(line >= Z80Stub::kFirstDisplayLine);
        CHECK(line < Z80Stub::kFirstDisplayLine + 192);
    }
}

TEST_CASE("parts: Z80STUB writes the page", "[parts]") {
    Bench b(kFrame + kT);
    const auto& page = Z80Stub::Page();
    std::vector<int> memory(page.size(), -1);
    for (const Time t : b.edges("WR_N", One)) {
        if (b.value("MREQ_N", t - 1) != Zero) {
            continue;
        }
        const auto address = b.address(t - 1);
        const auto data = b.data(t - 1);
        REQUIRE(address.has_value());
        REQUIRE(data.has_value());
        if (*address >= 0x4000 && *address < 0x4000 + page.size()) {
            memory[*address - 0x4000] = static_cast<int>(*data);
        }
    }
    for (std::size_t i = 0; i < page.size(); ++i) {
        INFO("cell " << i);
        CHECK(memory[i] == page[i]);
    }
}
