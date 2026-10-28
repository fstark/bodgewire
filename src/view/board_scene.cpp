#include "view/board_scene.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <map>
#include <utility>

#include "board/copper.hpp"

namespace bw {

namespace {

constexpr std::int64_t kCourtyardFallbackMargin = 500'000;  // nm

bool Contains(std::span<const Point> polygon, Point p) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Point a = polygon[i];
        const Point b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x = static_cast<double>(b.x - a.x) * static_cast<double>(p.y - a.y) /
                                 static_cast<double>(b.y - a.y) +
                             static_cast<double>(a.x);
            inside = inside != (static_cast<double>(p.x) < x);
        }
    }
    return inside;
}

std::vector<Point> EdgePoints(const OutlineEdge& edge) {
    if (edge.mid) {
        return ArcPolyline(edge.start, *edge.mid, edge.end);
    }
    return {edge.start, edge.end};
}

std::vector<Point> BoxCorners(Point min, Point max) {
    return {min, {max.x, min.y}, max, {min.x, max.y}};
}

std::vector<Point> OutlinePolygon(const std::vector<OutlineEdge>& outline) {
    std::vector<std::vector<Point>> edges;
    for (const OutlineEdge& edge : outline) {
        edges.push_back(EdgePoints(edge));
    }
    std::vector<Point> polygon;
    std::vector<bool> used(edges.size(), false);
    if (!edges.empty()) {
        polygon = edges[0];
        used[0] = true;
        bool extended = true;
        while (extended && polygon.back() != polygon.front()) {
            extended = false;
            for (std::size_t i = 0; i < edges.size(); ++i) {
                if (used[i]) {
                    continue;
                }
                std::vector<Point> e = edges[i];
                if (e.back() == polygon.back()) {
                    std::ranges::reverse(e);
                }
                if (e.front() == polygon.back()) {
                    polygon.insert(polygon.end(), e.begin() + 1, e.end());
                    used[i] = true;
                    extended = true;
                    break;
                }
            }
        }
    }
    if (polygon.size() >= 4 && polygon.back() == polygon.front() &&
        std::ranges::all_of(used, std::identity{})) {
        polygon.pop_back();
        return polygon;
    }
    Point min{INT64_MAX, INT64_MAX};
    Point max{INT64_MIN, INT64_MIN};
    for (const auto& e : edges) {
        for (const Point p : e) {
            min = {std::min(min.x, p.x), std::min(min.y, p.y)};
            max = {std::max(max.x, p.x), std::max(max.y, p.y)};
        }
    }
    return BoxCorners(min, max);
}

std::vector<Point> PadBox(const BoardComponent& component) {
    Point min{INT64_MAX, INT64_MAX};
    Point max{INT64_MIN, INT64_MIN};
    for (const Pad& pad : component.pads) {
        const std::int64_t half = std::max(pad.size.x, pad.size.y) / 2 + kCourtyardFallbackMargin;
        min = {std::min(min.x, pad.position.x - half), std::min(min.y, pad.position.y - half)};
        max = {std::max(max.x, pad.position.x + half), std::max(max.y, pad.position.y + half)};
    }
    if (component.pads.empty()) {
        const Point p = component.position;
        return BoxCorners({p.x - kCourtyardFallbackMargin, p.y - kCourtyardFallbackMargin},
                          {p.x + kCourtyardFallbackMargin, p.y + kCourtyardFallbackMargin});
    }
    return BoxCorners(min, max);
}

std::optional<std::size_t> PinNumber(std::string_view text) {
    std::size_t n = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), n);
    if (ec != std::errc{} || end != text.data() + text.size() || n == 0) {
        return std::nullopt;
    }
    return n;
}

}  // namespace

