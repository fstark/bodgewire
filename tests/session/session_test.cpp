#include "session/session.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/ram2114.hpp"
#include "parts/zx80_keys.hpp"

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "machine/crack.hpp"
#include "parts/ls47.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Session PuzzleSession(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    return session;
}

void RunTo(Session& session, Time t) {
    session.runner().resumeUntil(t);
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

std::size_t Clipped(const Session& session);

// Every Channel not yet clipped but `leave`, on the Board's pads in order (any pad with a lead,
// none of `skip`'s): a full load.
void ClipTheRest(Session& session, std::size_t leave = 0, std::string_view skip = "") {
    for (const BoardComponent& component : session.machine()->board().components) {
        if (component.reference == skip) {
            continue;
        }
        for (const Pad& pad : component.pads) {
            if (Clipped(session) + leave >= kChannels) {
                return;
            }
            if (!session.analyzer().clipAt(component.reference, pad.number) &&
                session.padLead(component.reference, pad.number)) {
                REQUIRE(session.analyzer().clip(component.reference, pad.number).has_value());
            }
        }
    }
}

std::size_t Clipped(const Session& session) {
    return static_cast<std::size_t>(std::ranges::count_if(
        session.analyzer().clips(), [](const auto& c) { return c.has_value(); }));
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
    session.probe().touch(ProbeTip{left, Layer::Front});
    const auto a = session.probe().net();
    session.probe().touch(ProbeTip{right, Layer::Front});
    const auto b = session.probe().net();
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a != *b);

    session.powerOn(1);
    RunTo(session, 1_ms);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    REQUIRE(session.probe().tip().has_value());
    CHECK(session.probe().tip()->at == right);
    const auto rightNet = session.probe().net();
    session.probe().touch(ProbeTip{left, Layer::Front});
    REQUIRE(rightNet.has_value());
    CHECK(session.probe().net() == rightNet);
    session.powerOff();
}

TEST_CASE("session: probe on an unconnected pin", "[session]") {
    // The heartbeat fixture: U1 RW, no copper; the 6502 only reads (NOPs), so RW stays high.
    auto machine = LoadNetlistMachine(test::HeartbeatNetlistPath());
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    const auto rw = session.padLead("U1", "RW");
    REQUIRE(rw.has_value());
    session.probe().touch(ProbeTip{rw->at, Layer::Front});
    REQUIRE(session.probe().net().has_value());
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(session.probe().lamps().high);
    CHECK_FALSE(session.probe().lamps().low);
    session.powerOff();
}

TEST_CASE("session: clips", "[session]") {
    Session session = PuzzleSession("broken_trace");
    CHECK(session.analyzer().clip("U6", "6") == 0);
    CHECK(session.analyzer().clip("U2", "9") == 1);
    CHECK(session.analyzer().clipAt("U6", "6") == 0);
    REQUIRE(session.analyzer().clips()[0].has_value());
    CHECK(session.analyzer().clips()[0]->label == "U6.6 1Y");
    CHECK(session.analyzer().clip("U6", "1Y") == 0);
    session.analyzer().unclip(0);
    CHECK_FALSE(session.analyzer().clipAt("U6", "6").has_value());
    CHECK(session.analyzer().clip("U9", "5") == 0);
    std::size_t next = 2;
    for (const BoardComponent& component : session.machine()->board().components) {
        for (const Pad& pad : component.pads) {
            if (next < kChannels && !session.analyzer().clipAt(component.reference, pad.number) &&
                session.padLead(component.reference, pad.number)) {
                CHECK(session.analyzer().clip(component.reference, pad.number) == next++);
            }
        }
    }
    REQUIRE(next == kChannels);
    // Every Channel used: a pin not yet clipped is refused.
    bool refused = false;
    for (const BoardComponent& component : session.machine()->board().components) {
        for (const Pad& pad : component.pads) {
            if (!refused && !session.analyzer().clipAt(component.reference, pad.number) &&
                session.padLead(component.reference, pad.number)) {
                CHECK_FALSE(session.analyzer().clip(component.reference, pad.number).has_value());
                refused = true;
            }
        }
    }
    CHECK(refused);
    CHECK_FALSE(session.analyzer().clip("U99", "1").has_value());
}

namespace {

Session HeartbeatSession() {
    auto machine = LoadNetlistMachine(test::HeartbeatNetlistPath());
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return Session(std::move(*machine), std::move(*built));
}

}  // namespace

TEST_CASE("session: clip a group", "[session]") {
    Session session = HeartbeatSession();
    REQUIRE(session.analyzer().clip("U1", "RW") == 0);
    REQUIRE(session.analyzer().clip("U1", "A5") == 1);
    const auto result = session.analyzer().clipGroup("U1", "A3");
    REQUIRE(result.has_value());
    REQUIRE(result->group == 0);
    REQUIRE(session.analyzer().groups().size() == 1);
    const ChannelGroup& group = session.analyzer().groups()[0];
    CHECK(group.name == "U1 A");
    REQUIRE(group.channels.size() == 16);
    for (std::size_t b = 0; b < 16; ++b) {
        INFO(b);
        const auto& clip = session.analyzer().clips()[group.channels[b]];
        REQUIRE(clip.has_value());
        CHECK(clip->label.ends_with(" A" + std::to_string(b)));
    }
    CHECK(group.channels[5] == 1);                                  // already clipped: moved in
    CHECK(session.analyzer().clipAt("U1", "12") == result->first);  // A3
    CHECK(session.analyzer().groupOf(0) == std::nullopt);
    CHECK(session.analyzer().groupOf(1) == 0);

    // RW has no stem partner: clipped alone (already, so its Channel).
    const auto alone = session.analyzer().clipGroup("U1", "RW");
    REQUIRE(alone.has_value());
    CHECK(alone->first == 0);
    CHECK_FALSE(alone->group.has_value());

    REQUIRE(session.analyzer().clipGroup("U1", "D0").has_value());
    CHECK(session.analyzer().groups()[1].name == "U1 D");
    // PHI0, PHI1, PHI2 share a stem too.
    const auto phi = session.analyzer().clipGroup("U1", "PHI2");
    REQUIRE(phi.has_value());
    REQUIRE(phi->group == 2);
    CHECK(session.analyzer().groups()[2].name == "U1 PHI");
    CHECK(session.analyzer().groups()[2].channels.size() == 3);
    CHECK_FALSE(session.analyzer().clipGroup("U99", "A0").has_value());
}

TEST_CASE("session: clip a bus the Part declares", "[session]") {
    Session session = PuzzleSession("dark_led");  // U1-U5: 74LS161
    // QA-QD have no digits: the 74LS161 declares them, QA bit 0 whichever pin is clicked.
    const auto q = session.analyzer().clipGroup("U1", "QC");
    REQUIRE(q.has_value());
    REQUIRE(q->group == 0);
    const ChannelGroup& outputs = session.analyzer().groups()[0];
    CHECK(outputs.name == "U1 QA-QD");
    REQUIRE(outputs.channels.size() == 4);
    for (std::size_t b = 0; b < 4; ++b) {
        INFO(b);
        CHECK(session.analyzer().clips()[outputs.channels[b]]->pin ==
              std::to_string(14 - b));  // QA 14 .. QD 11
    }
    const auto data = session.analyzer().clipGroup("U2", "A");
    REQUIRE(data.has_value());
    CHECK(session.analyzer().groups()[1].name == "U2 A-D");
    CHECK(session.analyzer().clips()[session.analyzer().groups()[1].channels[0]]->pin == "3");  // A
    // ENP is in no bus: clipped alone.
    const auto enp = session.analyzer().clipGroup("U1", "ENP");
    REQUIRE(enp.has_value());
    CHECK_FALSE(enp->group.has_value());

    // A hand group of declared pins is ordered by bit, not reversed as given.
    session.analyzer().ungroup(0);
    const auto qa = session.analyzer().clipAt("U1", "14");
    const auto qb = session.analyzer().clipAt("U1", "13");
    const auto qc = session.analyzer().clipAt("U1", "12");
    const auto qd = session.analyzer().clipAt("U1", "11");
    REQUIRE((qa && qb && qc && qd));
    const auto hand = session.analyzer().group({*qa, *qb, *qc, *qd});
    REQUIRE(hand.has_value());
    CHECK(session.analyzer().groups()[*hand].channels == std::vector{*qa, *qb, *qc, *qd});
}

