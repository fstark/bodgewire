#include "board/kicad_pcb_writer.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <map>
#include <string>
#include <utility>

#include "board/kicad_netlist.hpp"
#include "sim/check.hpp"

namespace bw {

namespace {

constexpr std::int64_t kA4Width = 297'000'000;  // nm
constexpr std::int64_t kA4Height = 210'000'000;

// nm → "15.24": exact, trailing zeros dropped.
std::string Millimetres(std::int64_t nm) {
    std::string text = std::format("{}{}.{:06}", nm < 0 ? "-" : "", std::abs(nm) / 1'000'000,
                                   std::abs(nm) % 1'000'000);
    while (text.back() == '0') {
        text.pop_back();
    }
    if (text.back() == '.') {
        text.pop_back();
    }
    return text == "-0" ? "0" : text;
}

std::string_view Trimmed(std::string_view line) {
    const std::size_t first = line.find_first_not_of('\t');
    return first == std::string_view::npos ? std::string_view{} : line.substr(first);
}

struct PadNet {
    std::string net;
    std::string pin;  // the Component's pin name; "" for a resistor
};

using PadNets = std::map<std::pair<std::string, std::string>, PadNet>;  // reference, pad number

PadNets PadNetsOf(const LoadedCircuit& loaded) {
    PadNets nets;
    for (const Connection& c : loaded.connections) {
        PadNet pad{c.net, ""};
        if (const auto it = loaded.components.find(c.reference); it != loaded.components.end()) {
            pad.pin = it->second->pins()[c.pin].name;
        }
        nets[{c.reference, std::to_string(c.pin + 1)}] = std::move(pad);
    }
    return nets;
}

void WriteFootprint(std::ostream& out, const PlacedPart& part, std::string_view value,
                    const PadNets& nets) {
    const std::string_view text = part.footprint->text;
    std::string pad;     // number of the pad block we are in, "" outside one
    bool point = false;  // in a (point …) block: KiCad keeps its (at …) in Board coordinates
    bool first = true;
    bool placed = false;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view line = text.substr(start, end - start);
        start = end + 1;
        const std::string_view body = Trimmed(line);
        const std::size_t depth = line.size() - body.size();
        if (body.empty()) {
            continue;
        }
        if (first) {
            out << "\t(footprint " << KicadQuote(part.footprint->id) << "\n";
            first = false;
            continue;
        }
        if (depth == 1 && (body.starts_with("(version ") || body.starts_with("(generator"))) {
            continue;
        }
        if (body.starts_with("(uuid ")) {
            continue;  // two copies of a footprint must not share them
        }
        if (depth == 1 && body.starts_with("(pad ")) {
            const std::size_t open = body.find('"');
            pad = std::string(body.substr(open + 1, body.find('"', open + 1) - open - 1));
        } else if (depth == 1) {
            pad.clear();
        }
        if (depth == 1) {
            point = body.starts_with("(point");
        }
        if (!pad.empty() && depth == 2 && body.starts_with("(layers ")) {
            if (const auto it = nets.find({part.reference, pad}); it != nets.end()) {
                out << "\t\t\t(net " << KicadQuote(it->second.net) << ")\n";
                if (!it->second.pin.empty()) {
                    out << "\t\t\t(pinfunction " << KicadQuote(it->second.pin) << ")\n";
                }
            }
        }
        std::string copy(line);
        if (depth == 1 && body.starts_with("(property \"Reference\" ")) {
            copy = std::string(line.substr(0, depth)) + "(property \"Reference\" " +
                   KicadQuote(part.reference);
        } else if (depth == 1 && body.starts_with("(property \"Value\" ")) {
            copy = std::string(line.substr(0, depth)) + "(property \"Value\" " + KicadQuote(value);
        }
        if (point && depth == 2 && body.starts_with("(at ")) {
            // A footprint's (point) is the one item KiCad reads in Board coordinates, unlike its
            // pads and graphics: moved by the origin, or it lands far from its part.
            double x = 0, y = 0;
            Check(std::sscanf(std::string(body).c_str(), "(at %lf %lf)", &x, &y) == 2,
                  "footprint " + part.footprint->id + ": bad point " + std::string(body));
            // Turned with its part: KiCad's +90° maps (x, y) to (y, −x).
            const double rx = part.angle == 90 ? y : x;
            const double ry = part.angle == 90 ? -x : y;
            copy = std::string(line.substr(0, depth)) + "(at " +
                   Millimetres(part.origin.x + std::llround(rx * 1e6)) + ' ' +
                   Millimetres(part.origin.y + std::llround(ry * 1e6)) + ')';
        } else if (part.angle != 0 && !pad.empty() && depth == 2 && body.starts_with("(at ")) {
            // A pad's angle is the Board's: the footprint's added to the pad's own.
            double x = 0, y = 0, a = 0;
            const int n = std::sscanf(std::string(body).c_str(), "(at %lf %lf %lf)", &x, &y, &a);
            Check(n >= 2, "footprint " + part.footprint->id + ": bad pad " + std::string(body));
            copy = std::string(line.substr(0, depth)) + "(at " +
                   Millimetres(std::llround(x * 1e6)) + ' ' + Millimetres(std::llround(y * 1e6)) +
                   ' ' + std::to_string(static_cast<int>(std::lround(a)) + part.angle) + ')';
        }
        out << '\t' << copy << '\n';
        if (!placed && depth == 1 && body == "(layer \"F.Cu\")") {
            out << "\t\t(at " << Millimetres(part.origin.x) << ' ' << Millimetres(part.origin.y);
            if (part.angle != 0) {
                out << ' ' << part.angle;
            }
            out << ")\n";
            out << "\t\t(path " << KicadQuote("/" + KicadUuid(part.reference)) << ")\n";
            placed = true;
        }
    }
    Check(placed, "footprint " + part.footprint->id + " has no (layer \"F.Cu\") line");
}

}  // namespace

