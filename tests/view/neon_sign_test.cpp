#include "view/neon_sign.hpp"

#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

constexpr int kLetters = 9;  // BODGEWIRE
constexpr double kStep = 0.01;

bool Steady(const NeonSign& sign) { return sign.sign == 1 && sign.letter == -1; }

}  // namespace

TEST_CASE("neon sign: strikes, then steady until 4 s") {
    CHECK(NeonSignAt(0, kLetters).sign == 0);
    for (double t = 0.4; t < 4; t += kStep) {
        CHECK(Steady(NeonSignAt(t, kLetters)));
    }
}

TEST_CASE("neon sign: in an hour, letters fail and the supply dips, and the sign never flashes") {
    std::vector<double> changes;  // when the whole sign's brightness changed
    float last = NeonSignAt(0, kLetters).sign;
    int failingSteps = 0;
    int halfLit = 0;
    int dips = 0;
    const int steps = static_cast<int>(3600 / kStep);
    for (int i = 0; i < steps; ++i) {
        const double t = i * kStep;
        const NeonSign sign = NeonSignAt(t, kLetters);
        REQUIRE(sign.sign >= 0);
        REQUIRE(sign.sign <= 1);
        REQUIRE(sign.letterBrightness >= 0);
        REQUIRE(sign.letterBrightness <= 1);
        REQUIRE(sign.letter >= -1);
        REQUIRE(sign.letter < kLetters);
        if (sign.sign != last) {
            changes.push_back(t);
            dips += sign.sign < last && t > 1 ? 1 : 0;
            last = sign.sign;
        }
        failingSteps += sign.letter >= 0 ? 1 : 0;
        halfLit += sign.lit != NeonPart::Whole ? 1 : 0;
    }
    CHECK(dips > 20);
    CHECK(halfLit > 0);
    // A letter fails about a tenth of the time: an event, not the look.
    CHECK(failingSteps > steps / 40);
    CHECK(failingSteps < steps / 6);
    // A flash is two changes: at most 2 in any second (the guideline's 3).
    for (std::size_t i = 0; i < changes.size(); ++i) {
        std::size_t inSecond = 0;
        for (std::size_t j = i; j < changes.size() && changes[j] < changes[i] + 1; ++j) {
            ++inSecond;
        }
        CHECK(inSecond <= 4);
    }
}