TEST_CASE("session: group refused without enough channels", "[session]") {
    Session session = HeartbeatSession();
    REQUIRE(session.analyzer().clipGroup("U1", "A0").has_value());  // 16
    REQUIRE(session.analyzer().clipGroup("U1", "D0").has_value());  // 24
    for (const char* pin : {"RW", "PHI0", "PHI1", "PHI2", "SYNC", "RDY"}) {
        REQUIRE(session.analyzer().clip("U1", pin).has_value());  // 30
    }
    ClipTheRest(session, 2, "U2");  // 62
    REQUIRE(Clipped(session) == kChannels - 2);
    // U2 is the first 74LS244: 1A1-1A4 is a group of 4, 2 free.
    const auto result = session.analyzer().clipGroup("U2", "1A1");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == "group U2 1A needs 4 channels, 2 free");
}

TEST_CASE("session: unclip leaves the group", "[session]") {
    Session session = HeartbeatSession();
    REQUIRE(session.analyzer().clipGroup("U1", "D0").has_value());
    const std::vector<std::size_t> channels = session.analyzer().groups()[0].channels;
    for (std::size_t b = 0; b < 6; ++b) {
        session.analyzer().unclip(channels[b]);
    }
    REQUIRE(session.analyzer().groups().size() == 1);
    CHECK(session.analyzer().groups()[0].channels.size() == 2);
    session.analyzer().unclip(channels[6]);  // one left: the group dissolves, D7 stays clipped
    CHECK(session.analyzer().groups().empty());
    CHECK(session.analyzer().clips()[channels[7]].has_value());

    REQUIRE(session.analyzer().clipGroup("U1", "D0").has_value());
    REQUIRE(session.analyzer().setGroupTrigger(0, "81"));
    session.analyzer().unclipGroup(0);
    CHECK(session.analyzer().groups().empty());
    CHECK(std::ranges::none_of(session.analyzer().clips(),
                               [](const auto& c) { return c.has_value(); }));
    CHECK(std::ranges::all_of(session.analyzer().settings().trigger,
                              [](TriggerCondition t) { return t == TriggerCondition::Any; }));
}

TEST_CASE("session: hand groups", "[session]") {
    Session session = HeartbeatSession();
    const auto a2 = session.analyzer().clip("U1", "A2");
    const auto a0 = session.analyzer().clip("U1", "A0");
    const auto rw = session.analyzer().clip("U1", "RW");
    const auto sync = session.analyzer().clip("U1", "SYNC");
    REQUIRE((a2 && a0 && rw && sync));
    CHECK_FALSE(session.analyzer().group({*a0}).has_value());
    CHECK_FALSE(session.analyzer().group({*a0, 9}).has_value());  // a free Channel
    // Both with a stem: by index, A0 first.
    REQUIRE(session.analyzer().group({*a2, *a0}) == 0);
    CHECK(session.analyzer().groups()[0].channels == std::vector{*a0, *a2});
    CHECK(session.analyzer().groups()[0].name == "group 1");
    // RW and SYNC have none: as given, the first most significant.
    REQUIRE(session.analyzer().group({*rw, *sync}) == 1);
    CHECK(session.analyzer().groups()[1].channels == std::vector{*sync, *rw});
    CHECK(session.analyzer().groups()[1].name == "group 2");
    // Regrouping takes Channels out of their groups (both dissolve).
    REQUIRE(session.analyzer().group({*a0, *rw}) == 0);
    CHECK(session.analyzer().groups().size() == 1);
    CHECK(session.analyzer().groups()[0].name == "group 1");
    session.analyzer().ungroup(0);
    CHECK(session.analyzer().groups().empty());
    CHECK(session.analyzer().clips()[*a0].has_value());
}

TEST_CASE("session: group trigger", "[session]") {
    Session session = HeartbeatSession();
    REQUIRE(session.analyzer().clipGroup("U1", "A0").has_value());
    const ChannelGroup group = session.analyzer().groups()[0];
    CHECK(session.analyzer().groupTrigger(0) == "xxxx");
    REQUIRE(session.analyzer().setGroupTrigger(0, "E0xx"));
    CHECK(session.analyzer().groupTrigger(0) == "E0xx");
    const auto& trigger = session.analyzer().settings().trigger;
    CHECK(trigger[group.channels[15]] == TriggerCondition::High);
    CHECK(trigger[group.channels[12]] == TriggerCondition::Low);
    CHECK(trigger[group.channels[0]] == TriggerCondition::Any);
    CHECK_FALSE(session.analyzer().setGroupTrigger(0, "1E0xx"));
    CHECK(session.analyzer().groupTrigger(0) == "E0xx");
}

TEST_CASE("session: groups survive a repair", "[session]") {
    Session session = HeartbeatSession();
    session.setSpeed(Speed::Max);
    REQUIRE(session.analyzer().clipGroup("U1", "A0").has_value());
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    session.powerOff();
    REQUIRE(session.repair(SwapRepair{"U2"}) == RepairResult::Done);
    session.powerOn(2, 1_ms);
    session.runner().waitIdle();
    session.powerOff();
    REQUIRE(session.analyzer().groups().size() == 1);
    CHECK(session.analyzer().groups()[0].channels.size() == 16);
}

TEST_CASE("session: unclip frees the Channel's Trigger", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.analyzer().clip("U1", "14") == 0);
    AnalyzerSettings settings = session.analyzer().settings();
    settings.trigger[0] = TriggerCondition::Rise;
    settings.trigger[1] = TriggerCondition::High;
    session.analyzer().setSettings(settings);
    session.analyzer().unclip(0);
    CHECK(session.analyzer().settings().trigger[0] == TriggerCondition::Any);
    CHECK(session.analyzer().settings().trigger[1] == TriggerCondition::High);
}

TEST_CASE("session: clips survive a repair", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.analyzer().clip("U13", "6") == 0);
    REQUIRE(session.analyzer().clip("U1", "14") == 1);
    CHECK(session.analyzer().net(0) != session.analyzer().net(1));
    session.powerOn(1);
    RunTo(session, 1_ms);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    CHECK(session.analyzer().net(0) == session.analyzer().net(1));
    session.powerOff();
}

