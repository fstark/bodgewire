#include "view/machine_keys.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("machine keys: on the bench at start", "[view]") {
    const MachineKeys keys;
    CHECK(keys.to() == KeysTo::Bench);
    CHECK_FALSE(keys.machine());
}

TEST_CASE("machine keys: a front click gives them to the Machine when something takes them",
          "[view]") {
    MachineKeys keys;
    keys.front();
    CHECK_FALSE(keys.machine());
    keys.setTakers(true);
    keys.front();
    CHECK(keys.machine());
    keys.front();
    CHECK(keys.machine());
}

TEST_CASE("machine keys: a bench click gives them back", "[view]") {
    MachineKeys keys;
    keys.setTakers(true);
    keys.front();
    keys.bench();
    CHECK(keys.to() == KeysTo::Bench);
    keys.bench();
    CHECK_FALSE(keys.machine());
}

TEST_CASE("machine keys: F2 toggles", "[view]") {
    MachineKeys keys;
    keys.toggle();
    CHECK_FALSE(keys.machine());  // nothing takes keys
    keys.setTakers(true);
    keys.toggle();
    CHECK(keys.machine());
    keys.toggle();
    CHECK_FALSE(keys.machine());
}

TEST_CASE("machine keys: the last taker put away gives them back", "[view]") {
    MachineKeys keys;
    keys.setTakers(true);
    keys.front();
    keys.setTakers(true);  // every frame
    CHECK(keys.machine());
    keys.setTakers(false);
    CHECK_FALSE(keys.machine());
    keys.setTakers(true);  // brought out again: still on the bench until a click
    CHECK_FALSE(keys.machine());
    keys.front();
    CHECK(keys.machine());
}
