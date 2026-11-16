#include "board/kicad_netlist.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bw {

namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 24> kFootprints{{
    {"DIP-8", "Package_DIP:DIP-8_W7.62mm"},
    {"DIP-14", "Package_DIP:DIP-14_W7.62mm"},
    {"DIP-16", "Package_DIP:DIP-16_W7.62mm"},
    {"DIP-18", "Package_DIP:DIP-18_W7.62mm"},
    {"DIP-20", "Package_DIP:DIP-20_W7.62mm"},
    {"DIP-24", "Package_DIP:DIP-24_W15.24mm"},
    {"DIP-40", "Package_DIP:DIP-40_W15.24mm"},
    {"KK-2", "Connector_Molex:Molex_KK-254_AE-6410-02A_1x02_P2.54mm_Vertical"},
    {"KK-3", "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical"},
    {"KK-4", "Connector_Molex:Molex_KK-254_AE-6410-04A_1x04_P2.54mm_Vertical"},
    {"KK-10", "Connector_Molex:Molex_KK-254_AE-6410-10A_1x10_P2.54mm_Vertical"},
    {"HDR-1x06", "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical"},
    {"HDR-1x07", "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical"},
    {"HDR-1x13", "Connector_PinHeader_2.54mm:PinHeader_1x13_P2.54mm_Vertical"},
    {"HDR-1x20", "Connector_PinHeader_2.54mm:PinHeader_1x20_P2.54mm_Vertical"},
    {"HDR-2x12", "Connector_PinHeader_2.54mm:PinHeader_2x12_P2.54mm_Vertical"},
    {"HDR-2x23", "Connector_PinHeader_2.54mm:PinHeader_2x23_P2.54mm_Vertical"},
    {"axial", "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"},
    {"disc", "Capacitor_THT:C_Disc_D3.0mm_W1.6mm_P2.50mm"},
    {"LED-5mm", "LED_THT:LED_D5.0mm"},
    {"DIGIT-0.56", "Display_7Segment:7SegmentLED_LTS6760_LTS6780"},
    {"TACT-6x3.5", "Button_Switch_THT:SW_PUSH_1P1T_6x3.5mm_H5.0_APEM_MJTP1250"},
    {"MX-1U", "Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB"},
    {"SLIDE-1", "Button_Switch_THT:SW_DIP_SPSTx01_Slide_9.78x4.72mm_W7.62mm_P2.54mm"},
}};

std::uint64_t Fnv1a(std::string_view text, std::uint64_t hash) {
    for (const char c : text) {
        hash = (hash ^ static_cast<std::uint8_t>(c)) * 0x100000001b3;
    }
    return hash;
}

}  // namespace

std::string KicadQuote(std::string_view text) {
    std::string quoted = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            quoted += '\\';
        }
        quoted += c;
    }
    return quoted + '"';
}

std::string KicadUuid(std::string_view reference) {
    const std::uint64_t high = Fnv1a(reference, 0xcbf29ce484222325);
    const std::uint64_t low = Fnv1a(reference, 0x84222325cbf29ce4);
    return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}", high >> 32, (high >> 16) & 0xffff,
                       high & 0xffff, low >> 48, low & 0xffffffffffff);
}

std::string_view KicadFootprint(std::string_view package) {
    for (const auto& [name, footprint] : kFootprints) {
        if (name == package) {
            return footprint;
        }
    }
    return {};
}

void WriteKicadNetlist(std::ostream& out, const LoadedCircuit& loaded, std::string_view source) {
    out << "(export (version \"E\")\n";
    out << "  (design (source " << KicadQuote(source) << ") (tool \"bodgewire\"))\n";
    out << "  (components";
    for (const LoadedPart& part : loaded.parts) {
        out << "\n    (comp (ref " << KicadQuote(part.reference) << ") (value "
            << KicadQuote(part.value) << ") (footprint "
            << KicadQuote(KicadFootprint(part.info->package)) << ")\n";
        out << "      (libsource (lib \"bodgewire\") (part " << KicadQuote(part.info->name)
            << ") (description " << KicadQuote(part.info->description) << "))\n";
        out << "      (sheetpath (names \"/\") (tstamps \"/\"))\n";
        out << "      (tstamps " << KicadQuote(KicadUuid(part.reference)) << "))";
    }
    out << ")\n";

    std::map<std::string_view, std::vector<const Connection*>> nets;
    for (const Connection& c : loaded.connections) {
        nets[c.net].push_back(&c);  // connections are already by reference, pin
    }
    out << "  (nets";
    int code = 1;
    for (const auto& [name, pins] : nets) {
        out << "\n    (net (code \"" << code++ << "\") (name " << KicadQuote(name) << ")";
        for (const Connection* c : pins) {
            out << "\n      (node (ref " << KicadQuote(c->reference) << ") (pin \"" << c->pin + 1
                << "\")";
            if (const auto it = loaded.components.find(c->reference);
                it != loaded.components.end()) {
                out << " (pinfunction " << KicadQuote(it->second->pins()[c->pin].name) << ")";
            }
            out << ")";
        }
        out << ")";
    }
    out << "))\n";
}

}  // namespace bw