TEST_CASE("session: arm before power-on captures", "[session]") {
    Session session = PuzzleSession("stuck_gate");
    REQUIRE(session.analyzer().clip("U12", "11") == 0);
    session.analyzer().setSettings({.period = 10'000});  // 100 MHz: a Capture in 655 us
    session.analyzer().arm();
    CHECK(session.analyzer().phase() == AnalyzerPhase::Armed);
    session.powerOn(1);
    RunTo(session, 5_ms);
    REQUIRE(session.analyzer().phase() == AnalyzerPhase::Captured);
    REQUIRE(session.analyzer().captures().update());
    const Capture& capture = session.analyzer().captures().front();
    for (std::size_t p = capture.first; p < kDepth; ++p) {
        CHECK((capture.points[p].unknown & 1) == 0);
        CHECK((capture.points[p].high & 1) == 1);
    }
    session.powerOff();
}

TEST_CASE("session: a power-on re-arms after a Capture", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.analyzer().clip("U1", "14") == 0);
    session.analyzer().setSettings({.period = 10'000});  // 100 MHz: a Capture in 655 us
    session.powerOn(1);
    RunTo(session, 5_ms);
    REQUIRE(session.analyzer().phase() == AnalyzerPhase::Captured);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);  // P8i: armed again, no key
    CHECK(session.analyzer().phase() == AnalyzerPhase::Armed);
    RunTo(session, 6_ms);
    CHECK(session.analyzer().phase() == AnalyzerPhase::Captured);
    CHECK(session.analyzer().captures().update());  // the rebuilt consumer's own Capture
    session.powerOff();
}

TEST_CASE("session: a power-on with no Clip does not arm", "[session]") {
    Session session = PuzzleSession("broken_trace");
    session.powerOn(1);
    RunTo(session, 1_ms);
    CHECK(session.analyzer().phase() == AnalyzerPhase::Idle);
    session.powerOff();
}

TEST_CASE("session: a new Clip arms", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.analyzer().clip("U1", "14") == 0);
    session.analyzer().setSettings({.period = 10'000});
    session.powerOn(1);
    RunTo(session, 5_ms);
    REQUIRE(session.analyzer().phase() == AnalyzerPhase::Captured);
    REQUIRE(session.analyzer().clip("U1", "13") == 1);  // after a Capture too (P8i)
    CHECK(session.analyzer().phase() == AnalyzerPhase::Armed);
    RunTo(session, 10_ms);
    CHECK(session.analyzer().phase() == AnalyzerPhase::Captured);
    session.powerOff();
}

TEST_CASE("session: a Channel reused is not in the Capture", "[session]") {
    Session session = PuzzleSession("broken_trace");
    AnalyzerSetup& analyzer = session.analyzer();
    REQUIRE(analyzer.clip("U1", "14") == 0);
    REQUIRE(analyzer.clip("U1", "13") == 1);
    analyzer.setSettings({.period = 10'000});
    session.powerOn(1);
    RunTo(session, 5_ms);
    REQUIRE(analyzer.phase() == AnalyzerPhase::Captured);
    REQUIRE(analyzer.captures().update());
    const Capture capture = analyzer.captures().front();
    analyzer.unclip(1);
    REQUIRE(analyzer.clip("U1", "12") == 1);  // the freed Channel, another pin
    CHECK(analyzer.inCapture(capture, std::vector<std::size_t>{0}));
    CHECK_FALSE(analyzer.inCapture(capture, std::vector<std::size_t>{1}));
    CHECK_FALSE(analyzer.inCapture(capture, std::vector<std::size_t>{0, 1}));
    analyzer.unclip(1);
    REQUIRE(analyzer.clip("U1", "13") == 1);  // the same pin again: what the Capture holds
    CHECK(analyzer.inCapture(capture, std::vector<std::size_t>{1}));
    analyzer.unclip(1);
    CHECK_FALSE(analyzer.inCapture(capture, std::vector<std::size_t>{1}));  // free
    session.powerOff();
}

TEST_CASE("session: the analyzer's rows in clip order", "[session]") {
    Session session = PuzzleSession("broken_trace");
    AnalyzerSetup& analyzer = session.analyzer();
    const auto order = [&] {
        std::vector<std::size_t> out;  // a loose row's Channel; a group's: 100 + its index
        for (const AnalyzerRow& row : analyzer.rows()) {
            out.push_back(row.group ? 100 + *row.group : row.channel);
        }
        return out;
    };
    REQUIRE(analyzer.clip("U1", "14") == 0);
    REQUIRE(analyzer.clip("U1", "13") == 1);
    REQUIRE(analyzer.clip("U1", "12") == 2);
    CHECK(order() == std::vector<std::size_t>{0, 1, 2});
    analyzer.unclip(1);
    CHECK(order() == std::vector<std::size_t>{0, 2});
    REQUIRE(analyzer.clip("U1", "11") == 1);  // the free Channel, its row at the bottom
    CHECK(order() == std::vector<std::size_t>{0, 2, 1});
    REQUIRE(analyzer.group({2, 1}) == 0);  // a group's row where its first member is
    CHECK(order() == std::vector<std::size_t>{0, 100});
    CHECK(analyzer.rows()[1].channel == 2);
    analyzer.move(1, 0);
    CHECK(order() == std::vector<std::size_t>{100, 0});
    analyzer.move(0, 9);  // clamped: to the bottom
    CHECK(order() == std::vector<std::size_t>{0, 100});
    analyzer.move(1, 0);
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);  // a rebuild
    CHECK(order() == std::vector<std::size_t>{100, 0});
    session.powerOff();
    analyzer.ungroup(0);
    CHECK(order() == std::vector<std::size_t>{2, 1, 0});  // the members where the group was
}

TEST_CASE("session: hooking the scope leaves a running Capture alone", "[session]") {
    Session session = PuzzleSession("broken_trace");
    REQUIRE(session.analyzer().clip("U1", "14") == 0);
    session.analyzer().arm();  // 10 MHz, any Trigger: filling from power-on, for 6.55 ms
    session.powerOn(1);
    RunTo(session, 3_ms);
    REQUIRE(session.analyzer().phase() == AnalyzerPhase::Filling);
    REQUIRE(session.scope().hook("U1", "2") == 0);
    RunTo(session, 8_ms);  // started again at 3 ms, it would fill until 9.55 ms
    CHECK(session.analyzer().phase() == AnalyzerPhase::Captured);
    session.powerOff();
}

TEST_CASE("session: hooks", "[session]") {
    Session session = PuzzleSession("broken_trace");
    CHECK(session.scope().hook("U1", "2") == 0);
    CHECK(session.scope().hook("U1", "QA") == 1);
    CHECK(session.scope().hook("U1", "14") == 1);
    CHECK(session.scope().hook("U1", "13") == 2);
    CHECK(session.scope().hook("U1", "12") == 3);
    CHECK_FALSE(session.scope().hook("U1", "11").has_value());
    REQUIRE(session.scope().hooks()[1].has_value());
    CHECK(session.scope().hooks()[1]->label == "U1.14 QA");
    CHECK(session.scope().hookAt("U1", "14") == 1);
    CHECK_FALSE(session.scope().rail(0).has_value());
    session.scope().unhook(0);
    CHECK_FALSE(session.scope().hookAt("U1", "2").has_value());
    CHECK(session.scope().hook("J1", "GND") == 0);
    CHECK(session.scope().rail(0) == 0.0);
}

TEST_CASE("session: the scope's bands in hook order", "[session]") {
    Session session = PuzzleSession("broken_trace");
    ScopeSetup& scope = session.scope();
    REQUIRE(scope.hook("U1", "2") == 0);
    REQUIRE(scope.hook("U1", "14") == 1);
    REQUIRE(scope.hook("U1", "13") == 2);
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{0, 1, 2}));
    scope.unhook(1);
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{0, 2}));
    REQUIRE(scope.hook("U1", "12") == 1);  // the free Channel, its band at the bottom
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{0, 2, 1}));
}

TEST_CASE("session: the scope's source follows its Hook", "[session]") {
    Session session = PuzzleSession("broken_trace");
    ScopeSetup& scope = session.scope();
    REQUIRE(scope.hook("U1", "2") == 0);
    REQUIRE(scope.hook("U1", "14") == 1);
    REQUIRE(scope.hook("U1", "13") == 2);
    ScopeSettings settings = scope.settings();
    settings.source = 1;
    scope.setSettings(settings);
    scope.unhook(1);  // the source's band: the Trigger goes to the top band
    CHECK(scope.settings().source == 0);
    scope.unhook(0);
    CHECK(scope.settings().source == 2);
    scope.unhook(2);
    REQUIRE(scope.hook("U1", "12") == 0);
    REQUIRE(scope.hook("U1", "11") == 1);
    settings = scope.settings();
    settings.source = 1;
    scope.setSettings(settings);
    scope.unhook(1);
    scope.unhook(0);                       // none left: the source stays where it was
    REQUIRE(scope.hook("U1", "13") == 0);  // the first Hook takes the Trigger
    CHECK(scope.settings().source == 0);
}

