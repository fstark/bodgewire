#include "machine/machine_list.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"

using namespace bw;

namespace {

namespace fs = std::filesystem;

void WriteFile(const fs::path& path, std::string_view text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

}  // namespace

TEST_CASE("machine list: a directory", "[machine]") {
    const fs::path root = fs::temp_directory_path() / "bw_machine_list_dir";
    fs::remove_all(root);
    WriteFile(root / "020-b/b.bwn", "");
    WriteFile(root / "010-a/a.bwn", "");
    WriteFile(root / "010-a/manuals/user.md",
              "# Alpha\n\n## What it does\n\nIts LED `D1` blinks.\nTwice.\n\nMore.\n");
    WriteFile(root / "010-a/puzzles/02-y.bwp", "machine a\ntitle \"Y\"\nbrief \"Why\"\n");
    WriteFile(root / "010-a/puzzles/01-x.bwp", "machine a\ntitle \"X\"\nbrief \"Ex\"\n");
    WriteFile(root / "010-a/puzzles/03-bad.bwp", "machine a\nnonsense 1\n");
    WriteFile(root / "010-a/puzzles/04-other.bwp", "machine b\ntitle \"O\"\n");
    WriteFile(root / "010-a/puzzles/notes.txt", "not a Puzzle");
    fs::create_directories(root / "030-c");
    WriteFile(root / "040-d/d.bwn", "");
    WriteFile(root / "040-d/e.bwn", "");
    WriteFile(root / "readme.bwn", "");  // a file, not a Machine's directory

    const MachineList list = ListMachines(root);
    REQUIRE(list.machines.size() == 2);
    const MachineEntry& a = list.machines[0];
    CHECK(a.dir.filename() == "010-a");
    CHECK(a.netlist.filename() == "a.bwn");
    CHECK(a.name == "Alpha");
    CHECK(a.line.find("D1") != std::string::npos);
    CHECK(a.line.find('`') == std::string::npos);
    CHECK(a.line.find("More") == std::string::npos);
    REQUIRE(a.puzzles.size() == 2);
    CHECK(a.puzzles[0].title == "X");
    CHECK(a.puzzles[0].brief == "Ex");
    CHECK(a.puzzles[1].title == "Y");
    CHECK(a.puzzles[1].file.filename() == "02-y.bwp");
    const MachineEntry& b = list.machines[1];
    CHECK(b.name == "020-b");
    CHECK(b.line.empty());
    CHECK(b.puzzles.empty());
    CHECK(list.problems.size() == 3);  // 03-bad, 04-other, 040-d

    CHECK(ListMachines(root / "nowhere").machines.empty());
    fs::remove_all(root);
}

TEST_CASE("machine list: the repository", "[machine]") {
    const MachineList list = ListMachines(test::RootDir() / "machines");
    CHECK(list.problems.empty());
    REQUIRE(list.machines.size() == 14);
    const MachineEntry& counter = list.machines.front();
    CHECK(counter.name == "Counter");
    REQUIRE(counter.puzzles.size() == 3);
    CHECK(counter.puzzles[0].title == "Dark LED");
    CHECK(counter.puzzles[1].title == "Frozen LED");
    CHECK(counter.puzzles[2].title == "LED too fast");
    const auto bw2001 = MachineIndexOf(list, test::NetlistPath("bw2001"));
    REQUIRE(bw2001);
    CHECK(list.machines[*bw2001].name == "BW-2001");
    CHECK(list.machines[*bw2001].puzzles.size() == 16);
    for (const MachineEntry& machine : list.machines) {
        INFO(machine.dir.filename().string());
        CHECK_FALSE(machine.line.empty());
        CHECK(machine.line.find('`') == std::string::npos);
        CHECK(machine.line.find("**") == std::string::npos);
        for (const PuzzleEntry& puzzle : machine.puzzles) {
            INFO(puzzle.file.filename().string());
            CHECK_FALSE(puzzle.title.empty());
            CHECK_FALSE(puzzle.brief.empty());
        }
    }
}

TEST_CASE("machine list: find machines", "[machine]") {
    const auto found = FindMachinesDir(test::RootDir() / "machines/010-counter/puzzles");
    REQUIRE(found);
    CHECK(*found == fs::weakly_canonical(test::RootDir() / "machines"));

    const fs::path temp = fs::temp_directory_path() / "bw_machine_list_find";
    fs::create_directories(temp / "deep");
    bool above = false;  // a machines/ above the temporary directory would be found
    for (fs::path dir = fs::weakly_canonical(temp);
         dir.has_parent_path() && dir.parent_path() != dir; dir = dir.parent_path()) {
        above = above || fs::exists(dir.parent_path() / "machines");
    }
    if (above) {
        WARN("a machines/ above " << temp.string() << ": skipped");
    } else {
        CHECK_FALSE(FindMachinesDir(temp / "deep"));
    }
    fs::remove_all(temp);
}

TEST_CASE("machine list: machine index", "[machine]") {
    const MachineList list = ListMachines(test::RootDir() / "machines");
    const auto bw2001 = MachineIndexOf(list, test::NetlistPath("bw2001"));
    REQUIRE(bw2001);
    CHECK(MachineIndexOf(list, test::PuzzlePath("odd_e")) == bw2001);
    CHECK(MachineIndexOf(list, fs::relative(test::PuzzlePath("odd_e"))) == bw2001);

    const auto puzzle = EntryOf(list, test::PuzzlePath("odd_e"));
    REQUIRE(puzzle);
    CHECK(puzzle->machine == *bw2001);
    CHECK(puzzle->puzzle == 0u);
    const auto netlist = EntryOf(list, test::NetlistPath("bw2001"));
    REQUIRE(netlist);
    CHECK(netlist->machine == *bw2001);
    CHECK_FALSE(netlist->puzzle);

    CHECK_FALSE(MachineIndexOf(list, test::RootDir() / "tests/data/bw1_bench.bwn"));
    CHECK_FALSE(EntryOf(list, test::RootDir() / "tests/data/bw1_bench.bwn"));
    CHECK_FALSE(EntryOf(list, test::RootDir() / "machines/130-bw2001/puzzles/99-none.bwp"));
}
