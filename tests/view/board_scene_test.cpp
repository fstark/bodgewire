#include "view/board_scene.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "board/copper.hpp"
#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "view/viewport.hpp"

using namespace bw;

namespace {

struct Fixture {
    std::unique_ptr<LoadedCircuit> design;
    Board pristine;
};

Fixture Load(const std::string& name) {
    auto design = LoadNetlistFile(std::string(BW_NETLIST_DIR) + "/" + name + ".bwn");
    REQUIRE(design.has_value());
    auto board =
        ReadKicadBoardFile(std::string(BW_BOARD_DIR) + "/" + name + "/" + name + ".kicad_pcb");
    REQUIRE(board.has_value());
    return {std::move(*design), std::move(*board)};
}

BoardScene Scene(const Fixture& f, std::span<const Crack> cracks = {}) {
    return BoardScene(CrackBoard(f.pristine, cracks), f.pristine, *f.design);
}

bool OnAnyPadOrVia(const Board& board, Point p) {
    for (const BoardComponent& component : board.components) {
        for (const Pad& pad : component.pads) {
            if (Covers(pad, p)) {
                return true;
            }
        }
    }
    for (const Via& via : board.vias) {
        if (Covers(via, p)) {
            return true;
        }
    }
    return false;
}

bool OnTrace(const Board& board, Layer layer, Point p) {
    for (const Trace& trace : board.traces) {
        if (trace.layer == layer && Covers(trace, p)) {
            return true;
        }
    }
    return false;
}

std::size_t ComponentIndex(const Board& board, std::string_view reference) {
    for (std::size_t k = 0; k < board.components.size(); ++k) {
        if (board.components[k].reference == reference) {
            return k;
        }
    }
    FAIL("no component " << reference);
    return 0;
}

std::size_t PadIndex(const BoardComponent& component, std::string_view number) {
    for (std::size_t p = 0; p < component.pads.size(); ++p) {
        if (component.pads[p].number == number) {
            return p;
        }
    }
    FAIL("no pad " << number);
    return 0;
}

double Distance(Point a, Point b) {
    return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

// Any pad or via centre within `nm` of p.
bool NearPadOrVia(const Board& board, Point p, double nm) {
    for (const BoardComponent& component : board.components) {
        for (const Pad& pad : component.pads) {
            if (Distance(pad.position, p) <= nm) {
                return true;
            }
        }
    }
    for (const Via& via : board.vias) {
        if (Distance(via.position, p) <= nm) {
            return true;
        }
    }
    return false;
}

// Distance from p to the centreline of a straight Trace.
double CentrelineDistance(const Trace& trace, Point p) {
    const double dx = static_cast<double>(trace.end.x - trace.start.x);
    const double dy = static_cast<double>(trace.end.y - trace.start.y);
    const double length2 = dx * dx + dy * dy;
    const double t = length2 == 0 ? 0
                                  : std::clamp((static_cast<double>(p.x - trace.start.x) * dx +
                                                static_cast<double>(p.y - trace.start.y) * dy) /
                                                   length2,
                                               0.0, 1.0);
    return std::hypot(static_cast<double>(trace.start.x) + dx * t - static_cast<double>(p.x),
                      static_cast<double>(trace.start.y) + dy * t - static_cast<double>(p.y));
}

// No Trace on `layer` other than `except` has its edge within `nm` of p.
bool ClearOfTraces(const Board& board, Layer layer, Point p, double nm,
                   std::optional<std::size_t> except = std::nullopt) {
    for (std::size_t t = 0; t < board.traces.size(); ++t) {
        const Trace& trace = board.traces[t];
        if (t == except || trace.layer != layer) {
            continue;
        }
        if (trace.mid ||
            CentrelineDistance(trace, p) - static_cast<double>(trace.width) / 2 <= nm) {
            if (!trace.mid || Distance(trace.start, p) < 20'000'000) {
                return false;
            }
        }
    }
    return true;
}

PickReach Reach(double pixelsPerMm) {
    return {static_cast<std::int64_t>(12 / pixelsPerMm * 1e6),
            static_cast<std::int64_t>(10 / pixelsPerMm * 1e6)};
}

}  // namespace

TEST_CASE("view: outline of every Board is a rectangle", "[view]") {
    for (const std::string name : {"counter", "test_pattern", "chaser", "dice", "stopwatch", "ball",
                                   "knob", "tennis", "message"}) {
        const Fixture f = Load(name);
        const BoardScene scene = Scene(f);
        REQUIRE(scene.outline().size() == 4);
        const Bounds b = scene.bounds();
        CHECK(b.min.x < b.max.x);
        CHECK(b.min.y < b.max.y);
        for (const Point p : scene.outline()) {
            CHECK((p.x == b.min.x || p.x == b.max.x));
            CHECK((p.y == b.min.y || p.y == b.max.y));
        }
        for (std::size_t k = 0; k < f.pristine.components.size(); ++k) {
            CHECK(scene.courtyard(k).size() >= 3);
        }
    }
}

TEST_CASE("view: pick", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();
    const auto hitAt = [&](Point p, Layer active) -> std::optional<Hit> {
        const auto picked = scene.pick(p, active, {});
        return picked ? std::optional(picked->hit) : std::nullopt;
    };

    const Point pad = board.components[0].pads[0].position;
    CHECK(hitAt(pad, Layer::Front) == Hit{HitKind::Pad, 0, 0});
    CHECK(hitAt(pad, Layer::Back) == Hit{HitKind::Pad, 0, 0});

    bool viaChecked = false;
    for (std::size_t v = 0; v < board.vias.size() && !viaChecked; ++v) {
        bool nearPad = false;
        for (const BoardComponent& component : board.components) {
            for (const Pad& p : component.pads) {
                nearPad = nearPad || Distance(p.position, board.vias[v].position) < 3'000'000;
            }
        }
        if (!nearPad) {
            CHECK(hitAt(board.vias[v].position, Layer::Back) == Hit{HitKind::Via, 0, v});
            viaChecked = true;
        }
    }
    CHECK(viaChecked);

    bool traceChecked = false;
    for (std::size_t t = 0; t < board.traces.size() && !traceChecked; ++t) {
        const Trace& trace = board.traces[t];
        const Point m{(trace.start.x + trace.end.x) / 2, (trace.start.y + trace.end.y) / 2};
        if (trace.layer != Layer::Front || trace.mid || NearPadOrVia(board, m, 3'000'000) ||
            OnTrace(board, Layer::Back, m)) {
            continue;
        }
        const auto front = scene.pick(m, Layer::Front, {});
        REQUIRE(front.has_value());
        CHECK(front->hit.kind == HitKind::Trace);
        CHECK(front->layer == Layer::Front);
        CHECK(Covers(board.traces[front->hit.index], m));
        const auto back = scene.pick(m, Layer::Back, {});
        REQUIRE(back.has_value());
        CHECK(back->hit == front->hit);
        CHECK(back->layer == Layer::Front);
        traceChecked = true;
    }
    CHECK(traceChecked);

    bool courtyardChecked = false;
    for (std::size_t k = 0; k < board.components.size() && !courtyardChecked; ++k) {
        const auto courtyard = scene.courtyard(k);
        Point centre{0, 0};
        for (const Point p : courtyard) {
            centre = {centre.x + p.x, centre.y + p.y};
        }
        const auto n = static_cast<std::int64_t>(courtyard.size());
        centre = {centre.x / n, centre.y / n};
        const Point c = courtyard[0];
        const auto inward = [](std::int64_t from, std::int64_t to) {
            return from < to ? from + 200'000 : from - 200'000;
        };
        const Point p{inward(c.x, centre.x), inward(c.y, centre.y)};
        if (NearPadOrVia(board, p, 1'300'000) || OnTrace(board, Layer::Front, p) ||
            OnTrace(board, Layer::Back, p)) {
            continue;
        }
        const auto hit = hitAt(p, Layer::Front);
        REQUIRE(hit.has_value());
        CHECK(hit->kind == HitKind::Component);
        courtyardChecked = true;
    }
    CHECK(courtyardChecked);

    const Bounds b = scene.bounds();
    CHECK_FALSE(hitAt({b.min.x - 5'000'000, b.min.y - 5'000'000}, Layer::Front));

    bool padWins = false;
    for (const Trace& trace : board.traces) {
        if (trace.layer == Layer::Front && OnAnyPadOrVia(board, trace.start)) {
            const auto hit = hitAt(trace.start, Layer::Front);
            REQUIRE(hit.has_value());
            CHECK((hit->kind == HitKind::Pad || hit->kind == HitKind::Via));
            padWins = true;
            break;
        }
    }
    CHECK(padWins);
}

TEST_CASE("view: pick snapping", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();
    const std::size_t u1 = ComponentIndex(board, "U1");
    const auto& u1Pads = board.components[u1].pads;

    Viewport viewport;
    viewport.fit(scene.bounds(), {850, 780});
    const double fittedScale = viewport.pixelsPerMm();
    for (const double scale : {fittedScale, 100.0}) {
        INFO("px/mm " << scale);
        const PickReach reach = Reach(scale);

        // 0.5 mm off a pad, along a Trace leaving it: the pad.
        bool padOverTrace = false;
        for (std::size_t p = 0; p < u1Pads.size() && !padOverTrace; ++p) {
            for (const Trace& trace : board.traces) {
                if (trace.mid || trace.layer != Layer::Front || trace.start != u1Pads[p].position) {
                    continue;
                }
                const double length = Distance(trace.start, trace.end);
                if (length < 1'000'000) {
                    continue;
                }
                const double k = 500'000 / length;
                const Point at{
                    trace.start.x +
                        std::llround(static_cast<double>(trace.end.x - trace.start.x) * k),
                    trace.start.y +
                        std::llround(static_cast<double>(trace.end.y - trace.start.y) * k)};
                const auto picked = scene.pick(at, Layer::Front, reach);
                REQUIRE(picked.has_value());
                CHECK(picked->hit == Hit{HitKind::Pad, u1, p});
                CHECK(picked->contact == u1Pads[p].position);
                padOverTrace = true;
                break;
            }
        }
        CHECK(padOverTrace);

        // Between two neighbouring pads: the nearer.
        const std::size_t pin1 = PadIndex(board.components[u1], "1");
        const std::size_t pin2 = PadIndex(board.components[u1], "2");
        const Point a = u1Pads[pin1].position;
        const Point b = u1Pads[pin2].position;
        const Point nearA{a.x + (b.x - a.x) * 4 / 10, a.y + (b.y - a.y) * 4 / 10};
        const Point nearB{a.x + (b.x - a.x) * 6 / 10, a.y + (b.y - a.y) * 6 / 10};
        CHECK(scene.pick(nearA, Layer::Front, reach)->hit == Hit{HitKind::Pad, u1, pin1});
        CHECK(scene.pick(nearB, Layer::Front, reach)->hit == Hit{HitKind::Pad, u1, pin2});

        // 8 px beside a thin Trace on bare board: that Trace, contact on its copper.
        const double eightPx = 8 / scale * 1e6;
        bool beside = false;
        for (std::size_t t = 0; t < board.traces.size() && !beside; ++t) {
            const Trace& trace = board.traces[t];
            if (trace.mid || trace.layer != Layer::Front || trace.width > 250'000) {
                continue;
            }
            const double dx = static_cast<double>(trace.end.x - trace.start.x);
            const double dy = static_cast<double>(trace.end.y - trace.start.y);
            const double length = std::hypot(dx, dy);
            if (length < 4'000'000) {
                continue;
            }
            const double offset = static_cast<double>(trace.width) / 2 + eightPx;
            const Point at{(trace.start.x + trace.end.x) / 2 + std::llround(-dy / length * offset),
                           (trace.start.y + trace.end.y) / 2 + std::llround(dx / length * offset)};
            if (NearPadOrVia(board, at, 3'000'000 + eightPx) ||
                !ClearOfTraces(board, Layer::Front, at, 2 * eightPx, t) ||
                !ClearOfTraces(board, Layer::Back, at, 2 * eightPx)) {
                continue;
            }
            const auto picked = scene.pick(at, Layer::Front, reach);
            REQUIRE(picked.has_value());
            CHECK(picked->hit == Hit{HitKind::Trace, 0, t});
            CHECK(Covers(trace, picked->contact));
            beside = true;
        }
        CHECK(beside);

        // B.Cu-only spot with F.Cu active: the B.Cu Trace.
        bool backOnly = false;
        for (std::size_t t = 0; t < board.traces.size() && !backOnly; ++t) {
            const Trace& trace = board.traces[t];
            if (trace.mid || trace.layer != Layer::Back) {
                continue;
            }
            const Point m{(trace.start.x + trace.end.x) / 2, (trace.start.y + trace.end.y) / 2};
            if (NearPadOrVia(board, m, 3'000'000 + eightPx) ||
                !ClearOfTraces(board, Layer::Front, m, 2 * eightPx) ||
                !ClearOfTraces(board, Layer::Back, m, 2 * eightPx, t)) {
                continue;
            }
            const auto picked = scene.pick(m, Layer::Front, reach);
            REQUIRE(picked.has_value());
            CHECK(picked->hit == Hit{HitKind::Trace, 0, t});
            CHECK(picked->layer == Layer::Back);
            backOnly = true;
        }
        CHECK(backOnly);

        const Bounds bounds = scene.bounds();
        CHECK_FALSE(scene.pick({bounds.min.x - 20'000'000, bounds.min.y - 20'000'000}, Layer::Front,
                               reach));
    }
}

TEST_CASE("view: scene accessors", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const auto u6 = scene.componentIndex("U6");
    REQUIRE(u6.has_value());
    CHECK(scene.board().components[*u6].reference == "U6");
    CHECK_FALSE(scene.componentIndex("U99"));
    CHECK(scene.partName(*u6) == "74LS20");

    const std::size_t u1 = *scene.componentIndex("U1");
    const auto qa = scene.padIndex(u1, "QA");
    REQUIRE(qa.has_value());
    CHECK(qa == scene.padIndex(u1, "14"));
    CHECK(scene.pinName(u1, *qa) == "QA");
    CHECK_FALSE(scene.padIndex(u1, "XX"));
    CHECK(scene.powerPin(u1, *scene.padIndex(u1, "VCC")));
    CHECK(scene.powerPin(u1, *scene.padIndex(u1, "GND")));
    CHECK_FALSE(scene.powerPin(u1, *qa));

    const Fixture counter = Load("counter");
    const BoardScene counterScene = Scene(counter);
    const auto r1 = counterScene.componentIndex("R1");
    REQUIRE(r1.has_value());
    CHECK(counterScene.partName(*r1) == "R");
    CHECK(counterScene.pinName(*r1, 0).empty());
}

TEST_CASE("board scene: componentAt", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();
    const std::size_t u1 = ComponentIndex(board, "U1");

    CHECK(scene.componentAt(
              board.components[u1].pads[PadIndex(board.components[u1], "14")].position) == u1);

    Point centre{0, 0};
    const auto courtyard = scene.courtyard(u1);
    for (const Point p : courtyard) {
        centre = {centre.x + p.x, centre.y + p.y};
    }
    const auto n = static_cast<std::int64_t>(courtyard.size());
    centre = {centre.x / n, centre.y / n};
    REQUIRE_FALSE(OnAnyPadOrVia(board, centre));
    CHECK(scene.componentAt(centre) == u1);

    const Bounds b = scene.bounds();
    CHECK_FALSE(scene.componentAt({b.min.x - 1'000'000, b.min.y - 1'000'000}));
}

TEST_CASE("view: highlight crosses the broken_trace crack", "[view]") {
    const Fixture f = Load("test_pattern");
    const Crack crack = H0Crack();
    const BoardScene scene = Scene(f, {&crack, 1});
    const Board& board = scene.board();
    REQUIRE(board.traces.size() == f.pristine.traces.size() + 1);
    const NetId h0 = f.design->nets.at("H0");
    CHECK(scene.traceNet(board.traces.size() - 2) == h0);
    CHECK(scene.traceNet(board.traces.size() - 1) == h0);
    const auto hit = scene.pick(H0Crack().at, Layer::Front, {});
    // A B.Cu Trace crosses under the crack: only the Front copper is cut.
    CHECK((!hit || hit->hit.kind != HitKind::Trace || hit->layer != Layer::Front));
}

TEST_CASE("view: every Trace and via has a design Net", "[view]") {
    for (const std::string name : {"counter", "test_pattern", "chaser", "dice", "stopwatch", "ball",
                                   "knob", "tennis", "message"}) {
        const Fixture f = Load(name);
        const BoardScene scene = Scene(f);
        for (std::size_t t = 0; t < scene.board().traces.size(); ++t) {
            INFO(name << " trace " << t);
            CHECK(scene.traceNet(t).has_value());
        }
        for (std::size_t v = 0; v < scene.board().vias.size(); ++v) {
            INFO(name << " via " << v);
            CHECK(scene.viaNet(v).has_value());
        }
    }
}

TEST_CASE("view: hover text", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();

    const std::size_t u1 = ComponentIndex(board, "U1");
    const std::size_t pin14 = PadIndex(board.components[u1], "14");
    CHECK(scene.hoverText({HitKind::Pad, u1, pin14}) == "U1 pin 14 QA · H0");
    CHECK(scene.hoverText({HitKind::Component, ComponentIndex(board, "U5"), 0}) == "U5 74LS161");

    const auto trace = scene.traceNet(0);
    REQUIRE(trace.has_value());
    CHECK(scene.hoverText({HitKind::Trace, 0, 0}) == scene.netName(*trace));

    bool unconnected = false;
    for (std::size_t k = 0; k < board.components.size() && !unconnected; ++k) {
        for (std::size_t p = 0; p < board.components[k].pads.size(); ++p) {
            if (!scene.padNet(k, p)) {
                CHECK(scene.hoverText({HitKind::Pad, k, p}).ends_with(" · —"));
                unconnected = true;
                break;
            }
        }
    }
    CHECK(unconnected);

    const Fixture counter = Load("counter");
    const BoardScene counterScene = Scene(counter);
    const std::size_t r1 = ComponentIndex(counterScene.board(), "R1");
    CHECK(counterScene.hoverText({HitKind::Pad, r1, 0}).starts_with("R1 pin 1 · "));
}

TEST_CASE("leads: attached on pads only", "[view]") {
    CHECK(AcceptsLead(LeadKind::Held, HitKind::Pad));
    CHECK(AcceptsLead(LeadKind::Held, HitKind::Via));
    CHECK(AcceptsLead(LeadKind::Held, HitKind::Trace));
    CHECK_FALSE(AcceptsLead(LeadKind::Held, HitKind::Component));
    CHECK(AcceptsLead(LeadKind::Attached, HitKind::Pad));
    CHECK_FALSE(AcceptsLead(LeadKind::Attached, HitKind::Via));
    CHECK_FALSE(AcceptsLead(LeadKind::Attached, HitKind::Trace));
    CHECK_FALSE(AcceptsLead(LeadKind::Attached, HitKind::Component));
}

TEST_CASE("leads: labels", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();
    const std::size_t u1 = ComponentIndex(board, "U1");
    const Hit qa{HitKind::Pad, u1, PadIndex(board.components[u1], "14")};
    CHECK(LeadLabel(scene, qa, Layer::Front) == "U1.14 QA leg");
    CHECK(LeadLabel(scene, qa, Layer::Back) == "U1.14 QA pad");
    CHECK(LeadLabel(scene, {HitKind::Trace, 0, 0}, Layer::Front) == "Trace");
    REQUIRE_FALSE(board.vias.empty());
    CHECK(LeadLabel(scene, {HitKind::Via, 0, 0}, Layer::Front) == "via");

    const Fixture counter = Load("counter");
    const BoardScene counterScene = Scene(counter);
    const std::size_t r1 = ComponentIndex(counterScene.board(), "R1");
    CHECK(LeadLabel(counterScene, {HitKind::Pad, r1, 0}, Layer::Front) == "R1.1");
}

TEST_CASE("leads: contact side on chip pads only", "[view]") {
    const Fixture f = Load("counter");
    const BoardScene scene = Scene(f);
    const std::size_t u1 = ComponentIndex(scene.board(), "U1");
    const std::size_t r1 = ComponentIndex(scene.board(), "R1");
    CHECK(ContactSide(scene, {HitKind::Pad, u1, 0}, Layer::Front) == "leg");
    CHECK(ContactSide(scene, {HitKind::Pad, u1, 0}, Layer::Back) == "pad");
    CHECK(ContactSide(scene, {HitKind::Pad, r1, 0}, Layer::Front).empty());
    CHECK(ContactSide(scene, {HitKind::Trace, 0, 0}, Layer::Front).empty());
    CHECK(scene.chip(u1));
    CHECK_FALSE(scene.chip(r1));
}

TEST_CASE("view: bodyAt is a courtyard away from the pads", "[view]") {
    const Fixture f = Load("dice");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();
    const std::size_t sw1 = ComponentIndex(board, "SW1");
    REQUIRE(board.components[sw1].pads.size() == 2);
    const Point a = board.components[sw1].pads[0].position;
    const Point b = board.components[sw1].pads[1].position;
    CHECK(scene.bodyAt({(a.x + b.x) / 2, (a.y + b.y) / 2}) == sw1);
    CHECK_FALSE(scene.bodyAt(a).has_value());
    CHECK_FALSE(scene.bodyAt({scene.bounds().min.x - 1'000'000, a.y}).has_value());
    // SW1's courtyard comes from its F.CrtYd lines: taller than the pads.
    const auto courtyard = scene.courtyard(sw1);
    const auto [minY, maxY] = std::ranges::minmax(courtyard, {}, &Point::y);
    CHECK(maxY.y - minY.y >= 3'500'000);
}