TEST_CASE("session: the scope's bands moved", "[session]") {
    Session session = PuzzleSession("broken_trace");
    ScopeSetup& scope = session.scope();
    REQUIRE(scope.hook("U1", "2") == 0);
    REQUIRE(scope.hook("U1", "14") == 1);
    REQUIRE(scope.hook("U1", "13") == 2);
    scope.move(2, 0);
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{2, 0, 1}));
    scope.move(0, 9);  // clamped: to the bottom
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{0, 1, 2}));
    scope.move(1, 1);
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{0, 1, 2}));
    scope.move(2, 1);
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(std::ranges::equal(scope.order(), std::vector<std::size_t>{0, 2, 1}));  // a rebuild
}

TEST_CASE("session: clips and hooks on a resistor's pad and a capacitor's", "[session]") {
    // A passive Part has no Component: its pads by number, on the Net the Board gives them.
    Session session = PuzzleSession("gap");
    const auto netUnder = [&](std::string_view reference, std::string_view pin) {
        const auto lead = session.padLead(reference, pin);
        REQUIRE(lead.has_value());
        session.probe().touch(ProbeTip{lead->at, Layer::Front});
        return session.probe().net();
    };
    REQUIRE(session.analyzer().clip("R6", "2") == 0);
    REQUIRE(session.analyzer().clip("D5", "2") == 1);
    CHECK(session.analyzer().clips()[0]->label == "R6.2");
    CHECK(session.analyzer().clipAt("R6", "2") == 0);
    REQUIRE(session.analyzer().net(0).has_value());
    CHECK(session.analyzer().net(0) == netUnder("R6", "2"));
    CHECK(session.analyzer().net(1) == netUnder("D5", "2"));
    CHECK(session.analyzer().net(0) != session.analyzer().net(1));  // either side of the crack
    const auto alone = session.analyzer().clipGroup("R6", "2");     // no bus: clipped alone
    REQUIRE(alone.has_value());
    CHECK(alone->first == 0);
    CHECK_FALSE(alone->group.has_value());
    CHECK(session.analyzer().group({0, 1}).has_value());
    CHECK_FALSE(session.analyzer().clip("R6", "3").has_value());

    REQUIRE(session.scope().hook("R6", "1") == 0);
    CHECK(session.scope().hooks()[0]->label == "R6.1");
    CHECK(session.scope().rail(0) == 5.0);  // the pull-up's VCC end

    auto bw80 = LoadNetlistMachine(test::NetlistPath("bw80"));
    REQUIRE(bw80.has_value());
    auto built = bw80->build();
    REQUIRE(built.has_value());
    Session video(std::move(*bw80), std::move(*built));
    REQUIRE(video.analyzer().clip("C9", "1") == 0);  // on DOT2, as IC18's 2CLK
    REQUIRE(video.analyzer().clip("IC18", "2CLK") == 1);
    REQUIRE(video.analyzer().net(0).has_value());
    CHECK(video.analyzer().net(0) == video.analyzer().net(1));
    CHECK(video.scope().hook("C9", "2") == 0);
}

TEST_CASE("session: meter volts", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto clk = session.padLead("U1", "2");
    const auto vcc = session.padLead("J1", "VCC");
    REQUIRE(clk.has_value());
    REQUIRE(vcc.has_value());
    CHECK_FALSE(session.meter().volts().has_value());
    session.meter().hold(TestLead{clk->at, Layer::Front, clk->label});
    CHECK(session.meter().mode() == MeterMode::Continuity);
    CHECK_FALSE(session.meter().volts().has_value());  // the power off: Continuity
    session.powerOn(1);
    CHECK(session.meter().mode() == MeterMode::Volts);
    RunTo(session, 1_s);
    REQUIRE(session.meter().volts().has_value());
    CHECK(std::abs(*session.meter().volts() - 1.85) < 0.01);
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});
    RunTo(session, 1_s + 300_ms);
    CHECK(session.meter().volts() == 5.0);
    session.meter().hold(std::nullopt);
    CHECK_FALSE(session.meter().volts().has_value());
    session.powerOff();
}

TEST_CASE("session: the clip refused with the power on, lifted by power on", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto qa = session.padLead("U1", "14");
    REQUIRE(qa.has_value());
    session.powerOn(1);
    CHECK_FALSE(session.meter().clip({qa->at, Layer::Front, qa->label}));
    CHECK_FALSE(session.meter().continuity().has_value());
    session.powerOff();
    REQUIRE(session.meter().clip({qa->at, Layer::Front, qa->label}));
    session.meter().hold(TestLead{qa->at, Layer::Front, qa->label});
    CHECK(session.meter().continuity() == true);
    session.powerOn(1);
    CHECK_FALSE(session.meter().black().has_value());
    session.powerOff();
    CHECK_FALSE(session.meter().continuity().has_value());
}

TEST_CASE("session: continuity follows a mend", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto qa = session.padLead("U1", "14");
    const auto far = session.padLead("U13", "6");
    REQUIRE(qa.has_value());
    REQUIRE(far.has_value());
    REQUIRE(session.meter().clip({qa->at, Layer::Front, qa->label}));
    CHECK_FALSE(session.meter().continuity().has_value());
    session.meter().hold(TestLead{far->at, Layer::Front, far->label});
    CHECK(session.meter().continuity() == false);
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(session.meter().continuity() == true);
    session.meter().hold(std::nullopt);
    CHECK_FALSE(session.meter().continuity().has_value());
    session.meter().hold(TestLead{far->at, Layer::Front, far->label});
    session.meter().unclip();
    CHECK_FALSE(session.meter().continuity().has_value());
}

TEST_CASE("session: undo every repair", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto qa = session.padLead("U1", "14");
    const auto far = session.padLead("U13", "6");
    REQUIRE(qa.has_value());
    REQUIRE(far.has_value());
    REQUIRE(session.meter().clip({qa->at, Layer::Front, qa->label}));
    session.meter().hold(TestLead{far->at, Layer::Front, far->label});
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    REQUIRE(session.repair(SwapRepair{"U3"}) == RepairResult::Done);
    REQUIRE(session.meter().continuity() == true);
    session.powerOn(1);
    CHECK(session.undoRepairs() == RepairResult::Refused);
    CHECK(session.log().size() == 2);
    session.powerOff();
    CHECK(session.undoRepairs() == RepairResult::Done);
    CHECK(session.log().empty());
    CHECK(session.blobs().empty());
    CHECK_FALSE(session.meter().black().has_value());  // the power on lifted the clip
    REQUIRE(session.meter().clip({qa->at, Layer::Front, qa->label}));
    CHECK(session.meter().continuity() == false);  // the crack back, the red lead kept
    session.powerOn(1);                            // rebuilt with the crack
    CHECK(session.powered());
}

TEST_CASE("session: hooks and leads survive a repair", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const auto far = session.padLead("U13", "6");
    REQUIRE(far.has_value());
    REQUIRE(session.scope().hook("U13", "6") == 0);
    session.meter().hold(TestLead{far->at, Layer::Front, far->label});
    session.powerOn(1);
    RunTo(session, 1_ms);
    CHECK(session.scope().net(0) != PinNet(session.circuit(), "U1", "14"));
    session.powerOff();
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    session.powerOn(1);
    const auto qa = PinNet(session.circuit(), "U1", "14");
    CHECK(session.scope().net(0) == PinNet(session.circuit(), "U13", "6"));
    CHECK(session.scope().net(0) == qa);
    CHECK(session.meter().net() == qa);
    session.powerOff();
}

