#include "view/bench_options.hpp"

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
    CHECK(usage.starts_with("usage: bw_ui [file.bwn|file.bwp] [--machines DIR]"));
    CHECK(usage.contains("[--probe REF.PIN]"));
    CHECK(usage.contains("[--debug]"));
    CHECK(usage.ends_with("[--screenshot OUT.png] [--after 1s]"));
}
