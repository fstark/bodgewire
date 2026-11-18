#include "manual/manual.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../paths.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

using Names = std::vector<std::string>;

std::unique_ptr<LoadedCircuit> Counter() {
    auto design = LoadNetlistFile(test::NetlistPath("counter"));
    REQUIRE(design.has_value());
    return std::move(*design);
}

Names Pins(std::string_view bullet) {
    const Markdown doc = ParseMarkdown(bullet);
    REQUIRE(doc.blocks.size() == 1);
    return BulletPins(doc.blocks[0]);
}

std::vector<std::string> Messages(const std::vector<Diagnostic>& problems) {
    std::vector<std::string> messages;
    for (const Diagnostic& d : problems) {
        CHECK(d.severity == Severity::Warning);
        messages.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return messages;
}

constexpr std::string_view kGoodService = "# Counter \u2014 Service Manual\n"
                                          "## Components\n"
                                          "- `X1` clock: 1 MHz\n"
                                          "- `J1` power\n"
                                          "- `R1` pull-up: holds RESET_N high\n"
                                          "- `R2` LED resistor\n"
                                          "- `U1` counter: bits 0-3\n"
                                          "- `U2` counter: bits 4-7\n"
                                          "- `U3` counter: bits 8-11\n"
                                          "- `U4` counter: bits 12-15\n"
                                          "- `U5` counter: bits 16-19\n"
                                          "- `D1` LED: lit while C19 is low\n";
constexpr std::string_view kServiceTail = "## Test points\nx\n## Troubleshooting\nx\n";

}  // namespace

TEST_CASE("manual: bullet pins and ranges", "[manual]") {
    CHECK(Pins("- `1A`\u2013`1D`: inputs") == Names{"1A", "1B", "1C", "1D"});
    CHECK(Pins("- `QA`\u2013`QD`: outputs") == Names{"QA", "QB", "QC", "QD"});
    CHECK(Pins("- `1A`\u2013`4A`: inputs") == Names{"1A", "2A", "3A", "4A"});
    CHECK(Pins("- `1Y` - `3Y`: outputs") == Names{"1Y", "2Y", "3Y"});
    CHECK(Pins("- `CLK`, `CLR_N`: clock and clear") == Names{"CLK", "CLR_N"});
    CHECK(Pins("- `1A`\u2013`2B`: odd") == Names{"1A", "2B"});
    CHECK(Pins("- `VCC`: +5 V, `GND` is not listed here") == Names{"VCC"});
}

TEST_CASE("manual: pin bullet", "[manual]") {
    const Markdown sheet = ParseMarkdown("# 74LS00 \u2014 quad NAND\n"
                                         "## Function\nNAND.\n"
                                         "## Pins\n"
                                         "- `1A`\u2013`4A`, `1B`\u2013`4B`: inputs\n"
                                         "- `1Y`\u2013`4Y`: outputs\n");
    const Block* bullet = PinBullet(sheet, "3B");
    REQUIRE(bullet != nullptr);
    CHECK(PlainText(bullet->spans).ends_with(": inputs"));
    CHECK(PinBullet(sheet, "2Y") != nullptr);
    CHECK(PinBullet(sheet, "VCC") == nullptr);
}

TEST_CASE("manual: component role", "[manual]") {
    const Markdown service = ParseMarkdown(kGoodService);
    const auto role = ComponentRole(service, "R1");
    REQUIRE(role.has_value());
    CHECK(role->name == "pull-up");
    CHECK(role->detail == "holds RESET_N high");
    const auto power = ComponentRole(service, "J1");
    REQUIRE(power.has_value());
    CHECK(power->name == "power");
    CHECK(power->detail.empty());
    CHECK_FALSE(ComponentRole(service, "U9"));
}

TEST_CASE("manual: links", "[manual]") {
    const auto design = Counter();
    const Markdown doc = ParseMarkdown("check `U1` pin 15 (RCO): a pulse; `QA` and `CLK`\n");
    const auto& spans = doc.blocks[0].spans;
    const auto link = LinkAt(spans, 1, *design, LinkScope::Machine);
    REQUIRE(link.has_value());
    CHECK(link->references == std::vector<std::string>{"U1"});
    CHECK(link->pin == "15");
    CHECK(link->pinChars == std::string(" pin 15").size());
    CHECK(spans[2].text.substr(link->pinChars) == " (RCO): a pulse; ");
    CHECK_FALSE(LinkAt(spans, 0, *design, LinkScope::Machine));
    CHECK_FALSE(LinkAt(spans, 3, *design, LinkScope::Machine));  // `QA`: a pin name alone
    // `CLK`: a Net in a Machine's manual, plain in a datasheet.
    const auto clk = LinkAt(spans, 5, *design, LinkScope::Machine);
    REQUIRE(clk.has_value());
    CHECK(clk->net == "CLK");
    CHECK(clk->references.empty());
    CHECK_FALSE(LinkAt(spans, 5, *design, LinkScope::Parts));
    CHECK(LinkAt(spans, 1, *design, LinkScope::Parts).has_value());  // parts in both
}