TEST_CASE("session: netlist without a Board", "[session]") {
    auto loaded = LoadNetlistFile(test::NetlistPath("counter"));
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.machine() == nullptr);
    CHECK(session.drawnBoard() == nullptr);
    CHECK(session.repair(SwapRepair{"U1"}) == RepairResult::Unknown);
    session.probe().touch(ProbeTip{{0, 0}, Layer::Front});
    CHECK_FALSE(session.probe().tip().has_value());
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(session.runner().status().simTime >= 1_ms);
}

namespace {

Session TestPatternSession() {
    auto machine = LoadNetlistMachine(test::NetlistPath("test_pattern"));
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
    REQUIRE(session.meter().clip({qa->at, Layer::Front, qa->label}));
    session.meter().hold(TestLead{far->at, Layer::Front, far->label});
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
    CHECK(session.meter().continuity() == false);
    REQUIRE(session.repair(kH0Wire) == RepairResult::Done);
    CHECK(session.meter().continuity() == true);
    CHECK(session.drawnBoard()->padJoins.empty());
}

TEST_CASE("session: continuity across a cut", "[session]") {
    Session session = TestPatternSession();
    const std::size_t traces = session.drawnBoard()->traces.size();
    LeadsAcrossH0(session);
    CHECK(session.meter().continuity() == true);
    REQUIRE(session.repair(CutRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(session.meter().continuity() == false);
    CHECK(session.drawnBoard()->traces.size() == traces);
    REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
    CHECK(session.meter().continuity() == true);
}

TEST_CASE("session: counter LED", "[session]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("counter"));
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

TEST_CASE("session: FindLedNets", "[session]") {
    // An LED is one lamp; a digit eight, less those the netlist leaves unconnected.
    const auto counter = LoadNetlistFile(test::NetlistPath("counter"));
    REQUIRE(counter.has_value());
    const std::vector<LedNets> one = FindLedNets(**counter);
    REQUIRE(one.size() == 1);
    CHECK(one[0].reference == "D1");
    CHECK(one[0].lamp == 0);
    CHECK(one[0].anodes[0].has_value());
    CHECK(one[0].cathodes[0].has_value());

    const auto stopwatch = LoadNetlistFile(test::NetlistPath("stopwatch"));
    REQUIRE(stopwatch.has_value());
    const std::vector<LedNets> digits = FindLedNets(**stopwatch);
    CHECK(digits.size() == 22);  // DS2's and DS3's decimal points are unconnected
    CHECK(std::ranges::count(digits, "DS1", &LedNets::reference) == 8);
    CHECK(std::ranges::count(digits, "DS2", &LedNets::reference) == 7);
    CHECK(std::ranges::count(digits, "DS3", &LedNets::reference) == 7);

    const auto pattern = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(pattern.has_value());
    CHECK(FindLedNets(**pattern).empty());
}

TEST_CASE("session: chaser with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("chaser"));
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
        REQUIRE(session.analyzer().clip("U5", pin).has_value());
    }
    REQUIRE(session.scope().hook("U5", "8").has_value());
    REQUIRE(session.scope().hook("U6", "8").has_value());
    const auto ser = session.padLead("U5", "1");
    const auto anode = session.padLead("D1", "A");
    REQUIRE(ser.has_value());
    REQUIRE(anode.has_value());
    session.probe().touch(ProbeTip{ser->at, Layer::Front});
    session.meter().hold(TestLead{anode->at, Layer::Front, anode->label});

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
    auto machine = LoadNetlistMachine(test::NetlistPath("stopwatch"));
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
        REQUIRE(session.analyzer().clip(chip, "QD").has_value());
    }
    for (const char* pin : {"QA", "QB", "QC", "QD"}) {
        REQUIRE(session.analyzer().clip("U7", pin).has_value());
    }
    REQUIRE(session.scope().hook("U11", "1Q").has_value());
    REQUIRE(session.scope().hook("U12", "1Y").has_value());
    const auto clock = session.padLead("U12", "1A");
    const auto segment = session.padLead("DS3", "A");
    REQUIRE(clock.has_value());
    REQUIRE(segment.has_value());
    session.probe().touch(ProbeTip{clock->at, Layer::Front});
    session.meter().hold(TestLead{segment->at, Layer::Front, segment->label});

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.leds()[7].brightness > 0.95f);  // DS1's point, on GND
    session.powerOff();
}

TEST_CASE("session: heartbeat with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::HeartbeatNetlistPath());
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.leds().size() == 16);
    CHECK(session.leds()[0].reference == "D1");
    CHECK(session.leds()[15].reference == "D16");

    // 32 LED Nets + 64 Clips + 2 Hooks + the probe = 99 watched, over P6o's former 96
    // (kMaxWatched 128); the meter lead's Net is an LED's.
    for (int bit = 0; bit < 8; ++bit) {
        REQUIRE(session.analyzer().clip("U1", "A" + std::to_string(bit)).has_value());
    }
    ClipTheRest(session);
    CHECK(Clipped(session) == kChannels);
    REQUIRE(session.scope().hook("U1", "PHI0").has_value());
    REQUIRE(session.scope().hook("U1", "SYNC").has_value());
    const auto rw = session.padLead("U1", "RW");
    const auto anode = session.padLead("D16", "A");
    REQUIRE(rw.has_value());
    REQUIRE(anode.has_value());
    session.probe().touch(ProbeTip{rw->at, Layer::Front});
    session.meter().hold(TestLead{anode->at, Layer::Front, anode->label});

    session.powerOn(1, 300_ms);
    session.runner().waitIdle();
    const float a0 = session.leds()[0].brightness;  // A0 at 250 kHz: half lit
    CHECK(a0 > 0.4f);
    CHECK(a0 < 0.6f);
    session.powerOff();
}

TEST_CASE("session: led computer with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("led_computer"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.leds().size() == 8);
    CHECK(session.leds()[0].reference == "D1");
    CHECK(session.leds()[7].reference == "D8");

    // At most 16 LED Nets + 32 Clips + 4 Hooks + the probe = 53 watched (kMaxWatched 128); the
    // meter lead's Net is an LED's.
    REQUIRE(session.analyzer().clipGroup("U1", "A0").has_value());
    REQUIRE(session.analyzer().clipGroup("U1", "D0").has_value());
    REQUIRE(session.analyzer().clipGroup("U1", "PHI0").has_value());
    for (const char* pin : {"RW", "SYNC", "RES_N", "IRQ_N", "NMI_N"}) {
        REQUIRE(session.analyzer().clip("U1", pin).has_value());
    }
    CHECK(Clipped(session) == 32);
    REQUIRE(session.analyzer().setGroupTrigger(0, "8000"));
    session.analyzer().arm();
    REQUIRE(session.scope().hook("U6", "4Y").has_value());  // LATCH_CLK
    REQUIRE(session.scope().hook("U1", "PHI0").has_value());
    REQUIRE(session.scope().hook("U1", "RW").has_value());
    REQUIRE(session.scope().hook("U1", "SYNC").has_value());
    const auto rom = session.padLead("U5", "Y7_N");  // ROM_N
    const auto anode = session.padLead("D1", "A");
    REQUIRE(rom.has_value());
    REQUIRE(anode.has_value());
    session.probe().touch(ProbeTip{rom->at, Layer::Front});
    session.meter().hold(TestLead{anode->at, Layer::Front, anode->label});

    // Latch writes at about 20.03, 144.8 and 269.6 ms: the light on D3 at 300 ms.
    session.powerOn(1, 300_ms);
    session.runner().waitIdle();
    CHECK(session.analyzer().phase() == AnalyzerPhase::Captured);
    for (std::size_t i = 0; i < 8; ++i) {
        INFO(session.leds()[i].reference);
        if (i == 2) {
            CHECK(session.leds()[i].brightness > 0.9f);
        } else {
            CHECK(session.leds()[i].brightness < 0.1f);
        }
    }
    session.powerOff();
}

