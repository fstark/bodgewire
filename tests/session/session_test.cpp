#include "session/session.hpp"

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "machine/crack.hpp"
#include "parts/ls47.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Session PuzzleSession(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    return session;
}

void RunTo(Session& session, Time t) {
    session.runner().pauseAt(t);
    session.runner().resume();
    session.runner().waitIdle();
}

// Point `nm` along the H0 Trace from the broken_trace crack.
Point Along(const Machine& machine, double nm) {
    const auto index = FindCrackTrace(machine.pristineBoard(), H0Crack().layer, H0Crack().at);
    REQUIRE(index.has_value());
    const Trace& h0 = machine.pristineBoard().traces[*index];
    const double dx = static_cast<double>(h0.end.x - h0.start.x);
    const double dy = static_cast<double>(h0.end.y - h0.start.y);
    const double length = std::hypot(dx, dy);
    return Point{H0Crack().at.x + static_cast<std::int64_t>(dx / length * nm),
                 H0Crack().at.y + static_cast<std::int64_t>(dy / length * nm)};
}

}  // namespace

TEST_CASE("session: repairs only with the power off", "[session]") {
    Session session = PuzzleSession("broken_trace");
    session.powerOn(1);
    CHECK(session.repair(SwapRepair{"U3"}) == RepairResult::Refused);
    CHECK(session.log().empty());
    session.powerOff();
    CHECK(session.repair(SwapRepair{"U3"}) == RepairResult::Done);
    CHECK(session.repair(MendRepair{Layer::Front, {0, 0}}) == RepairResult::Done);
    const std::vector<std::string> log(session.log().begin(), session.log().end());
    CHECK(log == std::vector<std::string>{"swap U3", "mend F.Cu 0.00 0.00"});
    CHECK(session.blobs().size() == 1);
    CHECK(session.repair(SwapRepair{"U99"}) == RepairResult::Unknown);
    CHECK(session.log().size() == 2);
}

TEST_CASE("session: drawn Board unchanged by a mend", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const std::vector<Trace> before = session.drawnBoard()->traces;
    const std::size_t machineTraces = session.machine()->board().traces.size();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    const std::vector<Trace>& after = session.drawnBoard()->traces;
    REQUIRE(after.size() == before.size());
    for (std::size_t t = 0; t < before.size(); ++t) {
        CHECK(after[t].start == before[t].start);
        CHECK(after[t].end == before[t].end);
    }
    CHECK(session.machine()->board().traces.size() == machineTraces - 1);
}

TEST_CASE("session: probe across a mend", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const Point left = Along(*session.machine(), -1'000'000);
    const Point right = Along(*session.machine(), 1'000'000);
    session.touch(ProbeTip{left, Layer::Front});
    const auto a = session.probedNet();
    session.touch(ProbeTip{right, Layer::Front});
    const auto b = session.probedNet();
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a != *b);

    session.powerOn(1);
    RunTo(session, 1_ms);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    REQUIRE(session.probeTip().has_value());
    CHECK(session.probeTip()->at == right);
    const auto rightNet = session.probedNet();
    session.touch(ProbeTip{left, Layer::Front});
    REQUIRE(rightNet.has_value());
    CHECK(session.probedNet() == rightNet);
    session.powerOff();
}

TEST_CASE("session: clips", "[session]") {
    Session session = PuzzleSession("broken_trace");
    CHECK(session.clip("U6", "6") == 0);
    CHECK(session.clip("U2", "9") == 1);
    CHECK(session.clipAt("U6", "6") == 0);
    REQUIRE(session.clips()[0].has_value());
    CHECK(session.clips()[0]->label == "U6.6 1Y");
    CHECK(session.clip("U6", "1Y") == 0);
    session.unclip(0);
    CHECK_FALSE(session.clipAt("U6", "6").has_value());
    CHECK(session.clip("U9", "5") == 0);
    for (std::size_t c = 2; c < kChannels; ++c) {
        CHECK(session.clip("U1", std::to_string(c + 9)) == c);
    }
    CHECK_FALSE(session.clip("U1", "2").has_value());
    CHECK_FALSE(session.clip("U99", "1").has_value());
}

