#include "view/pin_marks.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "sim/circuit.hpp"
#include "sim/component.hpp"

using namespace bw;

namespace {

// A Part's pin names and kinds in pin order, from a sample built as PartPinNames builds it.
struct Pins {
    std::vector<std::string> names;
    std::vector<std::optional<PinKind>> kinds;

    std::size_t at(std::string_view name) const {
        const auto it = std::ranges::find(names, name);
        REQUIRE(it != names.end());
        return static_cast<std::size_t>(it - names.begin());
    }
    // The names of what `name` works with, sorted.
    std::vector<std::string> with(std::string_view name) const {
        std::vector<std::string> out;
        for (const std::size_t i : WorksWith(names, kinds, at(name))) {
            out.push_back(names[i]);
        }
        std::ranges::sort(out);
        return out;
    }
};

Pins PinsOf(std::string_view part) {
    const PartInfo* info = FindPart(part);
    REQUIRE(info != nullptr);
    Circuit circuit;
    const auto component = info->create(circuit, "X", info->sample);
    REQUIRE(component.has_value());
    Pins pins;
    for (const PinSpec& spec : (*component)->pins()) {
        pins.names.push_back(spec.name);
        pins.kinds.emplace_back(spec.kind);
    }
    return pins;
}

std::vector<std::string> Sorted(std::vector<std::string> names) {
    std::ranges::sort(names);
    return names;
}

}  // namespace

TEST_CASE("pin marks: looks", "[view]") {
    CHECK(LookOf("A", PinKind::Input, true).mark == PinMark::In);
    CHECK(LookOf("Y", PinKind::Output, true).mark == PinMark::Out);
    CHECK(LookOf("Y", PinKind::OpenCollector, true).mark == PinMark::OpenCollector);
    CHECK(LookOf("D0", PinKind::Bidirectional, true).mark == PinMark::Both);
    CHECK(LookOf("PA0", PinKind::PullUp, true).mark == PinMark::Both);  // a port
    CHECK(LookOf("VCC", PinKind::Power, true).mark == PinMark::Power);
    CHECK(LookOf("NC1", PinKind::NoConnect, true).mark == PinMark::NoConnect);
    CHECK(LookOf("1", std::nullopt, false).mark == PinMark::Passive);  // a resistor
    // Not a chip: a switch's pins are Outputs, a connector's Inputs; both drawn plain.
    CHECK(LookOf("1", PinKind::Output, false).mark == PinMark::Passive);
    CHECK(LookOf("DATA", PinKind::Input, false).mark == PinMark::Passive);
    CHECK(LookOf("GND", PinKind::Input, false).mark == PinMark::Power);
    CHECK(LookOf("GND4", PinKind::Input, false).mark == PinMark::Power);

    CHECK(LookOf("CLR_N", PinKind::Input, true).activeLow);
    CHECK_FALSE(LookOf("CLK", PinKind::Input, true).activeLow);
    for (const char* clock : {"CLK", "CLK1", "1CLK", "CKA", "CKB", "PHI", "PHI0", "PHI2"}) {
        INFO(clock);
        CHECK(LookOf(clock, PinKind::Input, true).clock);
    }
    for (const char* other : {"CA1", "CB1", "LE", "OUT", "PHIX", "PHI12", "CLKX", "CK"}) {
        INFO(other);
        CHECK_FALSE(LookOf(other, PinKind::Input, true).clock);
    }
}

TEST_CASE("pin marks: gates", "[view]") {
    const Pins pins = PinsOf("74LS00");
    CHECK(pins.with("1Y") == Sorted({"1A", "1B", "1Y"}));
    CHECK(pins.with("3A") == Sorted({"3A", "3B", "3Y"}));
    CHECK(pins.with("VCC").empty());
    CHECK(pins.with("GND").empty());
}

TEST_CASE("pin marks: shared pins", "[view]") {
    const Pins ls273 = PinsOf("74LS273");
    CHECK(ls273.with("3Q") == Sorted({"3D", "3Q", "CLK", "CLR_N"}));
    std::vector<std::string> sections;
    for (int n = 1; n <= 8; ++n) {
        sections.push_back(std::to_string(n) + "D");
        sections.push_back(std::to_string(n) + "Q");
    }
    std::vector<std::string> clock = sections;
    clock.push_back("CLK");
    CHECK(ls273.with("CLK") == Sorted(clock));

    CHECK(PinsOf("74LS157").with("2Y") == Sorted({"2A", "2B", "2Y", "S", "G_N"}));
    CHECK(PinsOf("74LS74").with("1Q") == Sorted({"1D", "1CLK", "1PRE_N", "1CLR_N", "1Q", "1Q_N"}));
    CHECK(PinsOf("74LS244").with("1Y2") ==
          Sorted({"1A1", "1A2", "1A3", "1A4", "1Y1", "1Y2", "1Y3", "1Y4", "1OE_N"}));
    const Pins ls20 = PinsOf("74LS20");
    CHECK(ls20.with("NC3").empty());
    const auto y = ls20.with("1Y");
    CHECK(std::ranges::find(y, "NC3") == y.end());  // NC is no shared pin
}

TEST_CASE("pin marks: no sections", "[view]") {
    const Pins ls161 = PinsOf("74LS161");
    CHECK(ls161.with("QA").empty());
    CHECK(ls161.with("CLK").empty());
    const std::vector<std::string> resistor{"1", "2"};
    const std::vector<std::optional<PinKind>> none(2);
    CHECK(WorksWith(resistor, none, 0).empty());
    const std::vector<std::optional<PinKind>> button{PinKind::Output, PinKind::Output};
    CHECK(WorksWith(resistor, button, 0).empty());  // digits alone are no section
}