BoardScene::BoardScene(Board drawn, const Board& pristine, const LoadedCircuit& design)
    : board_(std::move(drawn)) {
    outline_ = OutlinePolygon(board_.outline);
    bounds_ = {outline_[0], outline_[0]};
    for (const Point p : outline_) {
        bounds_.min = {std::min(bounds_.min.x, p.x), std::min(bounds_.min.y, p.y)};
        bounds_.max = {std::max(bounds_.max.x, p.x), std::max(bounds_.max.y, p.y)};
    }

    for (const auto& [name, id] : design.nets) {
        const auto index = std::to_underlying(id);
        if (index >= netNames_.size()) {
            netNames_.resize(index + 1);
        }
        netNames_[index] = name;
    }
    std::map<std::pair<std::string, std::string>, NetId, std::less<>> pinNets;
    for (const Connection& c : design.connections) {
        if (const auto net = design.nets.find(c.net); net != design.nets.end()) {
            pinNets[{c.reference, std::to_string(c.pin + 1)}] = net->second;
        }
    }
    const auto pinNet = [&](const std::string& reference,
                            const std::string& number) -> std::optional<NetId> {
        const auto it = pinNets.find(std::pair{reference, number});
        return it != pinNets.end() ? std::optional(it->second) : std::nullopt;
    };

    for (const BoardComponent& component : board_.components) {
        courtyards_.push_back(component.courtyard.empty() ? PadBox(component)
                                                          : component.courtyard);
        auto& nets = padNets_.emplace_back();
        ComponentInfo& info = info_.emplace_back();
        for (const LoadedPart& part : design.parts) {
            if (part.reference == component.reference) {
                info.value = part.value;
            }
        }
        const auto chip = design.components.find(component.reference);
        for (const Pad& pad : component.pads) {
            nets.push_back(pinNet(component.reference, pad.number));
            std::string name;
            const auto number = PinNumber(pad.number);
            if (chip != design.components.end() && number &&
                *number <= chip->second->pins().size()) {
                name = chip->second->pins()[*number - 1].name;
            }
            info.pinNames.push_back(std::move(name));
        }
    }

    const Islands islands = FindIslands(pristine);
    std::vector<std::optional<NetId>> islandNet(islands.islandSize.size());
    for (std::size_t k = 0; k < pristine.components.size(); ++k) {
        const BoardComponent& component = pristine.components[k];
        for (std::size_t p = 0; p < component.pads.size(); ++p) {
            auto& net = islandNet[islands.padIsland[k][p]];
            if (!net) {
                net = pinNet(component.reference, component.pads[p].number);
            }
        }
    }
    for (std::size_t v = 0; v < board_.vias.size(); ++v) {
        viaNets_.push_back(v < islands.viaIsland.size() ? islandNet[islands.viaIsland[v]]
                                                        : std::nullopt);
    }
    for (const Trace& t : board_.traces) {
        const Point m =
            t.mid ? *t.mid : Point{(t.start.x + t.end.x) / 2, (t.start.y + t.end.y) / 2};
        std::optional<NetId> net;
        for (std::size_t s = 0; s < pristine.traces.size(); ++s) {
            const Trace& candidate = pristine.traces[s];
            if (candidate.layer != t.layer) {
                continue;
            }
            const std::int64_t reach = candidate.width / 2 + 1;
            const auto [minX, maxX] = std::minmax(candidate.start.x, candidate.end.x);
            const auto [minY, maxY] = std::minmax(candidate.start.y, candidate.end.y);
            if (!candidate.mid && (m.x < minX - reach || m.x > maxX + reach || m.y < minY - reach ||
                                   m.y > maxY + reach)) {
                continue;
            }
            if (Covers(candidate, m)) {
                net = islandNet[islands.traceIsland[s]];
                break;
            }
        }
        traceNets_.push_back(net);
    }
}

std::optional<NetId> BoardScene::netOf(const Hit& hit) const {
    switch (hit.kind) {
    case HitKind::Pad:
        return padNet(hit.component, hit.index);
    case HitKind::Via:
        return viaNet(hit.index);
    case HitKind::Trace:
        return traceNet(hit.index);
    case HitKind::Component:
        return std::nullopt;
    }
    return std::nullopt;
}

std::string_view BoardScene::netName(NetId net) const {
    const auto index = std::to_underlying(net);
    return index < netNames_.size() ? std::string_view(netNames_[index]) : std::string_view{};
}

std::optional<Hit> BoardScene::hitTest(Point at, Layer active) const {
    for (std::size_t k = 0; k < board_.components.size(); ++k) {
        const auto& pads = board_.components[k].pads;
        for (std::size_t p = 0; p < pads.size(); ++p) {
            if (Covers(pads[p], at)) {
                return Hit{HitKind::Pad, k, p};
            }
        }
    }
    for (std::size_t v = 0; v < board_.vias.size(); ++v) {
        if (Covers(board_.vias[v], at)) {
            return Hit{HitKind::Via, 0, v};
        }
    }
    for (std::size_t t = 0; t < board_.traces.size(); ++t) {
        if (board_.traces[t].layer == active && Covers(board_.traces[t], at)) {
            return Hit{HitKind::Trace, 0, t};
        }
    }
    for (std::size_t k = 0; k < courtyards_.size(); ++k) {
        if (Contains(courtyards_[k], at)) {
            return Hit{HitKind::Component, k, 0};
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> BoardScene::componentAt(Point at) const {
    for (std::size_t k = 0; k < board_.components.size(); ++k) {
        for (const Pad& pad : board_.components[k].pads) {
            if (Covers(pad, at)) {
                return k;
            }
        }
    }
    for (std::size_t k = 0; k < courtyards_.size(); ++k) {
        if (Contains(courtyards_[k], at)) {
            return k;
        }
    }
    return std::nullopt;
}

std::string BoardScene::hoverText(const Hit& hit) const {
    const auto name = [&](std::optional<NetId> net) {
        return net ? std::string(netName(*net)) : std::string();
    };
    switch (hit.kind) {
    case HitKind::Pad: {
        const BoardComponent& component = board_.components[hit.component];
        const std::string& pinName = info_[hit.component].pinNames[hit.index];
        const std::string net = name(padNet(hit.component, hit.index));
        return std::format("{} pin {}{}{} · {}", component.reference,
                           component.pads[hit.index].number, pinName.empty() ? "" : " ", pinName,
                           net.empty() ? "—" : net);
    }
    case HitKind::Via:
        return name(viaNet(hit.index));
    case HitKind::Trace:
        return name(traceNet(hit.index));
    case HitKind::Component:
        return std::format("{} {}", board_.components[hit.component].reference,
                           info_[hit.component].value);
    }
    return {};
}

}  // namespace bw
