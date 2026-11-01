#include "board/kicad_board.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <numbers>
#include <optional>
#include <utility>

#include "board/sexpr.hpp"

namespace bw {

namespace {

constexpr std::int64_t kNanometresPerMillimetre = 1'000'000;

bool IsCopper(std::string_view layer) { return layer.ends_with(".Cu"); }

std::optional<Layer> CopperLayer(std::string_view name) {
    if (name == "F.Cu") {
        return Layer::Front;
    }
    if (name == "B.Cu") {
        return Layer::Back;
    }
    return std::nullopt;
}

std::optional<double> ParseDouble(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    const std::string copy(text);
    char* end = nullptr;
    const double value = std::strtod(copy.c_str(), &end);
    if (end != copy.c_str() + copy.size()) {
        return std::nullopt;
    }
    return value;
}

std::string_view LayerOf(const SExpr& node) {
    const SExpr* layer = node.find("layer");
    return layer != nullptr ? layer->atomAt(1) : std::string_view{};
}

class BoardReader {
public:
    BoardReader(const SExpr& root, std::string file) : root_(root), file_(std::move(file)) {}

    BoardResult read() {
        readHeader();
        for (const SExpr& child : root_.list) {
            const std::string_view head = child.head();
            if (head == "footprint") {
                readFootprint(child);
            } else if (head == "segment" || head == "arc") {
                readTrace(child);
            } else if (head == "via") {
                readVia(child);
            } else if (head == "zone") {
                error(child, "copper zones are not supported");
            } else if (head.starts_with("gr_")) {
                readGraphic(child);
            }
        }
        if (board_.outline.empty()) {
            error(root_, "no board outline on Edge.Cuts");
        }
        if (!errors_.empty()) {
            std::stable_sort(
                errors_.begin(), errors_.end(),
                [](const Diagnostic& a, const Diagnostic& b) { return a.line < b.line; });
            return std::unexpected(std::move(errors_));
        }
        return std::move(board_);
    }

private:
    void error(const SExpr& node, std::string message) {
        errors_.push_back({file_, node.line, Severity::Error, std::move(message)});
    }

    std::int64_t length(const SExpr& node, std::size_t index) {
        const auto value = ParseMillimetres(node.atomAt(index));
        if (!value) {
            error(node, "bad number in (" + std::string(node.head()) + ")");
            return 0;
        }
        return *value;
    }

    // (head x y) child of `node`.
    Point point(const SExpr& node, std::string_view head) {
        const SExpr* child = node.find(head);
        if (child == nullptr) {
            error(node, "missing (" + std::string(head) + ")");
            return {};
        }
        return {length(*child, 1), length(*child, 2)};
    }

    std::int64_t size(const SExpr& node, std::string_view head) {
        const SExpr* child = node.find(head);
        if (child == nullptr) {
            error(node, "missing (" + std::string(head) + ")");
            return 0;
        }
        return length(*child, 1);
    }

    double angle(const SExpr& at) {
        if (at.atomAt(3).empty()) {
            return 0;
        }
        const auto value = ParseDouble(at.atomAt(3));
        if (!value) {
            error(at, "bad angle");
            return 0;
        }
        return *value;
    }

    void readHeader() {
        if (root_.head() != "kicad_pcb") {
            error(root_, "not a KiCad board file");
            return;
        }
        const SExpr* version = root_.find("version");
        std::int64_t number = 0;
        if (version != nullptr) {
            const std::string_view text = version->atomAt(1);
            std::from_chars(text.data(), text.data() + text.size(), number);
        }
        if (number < kMinKicadBoardVersion) {
            error(version != nullptr ? *version : root_,
                  "board file older than KiCad 9: open and save it in KiCad 9 or later");
        }
        if (const SExpr* layers = root_.find("layers")) {
            for (const SExpr& layer : layers->list) {
                const std::string_view name = layer.atomAt(1);
                if (layer.isList && IsCopper(name) && !CopperLayer(name)) {
                    error(layer, "copper layer " + std::string(name) +
                                     " not supported: only F.Cu and B.Cu");
                }
            }
        }
    }

