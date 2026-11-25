#include "view/bench_options.hpp"

#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

std::expected<Options, std::string> Parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list(args);
    return ParseOptions(list);
}

}  // namespace

TEST_CASE("bench options: no file opens the launcher", "[view]") {
    const auto none = Parse({});
    REQUIRE(none.has_value());
    CHECK_FALSE(none->file.has_value());
    const auto launcher = Parse({"--machines", "dir", "--screenshot", "a.png", "--after", "2s"});
    REQUIRE(launcher.has_value());
    CHECK(launcher->machines == "dir");
    CHECK(launcher->after == std::chrono::seconds(2));
}

TEST_CASE("bench options: a file's bench flags", "[view]") {
    const auto options =
        Parse({"m.bwp",   "--probe",         "U1.3",     "--hook",   "U1.1,U2.2", "--focus",
               "scope",   "--pop",           "tv,meter", "--run",    "max",       "--side",
               "back",    "--binder-height", "120%",     "--brief",  "10:-5",     "--contents",
               "U3:code", "--debug",         "--manual", "datasheet"});
    REQUIRE(options.has_value());
    CHECK(options->file == "m.bwp");
    const BenchOptions& bench = options->bench;
    CHECK(bench.probe == "U1.3");
    CHECK(bench.hook == std::vector<std::string>{"U1.1", "U2.2"});
    CHECK(bench.focus == Owner::Scope);
    CHECK(bench.pop == std::vector{BenchObject::Tv, BenchObject::Meter});
    CHECK(bench.run == RunSwitch::Max);
    CHECK(bench.back);
    CHECK(bench.binderHeight == 1.2f);
    CHECK(bench.brief == std::pair{10.0f, -5.0f});
    REQUIRE(bench.contents.has_value());
    CHECK(bench.contents->first == "U3");
    CHECK(bench.contents->second == ContentsView::Code);
    CHECK(bench.debug);
    CHECK(bench.manual == Divider::Datasheets);
}

TEST_CASE("bench options: usage errors", "[view]") {
    CHECK(Parse({"--probe", "U1.3"}).error().empty());            // a bench flag, no file
    CHECK(Parse({"--after", "1s"}).error().empty());              // no --screenshot
    CHECK(Parse({"m.bwp", "--nope"}).error().empty());            // unknown flag
    CHECK(Parse({"m.bwp", "--probe"}).error().empty());           // no value
    CHECK(Parse({"m.bwp", "--away", "manual"}).error().empty());  // not a slot object
    CHECK(Parse({"m.bwp", "--binder-height", "300%"}).error().empty());
    CHECK(Parse({"m.bwp", "--hook", "A1.1,A1.2,A1.3,A1.4,A1.5"}).error().empty());
    CHECK(Parse({"m.bwp", "--leave", "--reset"}).error().empty());
    CHECK_FALSE(Parse({"m.bwp", "--type", "J1"}).error().empty());  // ParseTyping says why
}

TEST_CASE("bench options: the usage lists every flag", "[view]") {
    const std::string usage = Usage();
    CHECK(usage.starts_with("usage: bw_ui [file.bwn|file.bwp|random.bwr] [--machines DIR]"));
    CHECK(usage.contains("[--seed N]"));
    CHECK(usage.contains("[--manual user|service|datasheet|repairs|giveup]"));
    CHECK(usage.contains("[--show-fault]"));
    CHECK(usage.contains("[--probe REF.PIN]"));
    CHECK(usage.contains("[--debug]"));
    CHECK(usage.ends_with("[--screenshot OUT.png] [--after 1s]"));
}

TEST_CASE("bench options: --seed", "[view]") {
    // The seed of the Random fault made for a Vetting Script (P8d): the file, or a visited one.
    const auto bench = Parse({"machines/010-counter/puzzles/random.bwr", "--seed", "7"});
    REQUIRE(bench.has_value());
    CHECK(bench->seed == 7u);
    CHECK_FALSE(Parse({"machines/010-counter/puzzles/random.bwr"})->seed.has_value());
    const auto visit =
        Parse({"--visit", "a.bwp,m/puzzles/random.bwr", "--seed", "3", "--screenshot", "a.png"});
    REQUIRE(visit.has_value());
    CHECK(visit->seed == 3u);
}