TEST_CASE("session: unclip frees the Channel's Trigger", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.clip("U1", "14") == 0);
    AnalyzerSettings settings = session.analyzerSettings();
    settings.trigger[0] = TriggerCondition::Rise;
    settings.trigger[1] = TriggerCondition::High;
    session.setAnalyzer(settings);
    session.unclip(0);
    CHECK(session.analyzerSettings().trigger[0] == TriggerCondition::Any);
    CHECK(session.analyzerSettings().trigger[1] == TriggerCondition::High);
}

TEST_CASE("session: clips survive a repair", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.clip("U13", "6") == 0);
    REQUIRE(session.clip("U1", "14") == 1);
    CHECK(session.clipNet(0) != session.clipNet(1));
    session.powerOn(1);
    RunTo(session, 1_ms);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    CHECK(session.clipNet(0) == session.clipNet(1));
    session.powerOff();
}

TEST_CASE("session: arm before power-on captures", "[session]") {
    Session session = PuzzleSession("stuck_gate");
    REQUIRE(session.clip("U12", "11") == 0);
    session.setAnalyzer({.period = 1'000'000});
    session.arm();
    CHECK(session.analyzerPhase() == AnalyzerPhase::Armed);
    session.powerOn(1);
    RunTo(session, 5_ms);
    REQUIRE(session.analyzerPhase() == AnalyzerPhase::Captured);
    REQUIRE(session.captures().update());
    const Capture& capture = session.captures().front();
    for (std::size_t p = capture.first; p < kDepth; ++p) {
        CHECK((capture.points[p].unknown & 1) == 0);
        CHECK((capture.points[p].high & 1) == 1);
    }
    session.powerOff();
}

TEST_CASE("session: a capture is not re-armed by a repair", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.clip("U1", "14") == 0);
    session.setAnalyzer({.period = 1'000'000});
    session.arm();
    session.powerOn(1);
    RunTo(session, 5_ms);
    REQUIRE(session.analyzerPhase() == AnalyzerPhase::Captured);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    RunTo(session, 6_ms);
    CHECK(session.analyzerPhase() == AnalyzerPhase::Idle);
    session.powerOff();
}

TEST_CASE("session: hooks", "[session]") {
    Session session = PuzzleSession("broken_trace");
    CHECK(session.hook("U1", "2") == 0);
    CHECK(session.hook("U1", "QA") == 1);
    CHECK(session.hook("U1", "14") == 1);
    CHECK_FALSE(session.hook("U1", "13").has_value());
    REQUIRE(session.hooks()[1].has_value());
    CHECK(session.hooks()[1]->label == "U1.14 QA");
    CHECK(session.hookAt("U1", "14") == 1);
    CHECK_FALSE(session.hookRail(0).has_value());
    session.unhook(0);
    CHECK_FALSE(session.hookAt("U1", "2").has_value());
    CHECK(session.hook("J1", "GND") == 0);
    CHECK(session.hookRail(0) == 0.0);
}

TEST_CASE("session: meter volts", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto clk = session.padLead("U1", "2");
    const auto vcc = session.padLead("J1", "VCC");
    REQUIRE(clk.has_value());
    REQUIRE(vcc.has_value());
    CHECK_FALSE(session.volts().has_value());
    REQUIRE(session.placeLead({clk->at, Layer::Front, clk->label}));
    CHECK(session.volts() == 0.0);
    session.powerOn(1);
    RunTo(session, 1_s);
    REQUIRE(session.volts().has_value());
    CHECK(std::abs(*session.volts() - 1.85) < 0.01);
    REQUIRE(session.placeLead({vcc->at, Layer::Front, vcc->label}));
    RunTo(session, 1_s + 300_ms);
    CHECK(session.volts() == 5.0);
    session.powerOff();
    CHECK(session.volts() == 0.0);
    session.liftLeads();
    CHECK_FALSE(session.volts().has_value());
}

TEST_CASE("session: continuity refused with the power on", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto qa = session.padLead("U1", "14");
    REQUIRE(qa.has_value());
    session.setMeterMode(MeterMode::Continuity);
    session.powerOn(1);
    CHECK_FALSE(session.placeLead({qa->at, Layer::Front, qa->label}));
    CHECK_FALSE(session.continuity().has_value());
    session.powerOff();
}