TEST_CASE("session: bw1 with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("bw1"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    // Six common-cathode digits: 48 lamps. Built from the Board, each unconnected decimal point
    // has its pad's own Net: 13 + 6 lamp Nets.
    REQUIRE(session.leds().size() == 48);

    // 19 lamp Nets + 32 Clips + 4 Hooks + the probe + the meter = 57 watched (kMaxWatched 128).
    REQUIRE(session.analyzer().clipGroup("U1", "A0").has_value());
    REQUIRE(session.analyzer().clipGroup("U1", "D0").has_value());
    REQUIRE(session.analyzer().clipGroup("U1", "PHI0").has_value());
    for (const char* pin : {"RW", "SYNC", "RES_N", "IRQ_N", "NMI_N"}) {
        REQUIRE(session.analyzer().clip("U1", pin).has_value());
    }
    CHECK(Clipped(session) == 32);
    REQUIRE(session.analyzer().setGroupTrigger(0, "1F1F"));  // SCANDS
    session.analyzer().arm();
    REQUIRE(session.scope().hook("U1", "PHI0").has_value());
    REQUIRE(session.scope().hook("U1", "SYNC").has_value());
    REQUIRE(session.scope().hook("U1", "NMI_N").has_value());
    REQUIRE(session.scope().hook("U4", "Y7_N").has_value());  // K7
    const auto row = session.padLead("U24", "Y0_N");          // ROW0
    const auto column = session.padLead("U2", "PA0");
    REQUIRE(row.has_value());
    REQUIRE(column.has_value());
    session.probe().touch(ProbeTip{row->at, Layer::Front});
    session.meter().hold(TestLead{column->at, Layer::Front, column->label});

    // The monitor shows 0200 from about 25 ms: DS1 a '0' (G dark), DS2 a '2' (C, F dark), each
    // digit lit about a sixth of the time.
    session.powerOn(1, 300_ms);
    session.runner().waitIdle();
    CHECK(session.analyzer().phase() == AnalyzerPhase::Captured);
    const auto glow = [&](std::string_view reference, LampShape shape) {
        for (const LedLight& led : session.leds()) {
            if (led.reference == reference && led.shape == shape) {
                return led.brightness;
            }
        }
        FAIL("no lamp " << reference);
        return 0.0f;
    };
    using enum LampShape;
    for (const LampShape lit : {SegmentA, SegmentB, SegmentC, SegmentD, SegmentE, SegmentF}) {
        CHECK(glow("DS1", lit) > 0.08f);
    }
    CHECK(glow("DS1", SegmentG) < 0.02f);
    for (const LampShape lit : {SegmentA, SegmentB, SegmentD, SegmentE, SegmentG}) {
        CHECK(glow("DS2", lit) > 0.08f);
    }
    CHECK(glow("DS2", SegmentC) < 0.02f);
    CHECK(glow("DS2", SegmentF) < 0.02f);
    session.powerOff();
}

