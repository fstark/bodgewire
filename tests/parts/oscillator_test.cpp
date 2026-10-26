#include "parts/oscillator.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "sim/circuit.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// High and low times of OUT over 10 periods, for several seeds.
void CheckDuty(Circuit& circuit, NetId out, Time period, Time high, Time low) {
    Simulator sim(circuit);
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        sim.powerOn(seed);
        sim.record(out);
        sim.runFor(10 * period);
        const auto samples = sim.recorder().samples(out);
        REQUIRE(samples.size() > 10);
        for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
            const Time held = samples[i + 1].time - samples[i].time;
            CHECK(held == (samples[i].value == One ? high : low));
        }
    }
}

std::string CreateError(std::vector<PartParam> params) {
    Circuit circuit;
    const auto created = FindPart("OSC")->create(circuit, "X1", params);
    return created ? std::string() : created.error();
}

}  // namespace

TEST_CASE("oscillator: PeriodFromFrequency", "[parts]") {
    CHECK(PeriodFromFrequency("1MHz") == 1'000'000);
    CHECK(PeriodFromFrequency("14.31818MHz") == 69'841);
    CHECK(PeriodFromFrequency("50Hz") == 20'000'000'000);
    CHECK(PeriodFromFrequency("2.5kHz") == 400'000'000);
    for (const char* bad : {"", "MHz", "10", "10GHz", "0Hz", "1.MHz", ".5MHz"}) {
        INFO(bad);
        CHECK_FALSE(PeriodFromFrequency(bad).has_value());
    }
    CHECK(PeriodFromFrequency("10GHz").error() ==
          "bad frequency '10GHz': expected a number then Hz, kHz or MHz, e.g. 14.31818MHz");
    CHECK(PeriodFromFrequency("0Hz").error() == "frequency must be > 0");
    CHECK(PeriodFromFrequency("900000MHz").error() == "frequency too high");
}

TEST_CASE("oscillator: period and duty", "[parts]") {
    SECTION("1 MHz from the catalogue") {
        Circuit circuit;
        const std::vector<PartParam> params{{"freq", "1MHz"}};
        Component& osc = *FindPart("OSC")->create(circuit, "X1", params).value();
        const NetId out = circuit.addNet("CLK");
        circuit.connect(osc, "OUT", out);
        circuit.finalize();
        CheckDuty(circuit, out, 1_us, 500_ns, 500_ns);
    }
    SECTION("odd period") {
        Circuit circuit;
        auto& osc = circuit.add<Oscillator>("X1", 1001);
        const NetId out = circuit.addNet("CLK");
        circuit.connect(osc, Oscillator::kOut, out);
        circuit.finalize();
        CheckDuty(circuit, out, 1001, 500, 501);
    }
}

TEST_CASE("catalogue: OSC parameter errors", "[parts]") {
    CHECK(CreateError({}) == "OSC needs freq=<frequency>");
    CHECK(CreateError({{"foo", "1"}}) == "OSC: unknown parameter 'foo'");
    CHECK(CreateError({{"freq", "fast"}}) ==
          "bad frequency 'fast': expected a number then Hz, kHz or MHz, e.g. 14.31818MHz");
    CHECK(CreateError({{"freq", "1MHz"}}).empty());
}
