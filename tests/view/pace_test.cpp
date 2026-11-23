#include "view/pace.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace std::chrono_literals;

namespace {

const PaceWatch::Clock::time_point t0{};

}  // namespace

TEST_CASE("pace: behind only once under the bar for 2 s") {
    PaceWatch watch;
    CHECK_FALSE(watch.update(true, 1.0, t0).behind);
    CHECK_FALSE(watch.update(true, 0.8, t0 + 1s).behind);
    CHECK_FALSE(watch.update(true, 0.8, t0 + 2500ms).behind);
    CHECK(watch.update(true, 0.8, t0 + 3s).behind);
}

TEST_CASE("pace: a dip shorter than 2 s waits again") {
    PaceWatch watch;
    CHECK_FALSE(watch.update(true, 0.8, t0).behind);
    CHECK_FALSE(watch.update(true, 1.0, t0 + 1500ms).behind);
    CHECK_FALSE(watch.update(true, 0.8, t0 + 2s).behind);
    CHECK_FALSE(watch.update(true, 0.8, t0 + 3500ms).behind);
    CHECK(watch.update(true, 0.8, t0 + 4s).behind);
}

TEST_CASE("pace: caught up only over 0.95") {
    PaceWatch watch;
    watch.update(true, 0.3, t0);
    CHECK(watch.update(true, 0.3, t0 + 2s).behind);
    CHECK(watch.update(true, 0.92, t0 + 3s).behind);  // over the bar, not caught up
    CHECK_FALSE(watch.update(true, 0.97, t0 + 4s).behind);
}

TEST_CASE("pace: not behind while not running, and the wait starts over") {
    PaceWatch watch;
    watch.update(true, 0.3, t0);
    CHECK(watch.update(true, 0.3, t0 + 2s).behind);
    CHECK_FALSE(watch.update(false, 0.0, t0 + 3s).behind);
    CHECK_FALSE(watch.update(true, 0.3, t0 + 4s).behind);
    CHECK(watch.update(true, 0.3, t0 + 6s).behind);
}

TEST_CASE("pace: the speed shown held 0.5 s") {
    PaceWatch watch;
    CHECK(watch.update(true, 3.41, t0).shown == 3.41);
    CHECK(watch.update(true, 3.38, t0 + 200ms).shown == 3.41);
    CHECK(watch.update(true, 3.39, t0 + 500ms).shown == 3.39);
    watch.update(false, 0.0, t0 + 600ms);
    CHECK(watch.update(true, 1.5, t0 + 700ms).shown == 1.5);  // at once after a stop
}

TEST_CASE("pace: a speed printed") {
    CHECK(SpeedText(0.244) == "0.24×");
    CHECK(SpeedText(3.4) == "3.40×");
    CHECK(SpeedText(42.3) == "42×");
}
