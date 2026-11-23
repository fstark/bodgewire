#include "board/footprint.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "board/kicad_board.hpp"
#include "board/kicad_netlist.hpp"
#include "board/sexpr.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

class Box {
public:
    void grow(Point p) {
        if (!min_) {
            min_ = max_ = p;
            return;
        }
        min_ = Point{std::min(min_->x, p.x), std::min(min_->y, p.y)};
        max_ = Point{std::max(max_->x, p.x), std::max(max_->y, p.y)};
    }
    bool empty() const { return !min_.has_value(); }
    Point min() const { return *min_; }
    Point max() const { return *max_; }

private:
    std::optional<Point> min_, max_;
};

std::optional<Point> PointOf(const SExpr& node) {
    const auto x = ParseMillimetres(node.atomAt(1));
    const auto y = ParseMillimetres(node.atomAt(2));
    if (!x || !y) {
        return std::nullopt;
    }
    return Point{*x, *y};
}

std::string_view LayerOf(const SExpr& node) {
    const SExpr* layer = node.find("layer");
    return layer != nullptr ? layer->atomAt(1) : std::string_view{};
}

}  // namespace

Footprint ReadFootprint(std::string_view text, std::string_view id, std::string_view file,
                        std::vector<Diagnostic>& errors) {
    Footprint footprint{.id = std::string(id), .text = std::string(text)};
    const auto error = [&](std::uint32_t line, std::string message) {
        errors.push_back({std::string(file), line, Severity::Error, std::move(message)});
    };
    const auto root = ParseSExpr(text, file);
    if (!root) {
        errors.push_back(root.error());
        return footprint;
    }
    if (root->head() != "footprint") {
        error(root->line, "not a footprint");
        return footprint;
    }
    Box courtyard, pads;
    for (const SExpr& child : root->list) {
        const std::string_view head = child.head();
        if (head == "pad") {
            if (child.atomAt(2) == "np_thru_hole") {
                continue;
            }
            const SExpr* at = child.find("at");
            const SExpr* size = child.find("size");
            const auto position = at != nullptr ? PointOf(*at) : std::nullopt;
            const auto extent = size != nullptr ? PointOf(*size) : std::nullopt;
            if (!position || !extent) {
                error(child.line, "pad without (at) or (size)");
                continue;
            }
            footprint.pads.push_back({std::string(child.atomAt(1)), *position});
            const std::int64_t half = std::max(extent->x, extent->y) / 2;
            pads.grow({position->x - half, position->y - half});
            pads.grow({position->x + half, position->y + half});
        } else if (head.starts_with("fp_") && LayerOf(child) == "F.CrtYd") {
            for (const SExpr& point : child.list) {
                const std::string_view name = point.head();
                if (name == "start" || name == "end" || name == "mid" || name == "center") {
                    if (const auto p = PointOf(point)) {
                        courtyard.grow(*p);
                    }
                } else if (name == "pts") {
                    for (const SExpr& xy : point.list) {
                        if (const auto p = xy.head() == "xy" ? PointOf(xy) : std::nullopt) {
                            courtyard.grow(*p);
                        }
                    }
                }
            }
        }
    }
    if (footprint.pads.empty()) {
        error(root->line, "footprint has no pads");
    }
    if (courtyard.empty()) {
        error(root->line, "footprint has no courtyard");
        return footprint;
    }
    if (!pads.empty()) {
        courtyard.grow(pads.min());
        courtyard.grow(pads.max());
    }
    footprint.courtyardMin = courtyard.min();
    footprint.courtyardMax = courtyard.max();
    return footprint;
}

std::expected<FootprintLibrary, std::vector<Diagnostic>>
LoadFootprints(const std::filesystem::path& directory, std::span<const std::string_view> packages) {
    FootprintLibrary library;
    std::vector<Diagnostic> errors;
    for (const std::string_view package : packages) {
        if (library.contains(package)) {
            continue;
        }
        const std::string_view id = KicadFootprint(package);
        if (id.empty()) {
            errors.push_back({"", 0, Severity::Error,
                              "no footprint for package '" + std::string(package) + "'"});
            continue;
        }
        const std::string name(id.substr(id.find(':') + 1));
        const std::filesystem::path path = directory / (name + ".kicad_mod");
        const auto text = ReadTextFile(path);
        if (!text) {
            errors.push_back({path.string(), 0, Severity::Error, "cannot read footprint"});
            continue;
        }
        library.emplace(std::string(package), ReadFootprint(*text, id, path.string(), errors));
    }
    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    return library;
}

}  // namespace bw
