#include "view/board_scene.hpp"

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <format>
#include <map>
#include <utility>

#include "board/copper.hpp"
#include "netlist/net_pins.hpp"

namespace bw {

namespace {

constexpr std::int64_t kCourtyardFallbackMargin = 500'000;  // nm
constexpr std::int64_t kMaxHalfPitch = 1'270'000;  // nm: a wide resistor must not grab the board

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

double Distance(Point a, Point b) {
    return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

Point NearestOnSegment(Point a, Point b, Point at) {
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    const double length2 = dx * dx + dy * dy;
    if (length2 == 0) {
        return a;
    }
    const double t = std::clamp(
        (static_cast<double>(at.x - a.x) * dx + static_cast<double>(at.y - a.y) * dy) / length2,
        0.0, 1.0);
    return {a.x + std::llround(dx * t), a.y + std::llround(dy * t)};
}

struct EdgeDistance {
    double edge;    // nm from the copper edge, 0 inside
    Point nearest;  // on the centreline
};

EdgeDistance TraceEdgeDistance(const Trace& trace, Point at) {
    const std::vector<Point> points = trace.mid ? ArcPolyline(trace.start, *trace.mid, trace.end)
                                                : std::vector<Point>{trace.start, trace.end};
    EdgeDistance best{DBL_MAX, trace.start};
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const Point p = NearestOnSegment(points[i], points[i + 1], at);
        const double edge = std::max(0.0, Distance(p, at) - static_cast<double>(trace.width) / 2);
        if (edge < best.edge) {
            best = {edge, p};
        }
    }
    return best;
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
                info.part = part.info;
            }
        }
        const auto chip = design.components.find(component.reference);
        for (const Pad& pad : component.pads) {
            nets.push_back(pinNet(component.reference, pad.number));
            std::string name;
            bool power = false;
            const auto number = PinNumber(pad.number);
            if (chip != design.components.end() && number &&
                *number <= chip->second->pins().size()) {
                const PinSpec& spec = chip->second->pins()[*number - 1];
                name = spec.name;
                power = spec.kind == PinKind::Power;
            }
            info.pinNames.push_back(std::move(name));
            info.powerPins.push_back(power);
        }
        double pitch = DBL_MAX;
        for (std::size_t a = 0; a < component.pads.size(); ++a) {
            for (std::size_t b = a + 1; b < component.pads.size(); ++b) {
                pitch = std::min(pitch,
                                 Distance(component.pads[a].position, component.pads[b].position));
            }
        }
        info.halfPitch =
            pitch == DBL_MAX
                ? 0
                : std::min(static_cast<std::int64_t>(std::llround(pitch / 2)), kMaxHalfPitch);
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

    driverPads_.resize(netNames_.size());
    for (const auto& [name, id] : design.nets) {
        for (const NetPin& pin : DescribeNet(design, name).drivers) {
            const auto component = componentIndex(pin.reference);
            const auto pad =
                component ? padIndex(*component, std::to_string(pin.pin + 1)) : std::nullopt;
            if (pad) {
                driverPads_[std::to_underlying(id)].push_back({*component, *pad});
            }
        }
    }
}

std::span<const ScenePad> BoardScene::drivers(NetId net) const {
    const auto index = std::to_underlying(net);
    return index < driverPads_.size() ? std::span<const ScenePad>(driverPads_[index])
                                      : std::span<const ScenePad>{};
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

std::optional<Pick> BoardScene::pick(Point at, Layer active, PickReach reach) const {
    std::optional<Pick> best;
    double bestDistance = DBL_MAX;
    for (std::size_t k = 0; k < board_.components.size(); ++k) {
        const auto& pads = board_.components[k].pads;
        const double limit = static_cast<double>(std::max(reach.pad, info_[k].halfPitch));
        for (std::size_t p = 0; p < pads.size(); ++p) {
            const double distance = Covers(pads[p], at) ? 0 : Distance(pads[p].position, at);
            if (distance <= limit && distance < bestDistance) {
                best = Pick{{HitKind::Pad, k, p}, active, pads[p].position};
                bestDistance = distance;
            }
        }
    }
    if (best) {
        return best;
    }
    for (std::size_t v = 0; v < board_.vias.size(); ++v) {
        const Via& via = board_.vias[v];
        const double edge =
            std::max(0.0, Distance(via.position, at) - static_cast<double>(via.diameter) / 2);
        if (edge <= static_cast<double>(reach.copper) && edge < bestDistance) {
            best = Pick{{HitKind::Via, 0, v}, active, via.position};
            bestDistance = edge;
        }
    }
    if (best) {
        return best;
    }
    const Layer other = active == Layer::Front ? Layer::Back : Layer::Front;
    for (const Layer layer : {active, other}) {
        for (std::size_t t = 0; t < board_.traces.size(); ++t) {
            const Trace& trace = board_.traces[t];
            if (trace.layer != layer) {
                continue;
            }
            const EdgeDistance d = TraceEdgeDistance(trace, at);
            if (d.edge <= static_cast<double>(reach.copper) && d.edge < bestDistance) {
                best = Pick{{HitKind::Trace, 0, t}, layer, d.nearest};
                bestDistance = d.edge;
            }
        }
        if (best) {
            return best;
        }
    }
    for (std::size_t k = 0; k < courtyards_.size(); ++k) {
        if (Contains(courtyards_[k], at)) {
            return Pick{{HitKind::Component, k, 0}, active, at};
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

std::string_view BoardScene::partName(std::size_t component) const {
    const PartInfo* part = info_[component].part;
    return part ? part->name : std::string_view{};
}

std::string_view BoardScene::pinName(std::size_t component, std::size_t pad) const {
    return info_[component].pinNames[pad];
}

bool BoardScene::powerPin(std::size_t component, std::size_t pad) const {
    return info_[component].powerPins[pad];
}

std::optional<std::size_t> BoardScene::componentIndex(std::string_view reference) const {
    for (std::size_t k = 0; k < board_.components.size(); ++k) {
        if (board_.components[k].reference == reference) {
            return k;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> BoardScene::padIndex(std::size_t component, std::string_view pin) const {
    const auto& pads = board_.components[component].pads;
    for (std::size_t p = 0; p < pads.size(); ++p) {
        if (pads[p].number == pin) {
            return p;
        }
    }
    for (std::size_t p = 0; p < pads.size(); ++p) {
        if (!pin.empty() && info_[component].pinNames[p] == pin) {
            return p;
        }
    }
    return std::nullopt;
}

bool AcceptsLead(LeadKind kind, HitKind hit) {
    switch (hit) {
    case HitKind::Pad:
        return true;
    case HitKind::Via:
    case HitKind::Trace:
        return kind == LeadKind::Held;
    case HitKind::Component:
        return false;
    }
    return false;
}

std::string LeadLabel(const BoardScene& scene, const Hit& hit) {
    switch (hit.kind) {
    case HitKind::Pad: {
        const BoardComponent& component = scene.board().components[hit.component];
        std::string label = component.reference + "." + component.pads[hit.index].number;
        if (const auto name = scene.pinName(hit.component, hit.index); !name.empty()) {
            label += " " + std::string(name);
        }
        return label;
    }
    case HitKind::Via:
        return "via";
    case HitKind::Trace:
        return "Trace";
    case HitKind::Component:
        return scene.board().components[hit.component].reference;
    }
    return {};
}

}  // namespace bw