    void readFootprint(const SExpr& node) {
        BoardComponent component{.line = node.line, .footprint = std::string(node.atomAt(1))};
        const auto side = CopperLayer(LayerOf(node));
        if (!side) {
            error(node, "footprint " + component.footprint + " not on F.Cu or B.Cu");
            return;
        }
        component.side = *side;
        if (const SExpr* at = node.find("at")) {
            component.position = {length(*at, 1), length(*at, 2)};
            component.angle = angle(*at);
        } else {
            error(node, "missing (at)");
        }
        for (const SExpr& child : node.list) {
            if (child.head() == "property" && child.atomAt(1) == "Reference") {
                component.reference = child.atomAt(2);
            }
        }
        if (component.reference.empty()) {
            error(node, "footprint " + component.footprint + " has no reference");
        }
        const double radians = component.angle * std::numbers::pi / 180;
        const double cos = std::cos(radians);
        const double sin = std::sin(radians);
        const Point origin = component.position;
        const auto place = [origin, cos, sin](double x, double y) {
            return Point{origin.x + std::llround(x * cos + y * sin),
                         origin.y + std::llround(-x * sin + y * cos)};
        };
        std::optional<std::pair<Point, Point>> lineBox;  // local min, max of .CrtYd fp_lines
        for (const SExpr& child : node.list) {
            const std::string_view head = child.head();
            if (head == "pad") {
                readPad(child, component, place);
            } else if (head == "zone") {
                error(child, "copper zones are not supported");
            } else if (head.starts_with("fp_") && IsCopper(LayerOf(child))) {
                error(child, component.reference + ": copper graphics are not supported");
            } else if ((head == "fp_rect" || head == "fp_poly") &&
                       LayerOf(child).ends_with(".CrtYd") && component.courtyard.empty()) {
                component.courtyard = readCourtyard(child, place);
            } else if (head == "fp_line" && LayerOf(child).ends_with(".CrtYd")) {
                for (const Point p : {point(child, "start"), point(child, "end")}) {
                    if (!lineBox) {
                        lineBox = {p, p};
                    }
                    auto& [min, max] = *lineBox;
                    min = {std::min(min.x, p.x), std::min(min.y, p.y)};
                    max = {std::max(max.x, p.x), std::max(max.y, p.y)};
                }
            }
        }
        if (component.courtyard.empty() && lineBox) {
            const auto [a, c] = *lineBox;
            for (const Point p : {a, Point{c.x, a.y}, c, Point{a.x, c.y}}) {
                component.courtyard.push_back(
                    place(static_cast<double>(p.x), static_cast<double>(p.y)));
            }
        }
        board_.components.push_back(std::move(component));
    }

    template <typename Place>
    std::vector<Point> readCourtyard(const SExpr& node, const Place& place) {
        std::vector<Point> points;
        if (node.head() == "fp_rect") {
            const Point a = point(node, "start");
            const Point c = point(node, "end");
            for (const Point p : {a, Point{c.x, a.y}, c, Point{a.x, c.y}}) {
                points.push_back(place(static_cast<double>(p.x), static_cast<double>(p.y)));
            }
            return points;
        }
        if (const SExpr* pts = node.find("pts")) {
            for (const SExpr& xy : pts->list) {
                if (xy.head() == "xy") {
                    points.push_back(place(static_cast<double>(length(xy, 1)),
                                           static_cast<double>(length(xy, 2))));
                }
            }
        }
        return points;
    }

    template <typename Place>
    void readPad(const SExpr& node, BoardComponent& component, const Place& place) {
        const std::string_view type = node.atomAt(2);
        if (type == "np_thru_hole") {
            return;
        }
        const std::string where = component.reference + " pad " + std::string(node.atomAt(1));
        if (type != "thru_hole") {
            error(node, where + ": only through-hole pads are supported");
            return;
        }
        Pad pad{.number = std::string(node.atomAt(1))};
        const std::string_view shape = node.atomAt(3);
        if (shape == "circle") {
            pad.shape = PadShape::Circle;
        } else if (shape == "oval") {
            pad.shape = PadShape::Oval;
        } else if (shape == "rect") {
            pad.shape = PadShape::Rectangle;
        } else if (shape == "roundrect") {
            pad.shape = PadShape::RoundedRectangle;
        } else {
            error(node, where + ": pad shape " + std::string(shape) + " not supported");
            return;
        }
        const SExpr* at = node.find("at");
        if (at == nullptr) {
            error(node, where + ": missing (at)");
            return;
        }
        pad.position =
            place(static_cast<double>(length(*at, 1)), static_cast<double>(length(*at, 2)));
        pad.angle = angle(*at);
        pad.size = point(node, "size");
        if (const SExpr* drill = node.find("drill")) {
            const std::size_t index = drill->atomAt(1) == "oval" ? 2 : 1;
            pad.drill = length(*drill, index);
        }
        if (pad.shape == PadShape::RoundedRectangle) {
            const SExpr* ratio = node.find("roundrect_rratio");
            const auto value = ratio != nullptr ? ParseDouble(ratio->atomAt(1)) : std::nullopt;
            if (!value) {
                error(node, where + ": missing (roundrect_rratio)");
            } else {
                pad.cornerRadius =
                    std::llround(*value * static_cast<double>(std::min(pad.size.x, pad.size.y)));
            }
        }
        component.pads.push_back(std::move(pad));
    }