TEST_CASE("manual: runs and pins by name", "[manual]") {
    const auto design = Counter();
    const Markdown doc =
        ParseMarkdown("`U2`\u2013`U4` count; `U2.ENT` and `U9.ENT`, `R1.X`, `UH1.bin`, `C3`\n");
    const auto& spans = doc.blocks[0].spans;
    const auto run = LinkAt(spans, 0, *design, LinkScope::Machine);
    REQUIRE(run.has_value());
    CHECK(run->references == std::vector<std::string>{"U2", "U3", "U4"});
    CHECK(run->spansUsed == 3);
    // In a datasheet `U2` alone is a part; the run is not one there.
    const auto alone = LinkAt(spans, 0, *design, LinkScope::Parts);
    REQUIRE(alone.has_value());
    CHECK(alone->spansUsed == 1);

    const auto ent = LinkAt(spans, 4, *design, LinkScope::Machine);
    REQUIRE(ent.has_value());
    CHECK(ent->references == std::vector<std::string>{"U2"});
    CHECK(ent->pin == "10");  // ENT
    CHECK_FALSE(LinkAt(spans, 4, *design, LinkScope::Parts));
    CHECK_FALSE(LinkAt(spans, 6, *design, LinkScope::Machine));  // U9: no such part
    CHECK_FALSE(LinkAt(spans, 8, *design, LinkScope::Machine));  // a resistor: no pin names
    const auto net = LinkAt(spans, 12, *design, LinkScope::Machine);
    REQUIRE(net.has_value());
    CHECK(net->net == "C3");
}

TEST_CASE("manual: run and pin name checks", "[manual]") {
    const auto design = Counter();
    CHECK(Messages(CheckUserManual(
              ParseMarkdown("# Counter\n## What it does\n"
                            "`U5`\u2013`U1`; `U1`\u2013`R2`; `C3`\u2013`C1`; `R1`\u2013`C3`\n"
                            "`U2.ENX`, `U9.ENT`, `UH1.bin`, `0.5`, `U2.ENT`\n"
                            "## How it works\n"),
              "u.md", *design)) ==
          std::vector<std::string>{"3: bad run U5\u2013U1", "3: bad run U1\u2013R2",
                                   "3: U2 has no pin ENX", "3: unknown reference 'U9'"});
}

TEST_CASE("manual: optional sections", "[manual]") {
    const auto design = Counter();
    const auto user = [&](std::string_view sections) {
        return Messages(
            CheckUserManual(ParseMarkdown("# Counter\n" + std::string(sections)), "u.md", *design));
    };
    CHECK(user("## What it does\n").front() ==
          "2: sections must be: What it does[, Using it], How it works");
    CHECK(user("## What it does\n## How it works\n").empty());
    CHECK(user("## What it does\n## Using it\n## How it works\n").empty());
    CHECK(user("## What it does\n## How it works\n## Using it\n").front() ==
          "2: sections must be: What it does[, Using it], How it works");
    CHECK(user("## What it does\n## How it works\n## How it works\n").size() == 1);
    CHECK(user("## How it works\n").size() == 1);
    const auto service = [&](std::string_view sections) {
        return Messages(CheckServiceManual(
            ParseMarkdown(std::string(kGoodService) + std::string(sections)), "s.md", *design));
    };
    CHECK(service(kServiceTail).empty());
    CHECK(service("## Troubleshooting\n").front() ==
          "2: sections must be: Components, Test points, Troubleshooting");
    CHECK(service("## Test points\n").size() == 1);
}

TEST_CASE("manual: part pin names", "[manual]") {
    const Names ls161 = PartPinNames(*FindPart("74LS161"));
    CHECK(ls161.size() == 16);
    CHECK(std::ranges::find(ls161, "CLR_N") != ls161.end());
    CHECK(PartPinNames(*FindPart("OSC")).size() == 14);
    CHECK(PartPinNames(*FindPart("PADDLE")).size() == 4);
    CHECK(PartPinNames(*FindPart("POR")).size() == 8);
    CHECK(PartPinNames(*FindPart("KEYBOARD")).size() == 10);
    const Names riot = PartPinNames(*FindPart("6530"));
    CHECK(riot.size() == 40);
    CHECK(std::ranges::find(riot, "CS1") != riot.end());
    CHECK(PartPinNames(*FindPart("R")).empty());
}