TEST_CASE("session: continuity follows a mend", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto qa = session.padLead("U1", "14");
    const auto far = session.padLead("U13", "6");
    REQUIRE(qa.has_value());
    REQUIRE(far.has_value());
    session.setMeterMode(MeterMode::Continuity);
    CHECK_FALSE(session.blackNext());
    REQUIRE(session.placeLead({qa->at, Layer::Front, qa->label}));
    CHECK(session.blackNext());
    CHECK_FALSE(session.continuity().has_value());
    REQUIRE(session.placeLead({far->at, Layer::Front, far->label}));
    CHECK(session.continuity() == false);
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(session.continuity() == true);
    session.setMeterMode(MeterMode::Volts);
    CHECK_FALSE(session.continuity().has_value());
}

TEST_CASE("session: hooks and leads survive a repair", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto far = session.padLead("U13", "6");
    REQUIRE(far.has_value());
    REQUIRE(session.hook("U13", "6") == 0);
    REQUIRE(session.placeLead({far->at, Layer::Front, far->label}));
    session.powerOn(1);
    RunTo(session, 1_ms);
    CHECK(session.hookNet(0) != PinNet(session.circuit(), "U1", "14"));
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    const auto qa = PinNet(session.circuit(), "U1", "14");
    CHECK(session.hookNet(0) == PinNet(session.circuit(), "U13", "6"));
    CHECK(session.hookNet(0) == qa);
    CHECK(session.meterNet() == qa);
    session.powerOff();
}

TEST_CASE("session: netlist without a Board", "[session]") {
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.machine() == nullptr);
    CHECK(session.drawnBoard() == nullptr);
    CHECK(session.repair(SwapRepair{"U1"}) == RepairResult::Unknown);
    session.touch(ProbeTip{{0, 0}, Layer::Front});
    CHECK_FALSE(session.probeTip().has_value());
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(session.runner().status().simTime >= 1_ms);
}

namespace {

Session TestPatternSession() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    return session;
}

// Continuity Leads on U1.14 and U13.6, the two ends of H0.
void LeadsAcrossH0(Session& session) {
    const auto qa = session.padLead("U1", "14");
    const auto far = session.padLead("U13", "6");
    REQUIRE(qa.has_value());
    REQUIRE(far.has_value());
    session.setMeterMode(MeterMode::Continuity);
    REQUIRE(session.placeLead({qa->at, Layer::Front, qa->label}));
    REQUIRE(session.placeLead({far->at, Layer::Front, far->label}));
}

const WireRepair kH0Wire{{"U1", "14"}, {"U13", "6"}};

}  // namespace