TEST_CASE("session: bw2001 with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("bw2001"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.tv() != nullptr);  // the MONITOR connector, separate drives
    CHECK(session.inputs().matrixKeyboards() == std::vector<std::string>{"J5"});
    REQUIRE(session.leds().size() == 1);  // DS1

    // The 6502's address and data buses and its control pins: 32 Clips.
    REQUIRE(session.analyzer().clipGroup("UF3", "A0").has_value());
    REQUIRE(session.analyzer().clipGroup("UF3", "D0").has_value());
    for (const char* pin : {"RW", "PHI2", "RES_N", "IRQ_N", "NMI_N", "SYNC", "RDY", "SO"}) {
        REQUIRE(session.analyzer().clip("UF3", pin).has_value());
    }
    CHECK(Clipped(session) == 32);
    REQUIRE(session.scope().hook("UC9", "QD").has_value());  // CLK1M
    REQUIRE(session.scope().hook("UC5", "2Q").has_value());  // DIS_ON
    REQUIRE(session.scope().hook("UC6", "3Y").has_value());  // VIDEO_ON
    REQUIRE(session.scope().hook("UE5", "4Y").has_value());  // SEL8
    const auto dot = session.padLead("UB2", "QH");
    const auto vcc = session.padLead("UB2", "VCC");
    REQUIRE(dot.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{dot->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    // The TV locks on the power-on picture within about 100 ms.
    session.powerOn(1, 150_ms);
    session.runner().waitIdle();
    TvConsumer* tv = session.tv();
    REQUIRE(tv != nullptr);
    tv->pictures().update();
    CHECK(tv->pictures().front().locked);
    session.powerOff();
}

TEST_CASE("session: bw80 with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("bw80"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.tv() != nullptr);
    CHECK(session.inputs().matrixKeyboards() == std::vector<std::string>{"J3"});
    CHECK(session.inputs().matrixKind("J3") == KeyMatrixKind::Bw80);
    CHECK(session.leds().empty());

    // The Z80's address and data buses and its strobes, the memory side of the data bus (MD0-MD7,
    // the drawing's D0'-D7', at the ROM), then the rest of the Board: 64 Clips (P6o).
    REQUIRE(session.analyzer().clipGroup("IC1", "A0").has_value());
    REQUIRE(session.analyzer().clipGroup("IC1", "D0").has_value());
    for (const char* pin :
         {"M1_N", "MREQ_N", "IORQ_N", "RD_N", "WR_N", "RFSH_N", "HALT_N", "INT_N"}) {
        REQUIRE(session.analyzer().clip("IC1", pin).has_value());
    }
    const auto memory = session.analyzer().clipGroup("IC2", "O0");
    REQUIRE(memory.has_value());
    REQUIRE(memory->group.has_value());
    CHECK(session.analyzer().groups()[*memory->group].channels.size() == 8);
    ClipTheRest(session);
    CHECK(Clipped(session) == kChannels);
    REQUIRE(session.scope().hook("IC18", "2Q").has_value());  // PHI
    REQUIRE(session.scope().hook("IC19", "1Q").has_value());  // SYNC
    REQUIRE(session.scope().hook("IC15", "1Y").has_value());  // NOP_EN
    REQUIRE(session.scope().hook("IC17", "4Y").has_value());  // KBD_N
    const auto dots = session.padLead("IC9", "QH_N");
    const auto vcc = session.padLead("IC9", "VCC");
    REQUIRE(dots.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{dots->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    // The ROM clears its RAM (about 160 ms), then the K cursor screen; the TV locks on it.
    session.powerOn(1, 450_ms);
    session.runner().waitIdle();
    TvConsumer* tv = session.tv();
    REQUIRE(tv != nullptr);
    tv->pictures().update();
    CHECK(tv->pictures().front().locked);
    session.powerOff();
}

TEST_CASE("session: ball with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("ball"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    CHECK(session.leds().empty());
    REQUIRE(session.tv() != nullptr);

    for (const char* pin : {"QA", "QB", "QC", "QD"}) {
        REQUIRE(session.analyzer().clip("U23", pin).has_value());
    }
    REQUIRE(session.analyzer().clip("U26", "1Q").has_value());
    REQUIRE(session.analyzer().clip("U26", "2Q_N").has_value());
    REQUIRE(session.analyzer().clip("U25", "1Y").has_value());
    REQUIRE(session.analyzer().clip("U25", "2Y").has_value());
    REQUIRE(session.scope().hook("U19", "RCO").has_value());
    REQUIRE(session.scope().hook("U24", "3Y").has_value());
    const auto ball = session.padLead("U24", "3Y");
    const auto vcc = session.padLead("U16", "VCC");
    REQUIRE(ball.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{ball->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.tv()->tv().isLocked());
    CHECK(session.tv()->tv().completedFrames() > 40);
    session.powerOff();
}

TEST_CASE("session: tennis with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("tennis"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    CHECK(session.leds().empty());
    REQUIRE(session.tv() != nullptr);
    CHECK(session.inputs().knobs() == std::vector<std::string>{"J3", "J4"});
    CHECK(session.inputs().switches() == std::vector<std::string>{"SW1"});

    for (const char* chip : {"U48", "U49"}) {
        for (const char* pin : {"QA", "QB", "QC", "QD"}) {
            REQUIRE(session.analyzer().clip(chip, pin).has_value());
        }
    }
    REQUIRE(session.scope().hook("U53", "SA").has_value());
    REQUIRE(session.scope().hook("U58", "Y").has_value());
    const auto start = session.padLead("SW1", "1");
    const auto vcc = session.padLead("U48", "VCC");
    REQUIRE(start.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{start->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    CHECK(session.inputs().setKnob("J3", 0.25));
    CHECK(session.inputs().setKnob("J4", 0.75));
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

TEST_CASE("session: typing on a BW80KB, the text queued whole", "[session]") {
    // A netlist-only Session (no Board), as bw_ui runs a bench.
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/bw80_keys.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.inputs().matrixKeyboards() == std::vector<std::string>{"J3"});
    CHECK(session.inputs().matrixKind("J3") == KeyMatrixKind::Bw80);
    CHECK_FALSE(session.inputs().matrixKind("J1").has_value());
    CHECK_FALSE(session.inputs().typeText("J3", U"A"));  // not powered

    // "LIST" at a line's start is the A key alone (row 1, low on the bench): held 60 ms, so
    // still down when the run pauses at 30 ms.
    session.powerOn(1, 30_ms);
    CHECK(session.inputs().typeText("J3", U"LIST"));
    session.runner().waitIdle();
    CHECK(session.runner().status().simTime >= 30_ms);
    CHECK(session.inputs().matrixKeyDown("J3", *Zx80KeyAt(1, 0)));
    CHECK_FALSE(session.inputs().matrixKeyDown("J3", *Zx80KeyAt(1, 1)));
    // A held key through the Session, and all released at power-off.
    session.inputs().setMatrixKeyHeld("J3", *Zx80KeyAt(1, 1), true);
    CHECK(session.inputs().matrixKeyHeld("J3", *Zx80KeyAt(1, 1)));
    session.inputs().setMatrixKeyHeld("J3", 99, true);  // no such key: ignored
    CHECK_FALSE(session.inputs().matrixKeyHeld("J3", 99));
    session.powerOff();
    CHECK_FALSE(session.inputs().matrixKeyHeld("J3", *Zx80KeyAt(1, 1)));
}

TEST_CASE("session: typewriter with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("typewriter"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.tv() != nullptr);
    CHECK(session.inputs().keyboards() == std::vector<std::string>{"J3"});

    for (const char* pin : {"O1", "O2", "O3", "O4", "O5"}) {
        REQUIRE(session.analyzer().clip("U23", pin).has_value());
    }
    REQUIRE(session.analyzer().clip("U16", "OUT").has_value());
    REQUIRE(session.analyzer().clip("U24", "SH_LD_N").has_value());
    REQUIRE(session.analyzer().clip("U24", "QH").has_value());
    REQUIRE(session.scope().hook("U1", "RCO").has_value());
    REQUIRE(session.scope().hook("U15", "2Y").has_value());  // PHI2_N
    const auto pixels = session.padLead("U24", "QH");
    const auto vcc = session.padLead("U23", "VCC");
    REQUIRE(pixels.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{pixels->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    session.powerOn(1, 500_ms);
    CHECK(session.inputs().type("J3", U'A'));
    session.runner().waitIdle();
    CHECK(session.tv()->tv().isLocked());
    CHECK(session.tv()->tv().completedFrames() > 15);
    session.powerOff();
}

TEST_CASE("session: message with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("message"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    CHECK(session.leds().empty());
    REQUIRE(session.tv() != nullptr);

    for (const char* pin : {"O1", "O2", "O3", "O4", "O5"}) {
        REQUIRE(session.analyzer().clip("U15", pin).has_value());
    }
    REQUIRE(session.analyzer().clip("U14", "O0").has_value());
    REQUIRE(session.analyzer().clip("U16", "SH_LD_N").has_value());
    REQUIRE(session.analyzer().clip("U16", "QH").has_value());
    REQUIRE(session.scope().hook("U1", "RCO").has_value());
    REQUIRE(session.scope().hook("U16", "CLK").has_value());
    const auto pixels = session.padLead("U16", "QH");
    const auto vcc = session.padLead("U15", "VCC");
    REQUIRE(pixels.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{pixels->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    session.powerOn(1, 1_s);
    session.runner().waitIdle();
    CHECK(session.tv()->tv().isLocked());
    CHECK(session.tv()->tv().completedFrames() > 40);
    session.powerOff();
}

TEST_CASE("session: knob meter with every instrument", "[session][slow]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("knob"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    REQUIRE(session.leds().size() == 16);  // DS1 A..G and DP, then DS2
    CHECK(session.inputs().knobs() == std::vector<std::string>{"J2"});

    for (const char* pin : {"A", "B", "C", "D"}) {
        REQUIRE(session.analyzer().clip("U12", pin).has_value());
    }
    REQUIRE(session.analyzer().clip("J2", "1").has_value());
    REQUIRE(session.analyzer().clip("J2", "2").has_value());
    REQUIRE(session.analyzer().clip("U6", "2Y").has_value());
    REQUIRE(session.analyzer().clip("U6", "1Y").has_value());
    REQUIRE(session.scope().hook("U8", "QA").has_value());
    REQUIRE(session.scope().hook("U5", "QB").has_value());
    const auto pulse = session.padLead("J2", "2");
    const auto vcc = session.padLead("J2", "3");
    REQUIRE(pulse.has_value());
    REQUIRE(vcc.has_value());
    session.probe().touch(ProbeTip{pulse->at, Layer::Front});
    session.meter().hold(TestLead{vcc->at, Layer::Front, vcc->label});

    CHECK(session.inputs().setKnob("J2", 0.42));
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
    auto loaded = LoadNetlistFile(test::NetlistPath("dice"));
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.inputs().switches() == std::vector<std::string>{"SW1"});
    session.inputs().setPressed("SW1", true);
    CHECK_FALSE(session.inputs().pressed("SW1"));

    session.powerOn(1);
    session.inputs().setPressed("SW1", true);
    CHECK(session.inputs().pressed("SW1"));
    const Switch* button = SwitchOf(session.circuit(), "SW1");
    REQUIRE(button != nullptr);
    session.pause();  // the press reaches the Part at the Runner's next slice boundary
    session.runner().waitIdle();
    CHECK(button->pressed());
    session.inputs().setPressed("SW9", true);  // not a Button: remembered, does nothing
    session.inputs().setPressed("SW9", false);
    session.powerOff();
    session.runner().waitIdle();
    CHECK_FALSE(session.inputs().pressed("SW1"));
    CHECK_FALSE(button->pressed());
}

TEST_CASE("session: rolling the dice", "[session]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("dice"));
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
    session.inputs().setPressed("SW1", true);
    session.runner().resumeUntil(70_ms);
    session.runner().waitIdle();
    CHECK(std::abs(brightness("D4") - 0.5f) < 0.02f);      // ODD_N: 1, 3, 5
    CHECK(std::abs(brightness("D3") - 1.0f / 6) < 0.02f);  // SIX_N: 6
    CHECK(std::abs(brightness("D1") - 5.0f / 6) < 0.02f);  // TWO_N: 2-6

    session.inputs().setPressed("SW1", false);
    session.runner().resumeUntil(130_ms);
    session.runner().waitIdle();
    for (const LedLight& led : session.leds()) {
        INFO(led.reference);
        CHECK((led.brightness == 0 || led.brightness == 1));
    }
    session.powerOff();
}

TEST_CASE("session: dice rolls both odd and even faces", "[session]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("dice"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    const auto runTo = [&](Time time) {
        session.runner().resumeUntil(time);
        session.runner().waitIdle();
    };
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    bool odd = false;
    bool even = false;
    Time t = 1_ms;
    for (int roll = 0; roll < 16 && !(odd && even); ++roll) {
        session.inputs().setPressed("SW1", true);
        runTo(t += 3_ms);
        session.inputs().setPressed("SW1", false);
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
    CHECK(session.inputs().knobs() == std::vector<std::string>{"J2"});
    CHECK_FALSE(session.inputs().setKnob("J9", 0.5));
    CHECK_FALSE(session.inputs().setKnob("U1", 0.5));
    CHECK(session.inputs().knob("J2") == 0.0);
    CHECK(session.inputs().setKnob("J2", 1.5));
    CHECK(session.inputs().knob("J2") == 1.0);
    CHECK(session.inputs().setKnob("J2", 0.25));  // with the power off
    session.runner().waitIdle();                  // the Part's change is the Runner's to apply
    CHECK(paddle->knob() == 0.25);
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(paddle->knob() == 0.25);
    session.powerOff();
    session.powerOn(2, 1_ms);
    session.runner().waitIdle();
    CHECK(session.inputs().knob("J2") == 0.25);
    CHECK(paddle->knob() == 0.25);
}

TEST_CASE("session: knob kept across a Repair", "[session]") {
    Session session = PuzzleSession("dead_knob");
    CHECK(session.inputs().setKnob("J2", 0.3));
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
    CHECK(session.inputs().knob("J2") == 0.3);
    session.powerOff();
}

TEST_CASE("session: typing", "[session]") {
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/ram_bench.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.inputs().keyboards() == std::vector<std::string>{"J1"});
    CHECK_FALSE(session.inputs().type("J1", U'H'));  // not powered

    session.powerOn(1, 50_ms);
    CHECK_FALSE(session.inputs().type("J9", U'H'));
    CHECK_FALSE(session.inputs().type("SW1", U'H'));
    CHECK_FALSE(session.inputs().type("J1", U'{'));
    CHECK(session.inputs().type("J1", U'h'));
    CHECK(session.inputs().type("J1", U'I'));
    CHECK(session.inputs().typed("J1") == "HI");
    session.runner().waitIdle();
    // U4 starts at a random count: HI is at two consecutive addresses of the 16.
    const auto& low = static_cast<const Ram2114&>(*session.circuit().components.at("U2"));
    const auto& high = static_cast<const Ram2114&>(*session.circuit().components.at("U3"));
    const auto word = [&](std::size_t a) {
        return low.word(a % 16) | (high.word(a % 16) & 0b111) << 4;
    };
    bool found = false;
    for (std::size_t a = 0; a < 16; ++a) {
        found |= word(a) == 'H' && word(a + 1) == 'I';
    }
    CHECK(found);

    for (int i = 0; i < 20; ++i) {
        CHECK(session.inputs().type("J1", U'A' + i));
    }
    CHECK(session.inputs().typed("J1") == "EFGHIJKLMNOPQRST");
    session.powerOff();
    CHECK_FALSE(session.inputs().type("J1", U'A'));
    session.powerOn(2);
    CHECK(session.inputs().typed("J1").empty());
    session.powerOff();
    session.runner().waitIdle();
}

TEST_CASE("session: typing control codes", "[session]") {
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/ram_bench.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK_FALSE(session.inputs().typeCode("J1", 0x01));  // not powered

    session.powerOn(1, 50_ms);
    CHECK(session.inputs().typeCode("J1", 0x00));
    CHECK(session.inputs().typeCode("J1", 0x01));
    CHECK(session.inputs().typeCode("J1", 0x1B));
    CHECK(session.inputs().typeCode("J1", 0x5F));
    CHECK_FALSE(session.inputs().typeCode("J1", 0x60));
    CHECK_FALSE(session.inputs().typeCode("J1", 0x7F));
    CHECK_FALSE(session.inputs().typeCode("J9", 0x01));
    CHECK(session.inputs().typed("J1") == std::string("\x00\x01\x1B\x5F", 4));
    CHECK(session.inputs().typeCode("J1", 'A'));
    CHECK(session.inputs().typeCode("J1", 0x07));
    CHECK(session.inputs().typeCode("J1", 'B'));
    session.runner().waitIdle();
    // U4 starts at a random count: A, BEL, B are at three consecutive addresses of the 16.
    const auto& low = static_cast<const Ram2114&>(*session.circuit().components.at("U2"));
    const auto& high = static_cast<const Ram2114&>(*session.circuit().components.at("U3"));
    const auto word = [&](std::size_t a) {
        return low.word(a % 16) | (high.word(a % 16) & 0b111) << 4;
    };
    bool found = false;
    for (std::size_t a = 0; a < 16; ++a) {
        found |= word(a) == 'A' && word(a + 1) == 0x07 && word(a + 2) == 'B';
    }
    CHECK(found);
    session.powerOff();
    session.runner().waitIdle();
}

TEST_CASE("session: switch keys on the switch bench", "[session]") {
    auto loaded =
        LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/switch_bench.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    CHECK(session.inputs().switches().size() == 17);
    const auto keys = session.inputs().switchKeys();
    REQUIRE(keys.size() == 17);
    CHECK(keys[0].reference == "SW1");
    CHECK(keys[0].pc == "0");
    CHECK(keys[10].pc == "A");
    CHECK(keys[16].reference == "SW17");
    CHECK(keys[16].pc == "Space");

    // A key held through the Session: the inverter on its column goes high on its row's scans.
    session.setSpeed(Speed::Max);
    session.powerOn(1, 1_ms);
    session.inputs().setPressed("SW1", true);  // key 0: row 0, column 0
    session.runner().waitIdle();
    const Switch* key = SwitchOf(session.circuit(), "SW1");
    REQUIRE(key != nullptr);
    CHECK(key->closed());
    session.powerOff();
    session.runner().waitIdle();
    CHECK_FALSE(key->pressed());
}

TEST_CASE("session: completed follows the Repairs", "[session]") {
    Session session = PuzzleSession("broken_trace");
    CHECK(session.puzzle());
    CHECK_FALSE(session.completed());
    CHECK(session.powerOns() == 0);
    session.powerOn(1);
    CHECK(session.powerOns() == 1);
    CHECK(session.repair(WireRepair{{"U1", "14"}, {"U13", "6"}}) == RepairResult::Refused);
    session.powerOff();
    REQUIRE(session.repair(WireRepair{{"U1", "14"}, {"U13", "6"}}) == RepairResult::Done);
    CHECK(session.completed());
    session.powerOn(1);
    CHECK(session.powerOns() == 2);
    session.powerOff();
    REQUIRE(session.undoRepairs() == RepairResult::Done);
    CHECK_FALSE(session.completed());
    CHECK(session.puzzle());
}

TEST_CASE("session: the working Machine is no Puzzle", "[session]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("counter"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    const Session session(std::move(*machine), std::move(*built));
    CHECK(session.completed());
    CHECK_FALSE(session.puzzle());
}