TEST_CASE("manual: checks", "[manual]") {
    const auto design = Counter();
    CHECK(CheckServiceManual(ParseMarkdown(std::string(kGoodService) + std::string(kServiceTail)),
                             "s.md", *design)
              .empty());
    CHECK(Messages(CheckServiceManual(ParseMarkdown("# S\n## How it works\n## Components\n"),
                                      "s.md", *design))
              .front() == "2: sections must be: Components, Test points, Troubleshooting");

    std::string service(kGoodService);
    const std::string u5 = "- `U5` counter: bits 16-19\n";
    service.replace(service.find(u5), u5.size(), "- `U3` again\n- `U9` x\n");
    service += "- `U1` pin 17 floats\n";
    service += kServiceTail;
    CHECK(Messages(CheckServiceManual(ParseMarkdown(service), "s.md", *design)) ==
          std::vector<std::string>{"12: unknown reference 'U9'", "14: U1 has no pin 17",
                                   "11: Components: U3 listed twice",
                                   "14: Components: U1 listed twice", "2: Components: missing U5"});
    CHECK(Messages(CheckServiceManual(
                       ParseMarkdown("# S\n## Components\nx\n" + std::string(kServiceTail)), "s.md",
                       *design))
              .front() == "3: Components: each bullet starts with a `REF`");

    CHECK(CheckUserManual(
              ParseMarkdown("# Counter\n## What it does\nConnect +5 V to `J1`.\n## How it works\n"),
              "u.md", *design)
              .empty());
    CHECK(Messages(CheckUserManual(
                       ParseMarkdown("# Counter\n## What it does\nCounts.\n## Connections\n- "
                                     "`J1`: power\n"),
                       "u.md", *design))
              .front() == "2: sections must be: What it does[, Using it], How it works");
    CHECK(Messages(CheckUserManual(ParseMarkdown("## What it does\nSee `J7`.\n## How it works\n"),
                                   "u.md", *design)) ==
          std::vector<std::string>{"1: must start with a # title", "2: unknown reference 'J7'"});

    const PartInfo& ls00 = *FindPart("74LS00");
    const std::string sheet =
        "# 74LS00 \u2014 quad 2-input NAND\n## Function\nNAND.\n## Pins\n"
        "- `1A`\u2013`4A`, `1B`\u2013`4B`: inputs\n- `1Y`\u2013`4Y`: outputs\n";
    CHECK(Messages(CheckDatasheet(ParseMarkdown(sheet), "d.md", ls00)) ==
          std::vector<std::string>{"0: Pins: missing GND", "0: Pins: missing VCC"});
    CHECK(CheckDatasheet(ParseMarkdown(sheet + "- `VCC`, `GND`: power\n"), "d.md", ls00).empty());
    CHECK(Messages(CheckDatasheet(ParseMarkdown("# 74LS00X\n## Pins\n## Function\n"), "d.md", ls00))
              .front() == "2: sections must be: Function, Pins");
}

TEST_CASE("manual: references in table cells", "[manual]") {
    const auto design = Counter();
    CHECK(Messages(CheckUserManual(ParseMarkdown("# Counter\n## What it does\n"
                                                 "| Probe | Reads |\n|---|---|\n"
                                                 "| `U1` pin 2 | pulsing |\n"
                                                 "| `U7` | `U1` pin 30 |\n"
                                                 "## How it works\n"),
                                   "u.md", *design)) ==
          std::vector<std::string>{"3: unknown reference 'U7'", "3: U1 has no pin 30"});
}

TEST_CASE("manual: load with missing files", "[manual]") {
    const auto design = Counter();
    const auto root = std::filesystem::temp_directory_path() / "bw_manuals_empty";
    std::filesystem::create_directories(root);
    const LoadedManuals loaded = LoadManuals(root, root / "datasheets", *design);
    CHECK(loaded.problems.empty());
    CHECK_FALSE(loaded.manuals.user);
    CHECK_FALSE(loaded.manuals.service);
    CHECK(loaded.manuals.datasheets.empty());
    std::filesystem::remove_all(root);
}

TEST_CASE("manual: datasheet index", "[manual]") {
    const auto design = Counter();
    const std::vector<IndexEntry> index = DatasheetIndex(*design);
    Names parts;
    for (const IndexEntry& entry : index) {
        parts.push_back(entry.part);
    }
    CHECK(parts == Names{"OSC", "POWER", "R", "74LS161", "LED"});
    CHECK(index[2].references == Names{"R1", "R2"});
    CHECK(index[3].references == Names{"U1", "U2", "U3", "U4", "U5"});
    CHECK(index[4].references == Names{"D1"});
}