TEST_CASE("bench options: --seed without a .bwr", "[view]") {
    CHECK(Parse({"m.bwp", "--seed", "7"}).error().empty());
    CHECK(Parse({"m.bwn", "--seed", "7"}).error().empty());
    CHECK(Parse({"--seed", "7"}).error().empty());
    CHECK(Parse({"--visit", "a.bwp", "--seed", "3", "--screenshot", "a.png"}).error().empty());
    CHECK(Parse({"random.bwr", "--seed", "seven"}).error().empty());
    CHECK(Parse({"random.bwr", "--seed", "-1"}).error().empty());
    CHECK(Parse({"random.bwr", "--seed"}).error().empty());
}

TEST_CASE("bench options: --show-fault", "[view]") {
    const auto bench = Parse({"random.bwr", "--manual", "giveup", "--show-fault"});
    REQUIRE(bench.has_value());
    CHECK(bench->bench.showFault);
    CHECK(bench->bench.manual == Divider::GiveUp);
    CHECK_FALSE(Parse({"random.bwr"})->bench.showFault);
    // Only a Random fault's bench has the page.
    CHECK(Parse({"m.bwp", "--show-fault"}).error().empty());
    CHECK(Parse({"m.bwn", "--show-fault"}).error().empty());
}

TEST_CASE("bench options: --manual giveup", "[view]") {
    CHECK(Parse({"random.bwr", "--manual", "giveup"})->bench.manual == Divider::GiveUp);
    CHECK(Parse({"m.bwp", "--manual", "giveup"}).error().empty());
    CHECK(Parse({"m.bwp", "--manual", "repairs"})->bench.manual == Divider::Repairs);
}

TEST_CASE("bench options: --make", "[view]") {
    // The Launcher's making started at once, for the screenshot of a waiting line (P8d).
    const auto make = Parse(
        {"--select", "m/puzzles/random.bwr", "--make", "--screenshot", "a.png", "--after", "0.5s"});
    REQUIRE(make.has_value());
    CHECK(make->make);
    CHECK_FALSE(make->seed.has_value());
    const auto seeded = Parse({"--select", "m/puzzles/random.bwr", "--make", "--seed", "4"});
    REQUIRE(seeded.has_value());
    CHECK(seeded->seed == 4u);
    CHECK_FALSE(Parse({})->make);
}

TEST_CASE("bench options: --make without --select", "[view]") {
    CHECK(Parse({"--make"}).error().empty());
    CHECK(Parse({"--select", "m/puzzles/01-x.bwp", "--make"}).error().empty());  // not a .bwr
    CHECK(Parse({"m/puzzles/random.bwr", "--make"}).error().empty());            // a bench
    CHECK(Parse({"--select", "m/puzzles/random.bwr", "--seed", "4"}).error().empty());
}

TEST_CASE("bench options: --progress", "[view]") {
    // The player's saved progress in a file of the agent's (P8e), with or without a bench.
    CHECK(Parse({"--progress", "/tmp/p.txt"})->progress == std::filesystem::path("/tmp/p.txt"));
    CHECK(Parse({"m.bwp", "--progress", "/tmp/p.txt"})->progress ==
          std::filesystem::path("/tmp/p.txt"));
    CHECK_FALSE(Parse({})->progress.has_value());
    CHECK(Parse({"--progress"}).error().empty());
}

TEST_CASE("bench options: --forget", "[view]") {
    // The Launcher's forget question open at start (P8e): the launcher only.
    CHECK(Parse({"--forget", "--screenshot", "a.png"})->forget);
    CHECK_FALSE(Parse({})->forget);
    CHECK(Parse({"m.bwp", "--forget"}).error().empty());
    CHECK(Parse({"--visit", "m.bwp", "--forget", "--screenshot", "a.png"}).error().empty());
}
