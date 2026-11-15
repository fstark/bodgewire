#include "board/kicad_pcb_writer.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "board/kicad_board.hpp"
#include "board/sexpr.hpp"
#include "parts/catalogue.hpp"

using namespace bw;

namespace {

const FootprintLibrary& Footprints() {
    static const FootprintLibrary library = [] {
        std::vector<std::string_view> packages;
        for (const PartInfo& part : AllParts()) {
            packages.push_back(part.package);
        }
        auto loaded = LoadFootprints(BW_FOOTPRINT_DIR, packages);
        REQUIRE(loaded.has_value());
        return std::move(*loaded);
    }();
    return library;
}

struct Written {
    std::unique_ptr<LoadedCircuit> circuit;
    Placement placement;
    std::string text;
};

Written Write(std::unique_ptr<LoadedCircuit> circuit, std::string_view rules,
              PlaceOptions options = {.tries = 1}) {
    const auto parsed = ParsePlaceRules(rules, "t.place");
    REQUIRE(parsed.has_value());
    auto placement = PlaceBoard(*circuit, *parsed, "t.place", Footprints(), options);
    REQUIRE(placement.has_value());
    std::ostringstream out;
    WriteKicadPcb(out, *circuit, *placement);
    return {std::move(circuit), std::move(*placement), out.str()};
}

std::unique_ptr<LoadedCircuit> LoadedFrom(std::string_view netlist) {
    auto loaded = LoadNetlist(netlist, "t.bwn");
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

// reference → pad number → net ("" when none), from the written file.
std::map<std::string, std::map<std::string, std::string>> PadNets(std::string_view text) {
    const auto root = ParseSExpr(text, "t.kicad_pcb");
    REQUIRE(root.has_value());
    std::map<std::string, std::map<std::string, std::string>> nets;
    for (const SExpr& footprint : root->list) {
        if (footprint.head() != "footprint") {
            continue;
        }
        std::string reference;
        for (const SExpr& child : footprint.list) {
            if (child.head() == "property" && child.atomAt(1) == "Reference") {
                reference = child.atomAt(2);
            }
        }
        for (const SExpr& child : footprint.list) {
            if (child.head() == "pad") {
                const SExpr* net = child.find("net");
                nets[reference][std::string(child.atomAt(1))] =
                    net != nullptr ? std::string(net->atomAt(1)) : "";
            }
        }
    }
    return nets;
}

void CheckPadNets(const Written& w) {
    const auto nets = PadNets(w.text);
    std::map<std::pair<std::string, std::string>, std::string> expected;
    for (const Connection& c : w.circuit->connections) {
        expected[{c.reference, std::to_string(c.pin + 1)}] = c.net;
    }
    for (const auto& [reference, pads] : nets) {
        for (const auto& [number, net] : pads) {
            INFO(reference << " pad " << number);
            const auto it = expected.find({reference, number});
            CHECK(net == (it != expected.end() ? it->second : ""));
        }
    }
    CHECK(nets.size() == w.circuit->parts.size());
}

constexpr std::string_view kNetlist = "part J1 POWER\n"
                                      "part U1 74LS04\n"
                                      "part D1 LED\n"
                                      "part R1 R value=330\n"
                                      "part DS1 DIGIT\n"
                                      "part DS2 DIGIT\n"
                                      "net L1 U1.1Y D1.K\n"
                                      "net A1 R1.2 D1.A\n"
                                      "net VCC R1.1 DS1.CA1 DS2.CA1\n";

constexpr std::string_view kRules = "edge J1 left middle\n"
                                    "edge D1 front middle\n"
                                    "next R1 D1 right\n"
                                    "row back middle DS1 DS2\n";

}  // namespace

TEST_CASE("kicad pcb writer: reads back where it was placed", "[board]") {
    const Written w = Write(LoadedFrom(kNetlist), kRules);
    const auto board = ReadKicadBoard(w.text, "t.kicad_pcb");
    if (!board) {
        for (const Diagnostic& d : board.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(board.has_value());
    REQUIRE(board->components.size() == w.placement.parts.size());
    for (const PlacedPart& part : w.placement.parts) {
        INFO(part.reference);
        const auto it =
            std::ranges::find(board->components, part.reference, &BoardComponent::reference);
        REQUIRE(it != board->components.end());
        CHECK(it->position == part.origin);
        CHECK(it->footprint == part.footprint->id);
        REQUIRE(it->pads.size() == part.footprint->pads.size());
        for (std::size_t i = 0; i < it->pads.size(); ++i) {
            CHECK(it->pads[i].position ==
                  Point{part.origin.x + part.footprint->pads[i].position.x,
                        part.origin.y + part.footprint->pads[i].position.y});
        }
    }
    REQUIRE(board->outline.size() == 4);
    CHECK(board->traces.empty());
    CHECK(board->vias.empty());
}

TEST_CASE("kicad pcb writer: a footprint's point in Board coordinates", "[board]") {
    // KiCad 10 reads a footprint's (point) at its Board position, not relative to the footprint.
    const Written w = Write(LoadedFrom(kNetlist), kRules);
    const auto root = ParseSExpr(w.text, "t.kicad_pcb");
    REQUIRE(root.has_value());
    const PlacedPart& u1 = *std::ranges::find(w.placement.parts, "U1", &PlacedPart::reference);
    int points = 0;
    for (const SExpr& footprint : root->list) {
        const auto isU1 = [](const SExpr& c) {
            return c.head() == "property" && c.atomAt(1) == "Reference" && c.atomAt(2) == "U1";
        };
        if (footprint.head() != "footprint" || !std::ranges::any_of(footprint.list, isU1)) {
            continue;
        }
        for (const SExpr& child : footprint.list) {
            if (child.head() == "point") {
                const SExpr* at = child.find("at");
                REQUIRE(at != nullptr);
                // DIP-14_W7.62mm's point is its centre, (3.81, 7.62) from pad 1.
                CHECK(std::stod(std::string(at->atomAt(1))) * 1e6 ==
                      Catch::Approx(static_cast<double>(u1.origin.x) + 3.81e6));
                CHECK(std::stod(std::string(at->atomAt(2))) * 1e6 ==
                      Catch::Approx(static_cast<double>(u1.origin.y) + 7.62e6));
                ++points;
            }
        }
    }
    CHECK(points == 1);
}

TEST_CASE("kicad pcb writer: pads carry their Nets", "[board]") {
    const Written w = Write(LoadedFrom(kNetlist), kRules);
    CheckPadNets(w);
    const auto nets = PadNets(w.text);
    CHECK(nets.at("U1").at("2") == "L1");
    CHECK(nets.at("U1").at("1") == "");  // input 1A unconnected
    CHECK(nets.at("U1").at("14") == "VCC");
}

TEST_CASE("kicad pcb writer: references, values, no shared uuids", "[board]") {
    const Written w = Write(LoadedFrom(kNetlist), kRules);
    CHECK_FALSE(w.text.contains("REF**"));
    CHECK_FALSE(w.text.contains("(uuid"));
    CHECK_FALSE(w.text.contains("generator_version \"10.0\""));
    CHECK(w.text.contains("(property \"Reference\" \"DS2\""));
    CHECK(w.text.contains("(property \"Value\" \"330\""));
    CHECK(w.text.contains("(footprint \"Display_7Segment:7SegmentLED_LTS6760_LTS6780\""));
    CHECK(w.text.contains("(pinfunction \"1Y\")"));
}

TEST_CASE("kicad pcb writer: the same placement writes the same file", "[board]") {
    CHECK(Write(LoadedFrom(kNetlist), kRules).text == Write(LoadedFrom(kNetlist), kRules).text);
}

TEST_CASE("kicad pcb writer: project", "[board]") {
    std::ostringstream out;
    WriteKicadProject(out, "tennis");
    const std::string text = out.str();
    CHECK(text.contains("\"filename\": \"tennis.kicad_pro\""));
    CHECK(text.contains("\"min_copper_edge_clearance\": 0.5"));
    CHECK(text.starts_with("{"));
    CHECK(text.ends_with("}\n"));
}

TEST_CASE("kicad pcb writer: tennis", "[board][slow]") {
    auto circuit = LoadNetlistFile(std::string(BW_NETLIST_DIR) + "/tennis.bwn");
    REQUIRE(circuit.has_value());
    std::ifstream in(std::string(BW_BOARD_DIR) + "/tennis/tennis.place");
    std::ostringstream rules;
    rules << in.rdbuf();
    REQUIRE_FALSE(rules.str().empty());
    const Written w = Write(std::move(*circuit), rules.str());
    const auto board = ReadKicadBoard(w.text, "tennis.kicad_pcb");
    REQUIRE(board.has_value());
    CHECK(board->components.size() == w.circuit->parts.size());
    CheckPadNets(w);
}

TEST_CASE("kicad pcb writer: a part turned by grid pet reads back with its pads turned",
          "[board]") {
    const Written w = Write(LoadedFrom("part J1 POWER\npart UC9 74LS93\npart UA5 6522\n"
                                       "net Q UC9.QA UA5.CA1\n"),
                            "grid pet\nupright UA5 down\nedge J1 left middle\n");
    const auto board = ReadKicadBoard(w.text, "t.kicad_pcb");
    REQUIRE(board.has_value());
    for (const PlacedPart& part : w.placement.parts) {
        INFO(part.reference);
        const auto it =
            std::ranges::find(board->components, part.reference, &BoardComponent::reference);
        REQUIRE(it != board->components.end());
        CHECK(it->position == part.origin);
        CHECK(it->angle == part.angle);
        for (std::size_t i = 0; i < it->pads.size(); ++i) {
            const Point local = part.footprint->pads[i].position;
            const Point turned = part.angle == 90    ? Point{local.y, -local.x}
                                 : part.angle == 180 ? Point{-local.x, -local.y}
                                                     : local;
            CHECK(it->pads[i].position ==
                  Point{part.origin.x + turned.x, part.origin.y + turned.y});
        }
    }
}
