#include "machine/progress.hpp"

#include <filesystem>
#include <functional>
#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../temp_files.hpp"
#include "machine/machine_list.hpp"

using namespace bw;

namespace {

namespace fs = std::filesystem;

using test::WriteFile;

}  // namespace

TEST_CASE("progress: text", "[machine]") {
    Progress progress;
    progress.add("020-b/y");
    progress.add("010-a/x");
    CHECK(progress.text() == "# Bodgewire progress\n010-a/x\n020-b/y\n");
    CHECK(Progress::Parse(progress.text()).keys() == progress.keys());
    const Progress parsed = Progress::Parse("\n# a comment\n  020-b/y \r\n010-a/x\n\n020-b/y");
    CHECK(parsed.keys() == progress.keys());
    CHECK(Progress::Parse("").empty());
    CHECK(Progress::Parse("# Bodgewire progress\n").empty());
}

TEST_CASE("progress: a key saved until 2026-11-28", "[machine]") {
    // The path under machines/, whatever the Puzzle's number then or now.
    const Progress parsed =
        Progress::Parse("# Bodgewire progress\n010-counter/puzzles/03-too-fast.bwp\n"
                        "130-bw2001/puzzles/145-slanted-banner.bwp\n130-bw2001/slanted-banner\n"
                        "010-counter/puzzles/notes.txt\n");
    CHECK(parsed.keys() == std::set<std::string, std::less<>>{"010-counter/puzzles/notes.txt",
                                                              "010-counter/too-fast",
                                                              "130-bw2001/slanted-banner"});
    CHECK(parsed.text() == "# Bodgewire progress\n010-counter/puzzles/notes.txt\n"
                           "010-counter/too-fast\n130-bw2001/slanted-banner\n");
    CHECK(parsed.upgraded());
    CHECK_FALSE(Progress::Parse(parsed.text()).upgraded());
    CHECK_FALSE(Progress::Parse("010-counter/puzzles/notes.txt\n").upgraded());
}

TEST_CASE("progress: add, merge and clear", "[machine]") {
    Progress progress;
    CHECK(progress.add("a"));
    CHECK_FALSE(progress.add("a"));
    CHECK(progress.has("a"));
    CHECK_FALSE(progress.has("b"));
    Progress other;
    other.add("b");
    other.add("a");
    progress.merge(other);
    CHECK(progress.keys().size() == 2);
    CHECK(progress.has("b"));
    progress.clear();
    CHECK(progress.empty());
}

TEST_CASE("progress: key", "[machine]") {
    const fs::path root = fs::temp_directory_path() / "bw_progress_key";
    fs::remove_all(root);
    WriteFile(root / "010-a/a.bwn", "");
    WriteFile(root / "010-a/puzzles/01-x.bwp", "machine a\ntitle \"X\"\nbrief \"Ex\"\n");
    WriteFile(root / "010-a/puzzles/random.bwr", "machine a\ncapture 1ms\n");
    WriteFile(root / "elsewhere/01-x.bwp", "machine a\ntitle \"X\"\nbrief \"Ex\"\n");
    const MachineList list = ListMachines(root);
    REQUIRE(list.machines.size() == 1);
    REQUIRE(list.machines[0].puzzles.size() == 1);
    CHECK(ProgressKey(list, root / "010-a/puzzles/01-x.bwp") == "010-a/x");
    CHECK(ProgressKey(list, root / "010-a/manuals/../puzzles/01-x.bwp") == "010-a/x");
    CHECK(ProgressKey(list.machines[0], list.machines[0].puzzles[0]) == "010-a/x");
    CHECK_FALSE(ProgressKey(list, root / "010-a/a.bwn").has_value());
    CHECK_FALSE(ProgressKey(list, root / "010-a/puzzles/random.bwr").has_value());
    CHECK_FALSE(ProgressKey(list, root / "elsewhere/01-x.bwp").has_value());
    fs::remove_all(root);
}

TEST_CASE("progress: file", "[machine]") {
    const fs::path root = fs::temp_directory_path() / "bw_progress_file";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path file = root / "progress.txt";
    const auto missing = ReadProgressFile(file);
    REQUIRE(missing.has_value());
    CHECK(missing->empty());

    Progress progress;
    progress.add("010-a/x");
    progress.add("020-b/z");
    REQUIRE(WriteProgressFile(file, progress).has_value());
    CHECK_FALSE(fs::exists(root / "progress.txt.tmp"));
    const auto read = ReadProgressFile(file);
    REQUIRE(read.has_value());
    CHECK(read->keys() == progress.keys());
    progress.clear();  // written over
    REQUIRE(WriteProgressFile(file, progress).has_value());
    CHECK(ReadProgressFile(file)->empty());

    fs::create_directories(root / "a directory");
    CHECK_FALSE(ReadProgressFile(root / "a directory").has_value());
    CHECK_FALSE(WriteProgressFile(root / "no such directory/progress.txt", progress).has_value());
    fs::remove_all(root);
}