TEST_CASE("session: cut and wire only with the power off", "[session]") {
    Session session = PuzzleSession("broken_trace");
    session.powerOn(1);
    CHECK(session.repair(CutRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Refused);
    CHECK(session.repair(kH0Wire) == RepairResult::Refused);
    CHECK(session.repair(UnwireRepair{kH0Wire.a, kH0Wire.b}) == RepairResult::Refused);
    session.powerOff();
    CHECK(session.repair(CutRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Unknown);
    CHECK(session.repair(kH0Wire) == RepairResult::Done);
    REQUIRE_FALSE(session.log().empty());
    CHECK(session.log().back() == "wire U1.14 U13.6");
    CHECK(session.repair(UnwireRepair{kH0Wire.a, kH0Wire.b}) == RepairResult::Done);
    CHECK(session.repair(UnwireRepair{kH0Wire.a, kH0Wire.b}) == RepairResult::Unknown);
    CHECK(session.log().size() == 2);
    CHECK(session.blobs().empty());
}

TEST_CASE("session: continuity through a bodge wire", "[session]") {
    Session session = PuzzleSession("broken_trace");
    LeadsAcrossH0(session);
    CHECK(session.continuity() == false);
    REQUIRE(session.repair(kH0Wire) == RepairResult::Done);
    CHECK(session.continuity() == true);
    CHECK(session.drawnBoard()->bodgeWires.empty());
}

TEST_CASE("session: continuity across a cut", "[session]") {
    Session session = TestPatternSession();
    const std::size_t traces = session.drawnBoard()->traces.size();
    LeadsAcrossH0(session);
    CHECK(session.continuity() == true);
    REQUIRE(session.repair(CutRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(session.continuity() == false);
    CHECK(session.drawnBoard()->traces.size() == traces);
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(session.continuity() == true);
}

TEST_CASE("session: counter LED", "[session]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.leds().size() == 1);
    CHECK(session.leds()[0].reference == "D1");
    CHECK(session.leds()[0].brightness == 0);

    session.powerOn(1, 100_ms);
    session.runner().waitIdle();
    const float brightness = session.leds()[0].brightness;
    CHECK((brightness == 0 || brightness == 1));
    session.powerOff();
    CHECK(session.leds()[0].brightness == 0);
}

TEST_CASE("session: chaser with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "chaser.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    std::vector<std::string> references;
    for (const LedLight& led : session.leds()) {
        references.push_back(led.reference);
    }
    CHECK(references == std::vector<std::string>{"D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8"});

    for (const char* pin : {"3", "4", "5", "6", "10", "11", "12", "13"}) {
        REQUIRE(session.clip("U5", pin).has_value());
    }
    REQUIRE(session.hook("U5", "8").has_value());
    REQUIRE(session.hook("U6", "8").has_value());
    const auto ser = session.padLead("U5", "1");
    const auto anode = session.padLead("D1", "A");
    REQUIRE(ser.has_value());
    REQUIRE(anode.has_value());
    session.touch(ProbeTip{ser->at, Layer::Front});
    REQUIRE(session.placeLead({anode->at, Layer::Front, anode->label}));

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    float sum = 0;
    for (const LedLight& led : session.leds()) {
        sum += led.brightness;
    }
    CHECK(std::abs(sum - 1.0f) < 0.05f);
    session.powerOff();
}

TEST_CASE("session: stopwatch with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "stopwatch.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    std::vector<std::string> lamps;
    for (const LedLight& led : session.leds()) {
        lamps.push_back(led.reference);
    }
    REQUIRE(lamps.size() == 24);  // DS1 A..G and DP, then DS2, DS3
    CHECK(lamps[0] == "DS1");
    CHECK(lamps[8] == "DS2");
    CHECK(lamps[16] == "DS3");
    CHECK(session.leds()[7].shape == LampShape::DecimalPoint);

    for (const char* chip : {"U1", "U2", "U3", "U4"}) {
        REQUIRE(session.clip(chip, "QD").has_value());
    }
    for (const char* pin : {"QA", "QB", "QC", "QD"}) {
        REQUIRE(session.clip("U7", pin).has_value());
    }
    REQUIRE(session.hook("U11", "1Q").has_value());
    REQUIRE(session.hook("U12", "1Y").has_value());
    const auto clock = session.padLead("U12", "1A");
    const auto segment = session.padLead("DS3", "A");
    REQUIRE(clock.has_value());
    REQUIRE(segment.has_value());
    session.touch(ProbeTip{clock->at, Layer::Front});
    REQUIRE(session.placeLead({segment->at, Layer::Front, segment->label}));

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.leds()[7].brightness > 0.95f);  // DS1's point, on GND
    session.powerOff();
}

TEST_CASE("session: ball with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "ball.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    CHECK(session.leds().empty());
    REQUIRE(session.tv() != nullptr);

    for (const char* pin : {"QA", "QB", "QC", "QD"}) {
        REQUIRE(session.clip("U23", pin).has_value());
    }
    REQUIRE(session.clip("U26", "1Q").has_value());
    REQUIRE(session.clip("U26", "2Q_N").has_value());
    REQUIRE(session.clip("U25", "1Y").has_value());
    REQUIRE(session.clip("U25", "2Y").has_value());
    REQUIRE(session.hook("U19", "RCO").has_value());
    REQUIRE(session.hook("U24", "3Y").has_value());
    const auto ball = session.padLead("U24", "3Y");
    const auto vcc = session.padLead("U16", "VCC");
    REQUIRE(ball.has_value());
    REQUIRE(vcc.has_value());
    session.touch(ProbeTip{ball->at, Layer::Front});
    REQUIRE(session.placeLead({vcc->at, Layer::Front, vcc->label}));

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.tv()->tv().isLocked());
    CHECK(session.tv()->tv().completedFrames() > 40);
    session.powerOff();
}

TEST_CASE("session: tennis with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "tennis.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    CHECK(session.leds().empty());
    REQUIRE(session.tv() != nullptr);
    CHECK(session.knobs() == std::vector<std::string>{"J3", "J4"});
    CHECK(session.buttons() == std::vector<std::string>{"SW1"});

    for (const char* chip : {"U48", "U49"}) {
        for (const char* pin : {"QA", "QB", "QC", "QD"}) {
            REQUIRE(session.clip(chip, pin).has_value());
        }
    }
    REQUIRE(session.hook("U53", "SA").has_value());
    REQUIRE(session.hook("U58", "Y").has_value());
    const auto start = session.padLead("SW1", "1");
    const auto vcc = session.padLead("U48", "VCC");
    REQUIRE(start.has_value());
    REQUIRE(vcc.has_value());
    session.touch(ProbeTip{start->at, Layer::Front});
    REQUIRE(session.placeLead({vcc->at, Layer::Front, vcc->label}));

    CHECK(session.setKnob("J3", 0.25));
    CHECK(session.setKnob("J4", 0.75));
    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.tv()->tv().isLocked());
    CHECK(session.tv()->tv().completedFrames() > 40);
    session.powerOff();
}

namespace {

// The lit segments of `digit` (A..G, bit k for 'A' + k) from the Session's lamps.
unsigned LitSegments(const Session& session, std::string_view digit) {
    unsigned lit = 0;
    int k = 0;
    for (const LedLight& led : session.leds()) {
        if (led.reference == digit && led.shape != LampShape::DecimalPoint) {
            if (led.brightness > 0.5f) {
                lit |= 1u << k;
            }
            ++k;
        }
    }
    return lit;
}

}  // namespace

TEST_CASE("session: message with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "message.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    CHECK(session.leds().empty());
    REQUIRE(session.tv() != nullptr);

    for (const char* pin : {"O1", "O2", "O3", "O4", "O5"}) {
        REQUIRE(session.clip("U15", pin).has_value());
    }
    REQUIRE(session.clip("U14", "O0").has_value());
    REQUIRE(session.clip("U16", "SH_LD_N").has_value());
    REQUIRE(session.clip("U16", "QH").has_value());
    REQUIRE(session.hook("U1", "RCO").has_value());
    REQUIRE(session.hook("U16", "CLK").has_value());
    const auto pixels = session.padLead("U16", "QH");
    const auto vcc = session.padLead("U15", "VCC");
    REQUIRE(pixels.has_value());
    REQUIRE(vcc.has_value());
    session.touch(ProbeTip{pixels->at, Layer::Front});
    REQUIRE(session.placeLead({vcc->at, Layer::Front, vcc->label}));

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.tv()->tv().isLocked());
    CHECK(session.tv()->tv().completedFrames() > 40);
    session.powerOff();
}

