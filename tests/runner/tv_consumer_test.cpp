#include "runner/tv_consumer.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr NetId kVideoN{0}, kHdrive{1}, kVdrive{2};
constexpr Time kLine = 64_us;
constexpr Time kFrame = 260 * kLine;

// PET-like drives for [from, end) (line starts), and a lit dot 40 us into line 100 of each frame.
void Feed(TvConsumer& tv, Time from, Time end) {
    if (from == 0) {
        tv.sample(kVideoN, {0, One});
        tv.sample(kHdrive, {0, Zero});
        tv.sample(kVdrive, {0, One});
    }
    for (Time k = from / kLine; k * kLine < end; ++k) {
        const Time t = k * kLine;
        if (k % 260 == 0) {
            tv.sample(kVdrive, {t, Zero});
        } else if (k % 260 == 20) {
            tv.sample(kVdrive, {t, One});
        }
        tv.sample(kHdrive, {t + 24_us, One});
        if (k % 260 == 100) {
            tv.sample(kVideoN, {t + 40_us, Zero});
            tv.sample(kVideoN, {t + 40_us + 125'000, One});
        }
        tv.sample(kHdrive, {t + 48_us, Zero});
    }
    tv.advanceTo(end);
}

}  // namespace

TEST_CASE("tv consumer: a MONITOR's video is low for a lit dot, on separate drives", "[runner]") {
    TvConsumer tv(SeparateDrives{kVideoN, kHdrive, kVdrive});
    CHECK(tv.nets().size() == 3);
    for (int run = 0; run < 2; ++run) {  // the mode survives power-off and on
        INFO("run " << run);
        tv.powerOn();
        Feed(tv, 0, 3 * kFrame + kLine);
        tv.pictures().update();
        const TvPicture& picture = tv.pictures().front();
        CHECK(picture.locked);
        CHECK(picture.frame.at(101, 128) > 0.99f);
        CHECK(picture.frame.at(101, 127) == 0.0f);
        CHECK(picture.frame.at(50, 128) == 0.0f);
        tv.powerOff();
    }
}