void WriteKicadPcb(std::ostream& out, const LoadedCircuit& loaded, const Placement& placement) {
    const bool a4 = placement.outlineMax.x <= kA4Width && placement.outlineMax.y <= kA4Height;
    out << "(kicad_pcb\n"
           "\t(version 20260206)\n"
           "\t(generator \"bodgewire\")\n"
           "\t(generator_version \"1\")\n"
           "\t(general\n\t\t(thickness 1.6)\n\t\t(legacy_teardrops no)\n\t)\n"
        << "\t(paper \"" << (a4 ? "A4" : "A3") << "\")\n"
        << "\t(layers\n"
           "\t\t(0 \"F.Cu\" signal)\n"
           "\t\t(2 \"B.Cu\" signal)\n"
           "\t\t(9 \"F.Adhes\" user \"F.Adhesive\")\n"
           "\t\t(11 \"B.Adhes\" user \"B.Adhesive\")\n"
           "\t\t(13 \"F.Paste\" user)\n"
           "\t\t(15 \"B.Paste\" user)\n"
           "\t\t(5 \"F.SilkS\" user \"F.Silkscreen\")\n"
           "\t\t(7 \"B.SilkS\" user \"B.Silkscreen\")\n"
           "\t\t(1 \"F.Mask\" user)\n"
           "\t\t(3 \"B.Mask\" user)\n"
           "\t\t(17 \"Dwgs.User\" user \"User.Drawings\")\n"
           "\t\t(19 \"Cmts.User\" user \"User.Comments\")\n"
           "\t\t(21 \"Eco1.User\" user \"User.Eco1\")\n"
           "\t\t(23 \"Eco2.User\" user \"User.Eco2\")\n"
           "\t\t(25 \"Edge.Cuts\" user)\n"
           "\t\t(27 \"Margin\" user)\n"
           "\t\t(31 \"F.CrtYd\" user \"F.Courtyard\")\n"
           "\t\t(29 \"B.CrtYd\" user \"B.Courtyard\")\n"
           "\t\t(35 \"F.Fab\" user)\n"
           "\t\t(33 \"B.Fab\" user)\n"
           "\t)\n"
           "\t(setup\n\t\t(pad_to_mask_clearance 0)\n\t)\n";
    const PadNets nets = PadNetsOf(loaded);
    for (std::size_t i = 0; i < placement.parts.size(); ++i) {
        const PlacedPart& part = placement.parts[i];
        std::string_view value = part.reference;
        for (const LoadedPart& loadedPart : loaded.parts) {
            if (loadedPart.reference == part.reference) {
                value = loadedPart.value;
            }
        }
        WriteFootprint(out, part, value, nets);
    }
    out << "\t(gr_rect\n"
        << "\t\t(start " << Millimetres(placement.outlineMin.x) << ' '
        << Millimetres(placement.outlineMin.y) << ")\n"
        << "\t\t(end " << Millimetres(placement.outlineMax.x) << ' '
        << Millimetres(placement.outlineMax.y) << ")\n"
        << "\t\t(stroke\n\t\t\t(width 0.05)\n\t\t\t(type default)\n\t\t)\n"
           "\t\t(fill no)\n"
           "\t\t(layer \"Edge.Cuts\")\n"
           "\t)\n"
           "\t(embedded_fonts no)\n"
           ")\n";
}

void WriteKicadProject(std::ostream& out, std::string_view name) {
    out << "{\n"
           "  \"board\": {\n"
           "    \"design_settings\": {\n"
           "      \"rules\": {\n"
           "        \"max_error\": 0.005,\n"
           "        \"min_clearance\": 0.0,\n"
           "        \"min_connection\": 0.0,\n"
           "        \"min_copper_edge_clearance\": 0.5,\n"
           "        \"min_groove_width\": 0.0,\n"
           "        \"min_hole_clearance\": 0.25,\n"
           "        \"min_hole_to_hole\": 0.25,\n"
           "        \"min_microvia_diameter\": 0.2,\n"
           "        \"min_microvia_drill\": 0.1,\n"
           "        \"min_resolved_spokes\": 2,\n"
           "        \"min_silk_clearance\": 0.0,\n"
           "        \"min_text_height\": 0.8,\n"
           "        \"min_text_thickness\": 0.08,\n"
           "        \"min_through_hole_diameter\": 0.3,\n"
           "        \"min_track_width\": 0.2,\n"
           "        \"min_via_annular_width\": 0.1,\n"
           "        \"min_via_diameter\": 0.5,\n"
           "        \"solder_mask_to_copper_clearance\": 0.0,\n"
           "        \"use_height_for_length_calcs\": true\n"
           "      }\n"
           "    }\n"
           "  },\n"
           "  \"meta\": {\n"
           "    \"filename\": "
        << KicadQuote(std::string(name) + ".kicad_pro")
        << ",\n"
           "    \"version\": 3\n"
           "  }\n"
           "}\n";
}

}  // namespace bw
