#include "view/repair_ack.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("repair ack: no tick before 3 s, then the tick and the card once", "[view]") {
    RepairAck ack(true);
    ack.update(true, true, 1, 10.0);
    ack.update(true, true, 1, 12.9);
    CHECK_FALSE(ack.ticked());
    CHECK_FALSE(ack.takeCard());
    ack.update(true, true, 1, 13.0);
    CHECK(ack.ticked());
    CHECK(ack.takeCard());
    CHECK_FALSE(ack.takeCard());
    ack.update(true, true, 1, 20.0);
    CHECK(ack.ticked());
    CHECK_FALSE(ack.takeCard());
}

TEST_CASE("repair ack: the working Machine never", "[view]") {
    RepairAck ack(false);
    ack.update(true, true, 1, 0.0);
    ack.update(true, true, 1, 10.0);
    CHECK_FALSE(ack.ticked());
    CHECK_FALSE(ack.takeCard());
}

TEST_CASE("repair ack: not completed, never", "[view]") {
    RepairAck ack(true);
    ack.update(false, true, 1, 0.0);
    ack.update(false, true, 1, 10.0);
    CHECK_FALSE(ack.ticked());
    CHECK_FALSE(ack.takeCard());
}

TEST_CASE("repair ack: a power-on restarts the count", "[view]") {
    RepairAck ack(true);
    ack.update(true, true, 1, 0.0);
    ack.update(true, true, 2, 2.5);  // power-cycled with no unpowered frame
    ack.update(true, true, 2, 4.0);
    CHECK_FALSE(ack.ticked());
    ack.update(true, true, 2, 5.5);
    CHECK(ack.ticked());
}

TEST_CASE("repair ack: power off before 3 s, no tick", "[view]") {
    RepairAck ack(true);
    ack.update(true, true, 1, 0.0);
    ack.update(true, false, 1, 2.0);
    ack.update(true, false, 1, 10.0);
    CHECK_FALSE(ack.ticked());
    ack.update(true, true, 2, 11.0);
    ack.update(true, true, 2, 14.0);
    CHECK(ack.ticked());
}

TEST_CASE("repair ack: the tick follows the verdict, the card does not come back", "[view]") {
    RepairAck ack(true);
    ack.update(true, true, 1, 0.0);
    ack.update(true, true, 1, 3.0);
    REQUIRE(ack.ticked());
    REQUIRE(ack.takeCard());
    ack.update(true, false, 1, 4.0);
    ack.update(false, false, 1, 5.0);  // a Repair broke it
    CHECK_FALSE(ack.ticked());
    ack.update(true, false, 1, 6.0);  // repaired again: not before a run
    CHECK_FALSE(ack.ticked());
    ack.update(true, true, 2, 7.0);
    ack.update(true, true, 2, 10.0);
    CHECK(ack.ticked());
    CHECK_FALSE(ack.takeCard());
}

TEST_CASE("repair ack: the tick stays with the power off", "[view]") {
    RepairAck ack(true);
    ack.update(true, true, 1, 0.0);
    ack.update(true, true, 1, 3.0);
    ack.update(true, false, 1, 4.0);
    CHECK(ack.ticked());
}
