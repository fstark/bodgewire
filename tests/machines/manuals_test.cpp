#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "manual/manual.hpp"
#include "netlist/diagnostic.hpp"

using namespace bw;

namespace {

const std::filesystem::path kRoot(BW_ROOT_DIR);

std::string Read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void CheckClean(const std::vector<Diagnostic>& problems) {
    for (const Diagnostic& d : problems) {
        FAIL_CHECK(Format(d));
    }
}

}  // namespace

TEST_CASE("manuals: every catalogue Part has a Datasheet", "[machines]") {
    for (const PartInfo& part : AllParts()) {
        const std::string file = "datasheets/" + std::string(part.name) + ".md";
        INFO(file);
        REQUIRE(std::filesystem::exists(kRoot / file));
        CheckClean(CheckDatasheet(ParseMarkdown(Read(kRoot / file)), file, part));
    }
}

TEST_CASE("manuals: every Machine's manuals pass their checks", "[machines]") {
    for (const std::string machine :
         {"counter", "test_pattern", "chaser", "dice", "stopwatch", "ball", "knob", "tennis",
          "message", "typewriter", "heartbeat", "led_computer", "bw1", "bw2001", "bw80"}) {
        INFO(machine);
        auto design = LoadNetlistFile(test::NetlistPath(machine));
        REQUIRE(design.has_value());
        const auto dir = test::MachineDir(machine);
        CheckClean(CheckUserManual(ParseMarkdown(Read(dir / "manuals" / "user.md")),
                                   "manuals/user.md", **design));
        CheckClean(CheckServiceManual(ParseMarkdown(Read(dir / "manuals" / "service.md")),
                                      "manuals/service.md", **design));

        const LoadedManuals loaded = LoadManuals(dir, kRoot / "datasheets", **design);
        CheckClean(loaded.problems);
        CHECK(loaded.manuals.user.has_value());
        CHECK(loaded.manuals.service.has_value());
        for (const LoadedPart& part : (*design)->parts) {
            CHECK(loaded.manuals.datasheets.contains(part.info->name));
        }
        for (const LoadedPart& part : (*design)->parts) {
            INFO(part.reference);
            CHECK(ComponentRole(*loaded.manuals.service, part.reference).has_value());
        }

        // The checks require the sections; each must also say something.
        const Markdown user = ParseMarkdown(Read(dir / "manuals" / "user.md"));
        const Markdown service = ParseMarkdown(Read(dir / "manuals" / "service.md"));
        for (const char* heading : {"What it does", "How it works"}) {
            INFO(heading);
            CHECK_FALSE(Section(user, heading).empty());
        }
        for (const char* heading : {"Components", "Test points", "Troubleshooting"}) {
            INFO(heading);
            CHECK_FALSE(Section(service, heading).empty());
        }
        const bool usingIt = std::ranges::any_of(user.blocks, [](const Block& block) {
            return block.kind == BlockKind::Heading && PlainText(block.spans) == "Using it";
        });
        if (usingIt) {
            CHECK_FALSE(Section(user, "Using it").empty());
        }
    }
}
