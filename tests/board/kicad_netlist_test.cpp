#include "board/kicad_netlist.hpp"

#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "board/sexpr.hpp"

using namespace bw;

namespace {

std::string Export(std::string_view netlist) {
    const auto loaded = LoadNetlist(netlist, "t.bwn");
    REQUIRE(loaded.has_value());
    std::ostringstream out;
    WriteKicadNetlist(out, **loaded, "t.bwn");
    return out.str();
}

constexpr std::string_view kNetlist = "part X1 OSC freq=1MHz\n"
                                      "part U1 74LS00\n"
                                      "part R1 R value=4k7\n"
                                      "net CLK X1.OUT U1.1A\n"
                                      "net VCC R1.1\n"
                                      "net B R1.2 U1.1B\n";

}  // namespace

TEST_CASE("kicad netlist: footprint table", "[board]") {
    CHECK(KicadFootprint("DIP-8") == "Package_DIP:DIP-8_W7.62mm");
    CHECK(KicadFootprint("DIP-14") == "Package_DIP:DIP-14_W7.62mm");
    CHECK(KicadFootprint("DIP-16") == "Package_DIP:DIP-16_W7.62mm");
    CHECK(KicadFootprint("DIP-18") == "Package_DIP:DIP-18_W7.62mm");
    CHECK(KicadFootprint("DIP-20") == "Package_DIP:DIP-20_W7.62mm");
    CHECK(KicadFootprint("DIP-24") == "Package_DIP:DIP-24_W15.24mm");
    CHECK(KicadFootprint("KK-10") ==
          "Connector_Molex:Molex_KK-254_AE-6410-10A_1x10_P2.54mm_Vertical");
    CHECK(KicadFootprint("axial") ==
          "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal");
    CHECK(KicadFootprint("KK-2") ==
          "Connector_Molex:Molex_KK-254_AE-6410-02A_1x02_P2.54mm_Vertical");
    CHECK(KicadFootprint("KK-3") ==
          "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical");
    CHECK(KicadFootprint("LED-5mm") == "LED_THT:LED_D5.0mm");
    CHECK(KicadFootprint("DIP-40").empty());
}

TEST_CASE("kicad netlist: components and nets", "[board]") {
    const std::string text = Export(kNetlist);
    const auto root = ParseSExpr(text, "t.net");
    REQUIRE(root.has_value());
    CHECK(root->head() == "export");

    const SExpr* components = root->find("components");
    REQUIRE(components != nullptr);
    REQUIRE(components->list.size() == 4);
    const SExpr& r1 = components->list[3];
    CHECK(r1.find("ref")->atomAt(1) == "R1");
    CHECK(r1.find("value")->atomAt(1) == "4k7");
    CHECK(r1.find("footprint")->atomAt(1) ==
          "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal");
    CHECK(components->list[1].find("value")->atomAt(1) == "OSC");

    const SExpr* nets = root->find("nets");
    REQUIRE(nets != nullptr);
    std::vector<std::string> lines;
    for (std::size_t i = 1; i < nets->list.size(); ++i) {
        const SExpr& net = nets->list[i];
        std::string line = std::string(net.find("code")->atomAt(1)) + " " +
                           std::string(net.find("name")->atomAt(1)) + ":";
        for (const SExpr& node : net.list) {
            if (node.head() == "node") {
                line += " " + std::string(node.find("ref")->atomAt(1)) + "." +
                        std::string(node.find("pin")->atomAt(1));
                if (const SExpr* function = node.find("pinfunction")) {
                    line += "(" + std::string(function->atomAt(1)) + ")";
                }
            }
        }
        lines.push_back(line);
    }
    // OSC NoConnect pins absent; implicit power present; nets sorted by name.
    CHECK(lines == std::vector<std::string>{
                       "1 B: R1.2 U1.2(1B)",
                       "2 CLK: U1.1(1A) X1.8(OUT)",
                       "3 GND: U1.7(GND) X1.7(GND)",
                       "4 VCC: R1.1 U1.14(VCC) X1.14(VCC)",
                   });
}

TEST_CASE("kicad netlist: stable UUIDs", "[board]") {
    const std::string first = Export(kNetlist);
    CHECK(first == Export(kNetlist));
    const auto root = ParseSExpr(first, "t.net");
    REQUIRE(root.has_value());
    const SExpr* components = root->find("components");
    const auto uuid = [&](std::size_t i) {
        return std::string(components->list[i].find("tstamps")->atomAt(1));
    };
    CHECK(uuid(1).size() == 36);
    CHECK(uuid(1) != uuid(2));
}