TEST_CASE("session: knob meter with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "knob.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.leds().size() == 16);  // DS1 A..G and DP, then DS2
    CHECK(session.knobs() == std::vector<std::string>{"J2"});

    for (const char* pin : {"A", "B", "C", "D"}) {
        REQUIRE(session.clip("U12", pin).has_value());
    }
    REQUIRE(session.clip("J2", "1").has_value());
    REQUIRE(session.clip("J2", "2").has_value());
    REQUIRE(session.clip("U6", "2Y").has_value());
    REQUIRE(session.clip("U6", "1Y").has_value());
    REQUIRE(session.hook("U8", "QA").has_value());
    REQUIRE(session.hook("U5", "QB").has_value());
    const auto pulse = session.padLead("J2", "2");
    const auto vcc = session.padLead("J2", "3");
    REQUIRE(pulse.has_value());
    REQUIRE(vcc.has_value());
    session.touch(ProbeTip{pulse->at, Layer::Front});
    REQUIRE(session.placeLead({vcc->at, Layer::Front, vcc->label}));

    CHECK(session.setKnob("J2", 0.42));
    session.powerOn(1, 300_ms);
    session.runner().waitIdle();
    CHECK(LitSegments(session, "DS1") == Ls47Segments(4, true, true, true));
    CHECK(LitSegments(session, "DS2") == Ls47Segments(2, true, true, true));
    session.powerOff();
}

TEST_CASE("session: no LEDs on the test pattern", "[session]") {
    CHECK(PuzzleSession("broken_trace").leds().empty());
}

TEST_CASE("session: buttons only press while powered", "[session]") {
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.buttons() == std::vector<std::string>{"SW1"});
    session.setPressed("SW1", true);
    CHECK_FALSE(session.pressed("SW1"));

    session.powerOn(1);
    session.setPressed("SW1", true);
    CHECK(session.pressed("SW1"));
    const Button* button = ButtonOf(session.circuit(), "SW1");
    REQUIRE(button != nullptr);
    CHECK(button->pressed());
    session.setPressed("SW9", true);  // not a Button: remembered, does nothing
    session.setPressed("SW9", false);
    session.powerOff();
    session.runner().waitIdle();
    CHECK_FALSE(session.pressed("SW1"));
    CHECK_FALSE(button->pressed());
}

