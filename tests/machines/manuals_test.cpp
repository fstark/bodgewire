#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <catch2/catch_test_macros.hpp>

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
    for (const std::string machine : {"counter", "test_pattern", "chaser", "dice", "stopwatch",
                                      "ball", "knob", "tennis", "message"}) {
        INFO(machine);
        auto design = LoadNetlistFile(kRoot / "netlists" / (machine + ".bwn"));
        REQUIRE(design.has_value());
        const auto dir = std::filesystem::path("manuals") / machine;
        CheckClean(CheckUserManual(ParseMarkdown(Read(kRoot / dir / "user.md")),
                                   (dir / "user.md").string(), **design));
        CheckClean(CheckServiceManual(ParseMarkdown(Read(kRoot / dir / "service.md")),
                                      (dir / "service.md").string(), **design));

        const LoadedManuals loaded = LoadManuals(kRoot, machine, **design);
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
    }
}