    void readTrace(const SExpr& node) {
        const auto layer = CopperLayer(LayerOf(node));
        if (!layer) {
            error(node, "track on layer " + std::string(LayerOf(node)) +
                            " not supported: only F.Cu and B.Cu");
            return;
        }
        Trace trace{.layer = *layer,
                    .start = point(node, "start"),
                    .end = point(node, "end"),
                    .width = size(node, "width")};
        if (node.head() == "arc") {
            trace.mid = point(node, "mid");
        }
        board_.traces.push_back(trace);
    }

    void readVia(const SExpr& node) {
        if (node.atomAt(1) == "blind" || node.atomAt(1) == "micro") {
            error(node, "blind and micro vias are not supported");
            return;
        }
        board_.vias.push_back({.position = point(node, "at"),
                               .diameter = size(node, "size"),
                               .drill = size(node, "drill")});
    }

    void readGraphic(const SExpr& node) {
        const std::string_view layer = LayerOf(node);
        const std::string_view head = node.head();
        if (IsCopper(layer)) {
            error(node, "copper graphics are not supported");
            return;
        }
        if (layer != "Edge.Cuts") {
            return;
        }
        if (head == "gr_line") {
            board_.outline.push_back({point(node, "start"), point(node, "end"), std::nullopt});
        } else if (head == "gr_arc") {
            board_.outline.push_back(
                {point(node, "start"), point(node, "end"), point(node, "mid")});
        } else if (head == "gr_rect") {
            const Point a = point(node, "start");
            const Point c = point(node, "end");
            const std::array<Point, 4> corners{a, Point{c.x, a.y}, c, Point{a.x, c.y}};
            for (std::size_t i = 0; i < corners.size(); ++i) {
                board_.outline.push_back({corners[i], corners[(i + 1) % 4], std::nullopt});
            }
        } else if (head == "gr_poly") {
            readPolyOutline(node);
        } else {
            error(node, "outline shape " + std::string(head) + " not supported");
        }
    }

    void readPolyOutline(const SExpr& node) {
        const SExpr* pts = node.find("pts");
        std::vector<Point> points;
        if (pts != nullptr) {
            for (const SExpr& xy : pts->list) {
                if (xy.head() == "xy") {
                    points.push_back({length(xy, 1), length(xy, 2)});
                }
            }
        }
        if (points.size() < 3) {
            error(node, "outline polygon needs 3 points");
            return;
        }
        for (std::size_t i = 0; i < points.size(); ++i) {
            board_.outline.push_back({points[i], points[(i + 1) % points.size()], std::nullopt});
        }
    }

    const SExpr& root_;
    std::string file_;
    Board board_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

std::optional<std::int64_t> ParseMillimetres(std::string_view text) {
    bool negative = false;
    if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
        negative = text[0] == '-';
        text.remove_prefix(1);
    }
    const std::size_t dot = text.find('.');
    const std::string_view whole = text.substr(0, dot);
    const std::string_view fraction =
        dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);
    if ((whole.empty() && fraction.empty()) || fraction.size() > 6 || whole.size() > 9) {
        return std::nullopt;
    }
    std::int64_t value = 0;
    for (const char c : whole) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    value *= kNanometresPerMillimetre;
    std::int64_t scale = kNanometresPerMillimetre / 10;
    for (const char c : fraction) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value += (c - '0') * scale;
        scale /= 10;
    }
    return negative ? -value : value;
}

BoardResult ReadKicadBoard(std::string_view text, std::string file) {
    const auto root = ParseSExpr(text, file);
    if (!root) {
        return std::unexpected(std::vector<Diagnostic>{root.error()});
    }
    return BoardReader(*root, std::move(file)).read();
}

BoardResult ReadKicadBoardFile(const std::filesystem::path& path) {
    std::string file = path.filename().string();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return ReadKicadBoard(text, std::move(file));
}

}  // namespace bw
