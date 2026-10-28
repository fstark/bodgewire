#include "view/board_scene.hpp"

#include <memory>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "board/copper.hpp"
#include "board/kicad_board.hpp"
#include "machine/crack.hpp"

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

BoardScene Scene(const Fixture& f, std::span<const CrackDecl> cracks = {}) {
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

}  // namespace

TEST_CASE("view: outline of both Boards is a rectangle", "[view]") {
    for (const std::string name : {"counter", "test_pattern"}) {
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

TEST_CASE("view: hit test", "[view]") {
    const Fixture f = Load("test_pattern");
    const BoardScene scene = Scene(f);
    const Board& board = scene.board();

    const Point pad = board.components[0].pads[0].position;
    CHECK(scene.hitTest(pad, Layer::Front) == Hit{HitKind::Pad, 0, 0});
    CHECK(scene.hitTest(pad, Layer::Back) == Hit{HitKind::Pad, 0, 0});

    REQUIRE_FALSE(board.vias.empty());
    CHECK(scene.hitTest(board.vias[0].position, Layer::Back) == Hit{HitKind::Via, 0, 0});

    bool traceChecked = false;
    for (std::size_t t = 0; t < board.traces.size() && !traceChecked; ++t) {
        const Trace& trace = board.traces[t];
        const Point m{(trace.start.x + trace.end.x) / 2, (trace.start.y + trace.end.y) / 2};
        if (trace.layer != Layer::Front || trace.mid || OnAnyPadOrVia(board, m) ||
            OnTrace(board, Layer::Back, m)) {
            continue;
        }
        const auto hit = scene.hitTest(m, Layer::Front);
        REQUIRE(hit.has_value());
        CHECK(hit->kind == HitKind::Trace);
        CHECK(Covers(board.traces[hit->index], m));
        const auto back = scene.hitTest(m, Layer::Back);
        CHECK((!back || back->kind == HitKind::Component));
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
        if (OnAnyPadOrVia(board, p) || OnTrace(board, Layer::Front, p)) {
            continue;
        }
        const auto hit = scene.hitTest(p, Layer::Front);
        REQUIRE(hit.has_value());
        CHECK(hit->kind == HitKind::Component);
        courtyardChecked = true;
    }
    CHECK(courtyardChecked);

    const Bounds b = scene.bounds();
    CHECK_FALSE(scene.hitTest({b.min.x - 1'000'000, b.min.y - 1'000'000}, Layer::Front));

    bool padWins = false;
    for (const Trace& trace : board.traces) {
        if (trace.layer == Layer::Front && OnAnyPadOrVia(board, trace.start)) {
            const auto hit = scene.hitTest(trace.start, Layer::Front);
            REQUIRE(hit.has_value());
            CHECK((hit->kind == HitKind::Pad || hit->kind == HitKind::Via));
            padWins = true;
            break;
        }
    }
    CHECK(padWins);
}

TEST_CASE("view: highlight crosses the broken_trace crack", "[view]") {
    const Fixture f = Load("test_pattern");
    const CrackDecl crack{1, Layer::Front, kH0Crack};
    const BoardScene scene = Scene(f, {&crack, 1});
    const Board& board = scene.board();
    REQUIRE(board.traces.size() == f.pristine.traces.size() + 1);
    const NetId h0 = f.design->nets.at("H0");
    CHECK(scene.traceNet(board.traces.size() - 2) == h0);
    CHECK(scene.traceNet(board.traces.size() - 1) == h0);
    const auto hit = scene.hitTest(kH0Crack, Layer::Front);
    CHECK((!hit || hit->kind != HitKind::Trace));
}

TEST_CASE("view: every Trace and via has a design Net", "[view]") {
    for (const std::string name : {"counter", "test_pattern"}) {
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
