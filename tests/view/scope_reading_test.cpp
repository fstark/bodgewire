#include "view/scope_reading.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

constexpr LogicValue H = LogicValue::One, L = LogicValue::Zero;

// A square wave from 0: `high` ps high then `low` low, until `end`.
std::vector<Sample> Square(Time high, Time low, Time end) {
    std::vector<Sample> samples{{0, L}};
    for (Time t = 500_ns; t < end; t += high + low) {
        samples.push_back({t, H});
        if (t + high < end) {
            samples.push_back({t + high, L});
        }
    }
    return samples;
}

}  // namespace

TEST_CASE("scope reading: a clock", "[view]") {
    CHECK(BandReading(Square(500_ns, 500_ns, 10_us), 0, 10_us, 0, std::nullopt) == "1 MHz · 50 %");
    CHECK(BandReading(Square(250_ns, 750_ns, 10_us), 0, 10_us, 0, std::nullopt) == "1 MHz · 25 %");
    // Edges past `to` (under the screen's right edge) are left out: two rises in 2 µs.
    CHECK(BandReading(Square(1_us, 1_us, 10_us), 0, 2'600'000, 0, std::nullopt) ==
          "500 kHz · 50 %");
}

TEST_CASE("scope reading: a pulse", "[view]") {
    const std::vector<Sample> high{{0, L}, {1_us, H}, {2_us, L}};
    CHECK(BandReading(high, 0, 10_us, 0, std::nullopt) == "high 1 µs");
    const std::vector<Sample> low{{0, H}, {2_us, L}, {6_us, H}};
    CHECK(BandReading(low, 0, 10_us, 0, std::nullopt) == "low 4 µs");
}

TEST_CASE("scope reading: an edge, from the Trigger", "[view]") {
    const std::vector<Sample> rise{{0, L}, {3_us, H}};
    CHECK(BandReading(rise, 0, 10_us, 1_us, std::nullopt) == "rises +2 µs");
    const std::vector<Sample> fall{{0, H}, {3_us, L}};
    CHECK(BandReading(fall, 0, 10_us, 5_us, std::nullopt) == "falls -2 µs");
    const std::vector<Sample> floats{{0, L}, {3_us, H, true}};
    CHECK(BandReading(floats, 0, 10_us, 3_us, std::nullopt) == "floats 0");
}

TEST_CASE("scope reading: a level", "[view]") {
    CHECK(BandReading(std::vector<Sample>{{0, L}}, 0, 10_us, 0, std::nullopt) == "low");
    CHECK(BandReading(std::vector<Sample>{{0, H}}, 0, 10_us, 0, std::nullopt) == "high");
    CHECK(BandReading(std::vector<Sample>{{0, H, true}}, 0, 10_us, 0, std::nullopt) ==
          "floating 1.4 V");
    CHECK(BandReading(std::vector<Sample>{{0, LogicValue::X}}, 0, 10_us, 0, std::nullopt) ==
          "unknown");
    CHECK(BandReading(std::vector<Sample>{{0, L}}, 0, 10_us, 0, 5.0) == "5.0 V");
    CHECK(BandReading({}, 0, 10_us, 0, std::nullopt).empty());
}

TEST_CASE("scope reading: a float driven high is a rise", "[view]") {
    // Floating (its logic One) between low pulses: each drive high after it is a rising edge.
    const std::vector<Sample> samples{{0, L},          {1_us, H, true}, {2_us, H}, {3_us, L},
                                      {4_us, H, true}, {5_us, H},       {6_us, L}};
    CHECK(BandReading(samples, 0, 10_us, 0, std::nullopt) == "333 kHz · 33 %");
}
