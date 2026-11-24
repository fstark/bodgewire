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

namespace {

// `ratio`× for `seconds`, the headroom updated every 100 ms from `t`: simulated time, run ns.
struct HeadroomRun {
    Headroom headroom;
    PaceWatch::Clock::time_point t = t0;
    Time sim = 0;
    std::int64_t runNs = 0;

    void run(double ratio, int tenths) {
        for (int k = 0; k < tenths; ++k) {
            t += 100ms;
            runNs += 10'000'000;  // 10 ms of work per 100 ms
            sim += static_cast<Time>(ratio * 10e-3 * static_cast<double>(kPicosPerSecond));
            headroom.update(sim, runNs, t);
        }
    }
};

}  // namespace

TEST_CASE("headroom: simulated time over the time spent running, after most of a second") {
    HeadroomRun run;
    run.run(13.0, 8);
    CHECK_FALSE(run.headroom.shown());
    run.run(13.0, 10);
    REQUIRE(run.headroom.shown());
    CHECK(*run.headroom.shown() > 12.9);
    CHECK(*run.headroom.shown() < 13.1);
    CHECK(run.headroom.band() == Playability::Playable);
    CHECK(SpeedText(*run.headroom.shown()) == "13×");
}

TEST_CASE("headroom: orange under 0.9, gold again only over 1") {
    HeadroomRun run;
    run.run(0.95, 20);
    CHECK(run.headroom.band() == Playability::Playable);
    run.run(0.85, 20);
    CHECK(run.headroom.band() == Playability::Slow);
    run.run(0.98, 20);
    CHECK(run.headroom.band() == Playability::Slow);  // at the limit: no flicker
    run.run(1.05, 20);
    CHECK(run.headroom.band() == Playability::Playable);
}

TEST_CASE("headroom: red under 0.27, orange again only over 0.3") {
    HeadroomRun run;
    run.run(0.28, 20);
    CHECK(run.headroom.band() == Playability::Slow);
    run.run(0.2, 20);
    CHECK(run.headroom.band() == Playability::TooSlow);
    run.run(0.29, 20);
    CHECK(run.headroom.band() == Playability::TooSlow);
    run.run(0.5, 20);
    CHECK(run.headroom.band() == Playability::Slow);
    run.run(0.1, 20);
    run.run(2.0, 20);
    CHECK(run.headroom.band() == Playability::Playable);
}

TEST_CASE("headroom: powered on again, measured anew") {
    HeadroomRun run;
    run.run(0.1, 20);
    CHECK(run.headroom.band() == Playability::TooSlow);
    run.sim = 0;
    run.headroom.update(0, run.runNs, run.t);
    CHECK_FALSE(run.headroom.shown());
    CHECK(run.headroom.band() == Playability::Playable);
}

TEST_CASE("headroom: its sentence, full speed or a share of the real one's") {
    CHECK(HeadroomText(13.2, Playability::Playable) ==
          "On this computer it runs at full speed: it could go 13× as fast as the real one.");
    CHECK(HeadroomText(0.344, Playability::TooSlow) ==
          "On this computer it runs at 34% of the real one's speed.");
    CHECK(HeadroomText(1.01, Playability::Slow) ==
          "On this computer it runs at 99% of the real one's speed.");  // the band not yet back
}

TEST_CASE("headroom: the number held 5 s, the band followed at once") {
    HeadroomRun run;
    run.run(13.0, 10);
    REQUIRE(run.headroom.shown());
    CHECK(SpeedText(*run.headroom.shown()) == "13×");
    run.run(20.0, 30);
    CHECK(SpeedText(*run.headroom.shown()) == "13×");  // held
    run.run(20.0, 20);
    CHECK(SpeedText(*run.headroom.shown()) == "20×");  // the last 5 s's
    run.run(0.5, 9);
    CHECK(run.headroom.band() == Playability::Playable);  // the last second's: not yet
    CHECK(SpeedText(*run.headroom.shown()) == "20×");
    run.run(0.5, 1);
    CHECK(run.headroom.band() == Playability::Slow);
    REQUIRE(run.headroom.shown());
    CHECK(*run.headroom.shown() < 1.0);  // anew with the band: its sentence agrees with the bar
}
