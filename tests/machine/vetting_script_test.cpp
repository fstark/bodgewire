#include "machine/vetting_script.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "machine/input_edges.hpp"
#include "machine/machine.hpp"
#include "machine/machine_list.hpp"
#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// "line: message" of each error, in order.
std::vector<std::string> Errors(std::string_view text) {
    const auto script = ParseVettingScript(text, "random.bwr");
    REQUIRE_FALSE(script.has_value());
    std::vector<std::string> errors;
    for (const Diagnostic& d : script.error()) {
        CHECK(d.file == "random.bwr");
        errors.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return errors;
}

}  // namespace

TEST_CASE("vetting script: every line", "[machine]") {
    const auto script =
        ParseVettingScript("# Random faults on a made-up Machine.\n"
                           "machine tennis   # as its netlist\n"
                           "\n"
                           "task \"Press START and turn both knobs.\"  # the Brief\n"
                           "knob J3:40%\n"
                           "press SW1:200ms:300ms\n"
                           "knob J3:85%:1100ms\n"
                           "type J5:1.4s:10 REM HI\\n\n"
                           "capture 1s\n"
                           "capture 1500ms # the knobs moved\n",
                           "random.bwr");
    REQUIRE(script.has_value());
    CHECK(script->file == "random.bwr");
    CHECK(script->machine == "tennis");
    CHECK(script->machineLine == 2);
    CHECK(script->task == "Press START and turn both knobs.");
    REQUIRE(script->presses.size() == 1);
    CHECK(script->presses[0].reference == "SW1");
    CHECK(script->presses[0].down == 200_ms);
    CHECK(script->presses[0].up == 300_ms);
    REQUIRE(script->knobs.size() == 2);
    CHECK(script->knobs[0].turn == 0.4);
    CHECK(script->knobs[0].at == 0);
    CHECK(script->knobs[1].turn == 0.85);
    CHECK(script->knobs[1].at == 1100_ms);
    REQUIRE(script->typing.size() == 1);
    CHECK(script->typing[0].reference == "J5");
    CHECK(script->typing[0].start == 1400_ms);
    CHECK(script->typing[0].text == U"10 REM HI\r");
    CHECK(script->captures == std::vector<Time>{1000_ms, 1500_ms});

    const auto bare = ParseVettingScript("machine counter\ncapture 250ms", "random.bwr");
    REQUIRE(bare.has_value());
    CHECK(bare->task.empty());
    CHECK(bare->presses.empty());
    CHECK(bare->captures == std::vector<Time>{250_ms});
}

TEST_CASE("vetting script: a type line keeps its spaces and its #", "[machine]") {
    const auto script = ParseVettingScript("machine typewriter\n"
                                           "task \"Type a # and a few words.\"\n"
                                           "  type   J3:1s:HELLO,  WORLD #1  \n"
                                           "capture 2s\n",
                                           "random.bwr");
    REQUIRE(script.has_value());
    CHECK(script->task == "Type a # and a few words.");
    REQUIRE(script->typing.size() == 1);
    CHECK(script->typing[0].text == U"HELLO,  WORLD #1");
}

TEST_CASE("vetting script: errors by line", "[machine]") {
    CHECK(Errors("machine Dice\n"
                 "machine dice\n"
                 "machine dice\n"
                 "machine\n"
                 "task Hold ROLL\n"
                 "task \"Hold ROLL.\"\n"
                 "task \"Again.\"\n"
                 "press SW1:300ms\n"
                 "press SW1:300ms:800ms SW2:1ms:2ms\n"
                 "knob J2:120%\n"
                 "knob\n"
                 "type\n"
                 "type J3:soon:HELLO\n"
                 "fault stuck U1.QA 0\n"
                 "capture\n"
                 "capture soon\n"
                 "capture 500ms\n") ==
          std::vector<std::string>{
              "1: bad machine name 'Dice'",
              "3: machine already given at line 2",
              "4: expected: machine NAME",
              "5: expected: task \"TEXT\"",
              "7: task already given at line 6",
              "8: bad press 'SW1:300ms' (expected REF:DOWN:UP)",
              "9: expected: press REF:DOWN:UP",
              "10: bad knob 'J2:120%' (expected REF:N% or REF:N%:TIME, 0 to 100)",
              "11: expected: knob REF:N%[:TIME]",
              "12: expected: type REF:START:TEXT",
              "13: bad typing 'J3:soon:HELLO' (expected REF:START:TEXT)",
              "14: unknown directive 'fault'",
              "15: expected: capture TIME",
              ParseDuration("soon").error().insert(0, "16: "),
          });
    CHECK(Errors("capture 1ms\n") == std::vector<std::string>{"0: no machine line"});
    CHECK(Errors("machine dice\n") == std::vector<std::string>{"0: no capture line"});
    CHECK(Errors("") == std::vector<std::string>{"0: no machine line", "0: no capture line"});
    // A capture line in error is its own error, not also "no capture line".
    CHECK(Errors("machine dice\ncapture never\n").size() == 1);
}

TEST_CASE("vetting script: captures in order", "[machine]") {
    CHECK(Errors("machine dice\ncapture 500ms\ncapture 500ms\ncapture 700ms\ncapture 600ms\n") ==
          std::vector<std::string>{"3: captures must come in time order",
                                   "5: captures must come in time order"});
}

TEST_CASE("vetting script: a file that cannot be read", "[machine]") {
    const auto script = ReadVettingScriptFile(test::RootDir() / "machines/none/random.bwr");
    REQUIRE_FALSE(script.has_value());
    REQUIRE(script.error().size() == 1);
    CHECK(script.error()[0].file == "random.bwr");
    CHECK(script.error()[0].message == "cannot read file");
}

TEST_CASE("vetting script: every Machine's inputs exist", "[machine]") {
    // Each Machine's puzzles/random.bwr against its own netlist: the switches, knobs and
    // keyboards it names, and no input starting after the last capture time (never seen).
    const MachineList list = ListMachines(test::RootDir() / "machines");
    REQUIRE(list.machines.size() == 14);
    for (const MachineEntry& machine : list.machines) {
        INFO(machine.dir.filename().string());
        REQUIRE(machine.random.has_value());
        const auto script = ReadVettingScriptFile(*machine.random);
        REQUIRE(script.has_value());
        CHECK(script->machine == machine.netlist.stem().string());
        CHECK(MachineDir(*machine.random) == MachineDir(machine.netlist));
        auto loaded = LoadNetlistFile(machine.netlist);
        REQUIRE(loaded.has_value());
        const auto edges = InputEdges(**loaded, script->presses, script->knobs, script->typing);
        REQUIRE(edges.has_value());
        const Time last = script->captures.back();
        for (const ButtonPress& press : script->presses) {  // released later is fine: the Dice
            CHECK(press.down < last);
        }
        for (const KnobSetting& knob : script->knobs) {
            CHECK(knob.at < last);
        }
        for (const Typing& typing : script->typing) {
            CHECK(typing.start < last);
        }
        // The task is there exactly when the player has something to do.
        CHECK(script->task.empty() ==
              (script->presses.empty() && script->knobs.empty() && script->typing.empty()));
    }
}