TEST_CASE("session: rolling the dice", "[session]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    const auto brightness = [&](std::string_view reference) {
        for (const LedLight& led : session.leds()) {
            if (led.reference == reference) {
                return led.brightness;
            }
        }
        FAIL("no LED " << reference);
        return 0.0f;
    };

    session.powerOn(1, 10_ms);
    session.runner().waitIdle();
    session.setPressed("SW1", true);
    session.runner().pauseAt(70_ms);
    session.runner().resume();
    session.runner().waitIdle();
    CHECK(std::abs(brightness("D4") - 0.5f) < 0.02f);      // ODD_N: 1, 3, 5
    CHECK(std::abs(brightness("D3") - 1.0f / 6) < 0.02f);  // SIX_N: 6
    CHECK(std::abs(brightness("D1") - 5.0f / 6) < 0.02f);  // TWO_N: 2-6

    session.setPressed("SW1", false);
    session.runner().pauseAt(130_ms);
    session.runner().resume();
    session.runner().waitIdle();
    for (const LedLight& led : session.leds()) {
        INFO(led.reference);
        CHECK((led.brightness == 0 || led.brightness == 1));
    }
    session.powerOff();
}

TEST_CASE("session: dice rolls both odd and even faces", "[session]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    const auto runTo = [&](Time time) {
        session.runner().pauseAt(time);
        session.runner().resume();
        session.runner().waitIdle();
    };
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    bool odd = false;
    bool even = false;
    Time t = 1_ms;
    for (int roll = 0; roll < 16 && !(odd && even); ++roll) {
        session.setPressed("SW1", true);
        runTo(t += 3_ms);
        session.setPressed("SW1", false);
        runTo(t += 45_ms);  // two closed 20 ms LED windows after the release
        for (const LedLight& led : session.leds()) {
            if (led.reference == "D4") {
                (led.brightness > 0.5f ? odd : even) = true;
            }
        }
    }
    CHECK(odd);
    CHECK(even);
    session.powerOff();
}

TEST_CASE("session: knob kept across power", "[session]") {
    auto loaded = LoadNetlist("part X1 OSC freq=1kHz\npart J1 POWER\n"
                              "part J2 PADDLE min=30us max=9.93ms\npart U1 74LS04\n"
                              "net TRIG_N X1.OUT J2.TRIG_N\nnet PULSE J2.OUT U1.1A\n"
                              "net GND U1.2A U1.3A U1.4A U1.5A U1.6A\n",
                              "knob.bwn");
    REQUIRE(loaded.has_value());
    const Paddle* paddle = PaddleOf(**loaded, "J2");
    REQUIRE(paddle != nullptr);
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.knobs() == std::vector<std::string>{"J2"});
    CHECK_FALSE(session.setKnob("J9", 0.5));
    CHECK_FALSE(session.setKnob("U1", 0.5));
    CHECK(session.knob("J2") == 0.0);
    CHECK(session.setKnob("J2", 1.5));
    CHECK(session.knob("J2") == 1.0);
    CHECK(session.setKnob("J2", 0.25));  // with the power off
    CHECK(paddle->knob() == 0.25);
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(paddle->knob() == 0.25);
    session.powerOff();
    session.powerOn(2, 1_ms);
    session.runner().waitIdle();
    CHECK(session.knob("J2") == 0.25);
    CHECK(paddle->knob() == 0.25);
}

TEST_CASE("session: knob kept across a Repair", "[session]") {
    Session session = PuzzleSession("dead_knob");
    CHECK(session.setKnob("J2", 0.3));
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    session.powerOff();
    REQUIRE(session.repair(MendRepair{DeadKnobCrack().layer, DeadKnobCrack().at}) ==
            RepairResult::Done);
    session.powerOn(2, 1_ms);  // rebuilt from the mended Board
    session.runner().waitIdle();
    const Paddle* paddle = PaddleOf(session.circuit(), "J2");
    REQUIRE(paddle != nullptr);
    CHECK(paddle->knob() == 0.3);
    CHECK(session.knob("J2") == 0.3);
    session.powerOff();
}
