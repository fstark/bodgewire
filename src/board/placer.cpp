#include "board/placer.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string_view>
#include <utility>
#include <variant>

namespace bw {

namespace {

constexpr std::int64_t kMm = 1'000'000;
constexpr std::int64_t kColumnPitch = 16 * kMm;  // the 070 prototype's spacing
constexpr std::int64_t kRowPitch = 27 * kMm;
constexpr std::int64_t kEdgeMargin = 1 * kMm;  // panel courtyard to outline
constexpr std::int64_t kBandGap = 4 * kMm;     // panel courtyard to grid courtyard
constexpr std::int64_t kBareMargin = 5 * kMm;  // outline to grid, edge without panel parts
constexpr std::int64_t kPartGap = 2 * kMm;     // between panel courtyards
constexpr std::int64_t kSnap = 1'270'000;      // 0.05 in: pads on the routing grid
constexpr Point kOrigin{15 * kMm, 15 * kMm};   // outline top-left corner
constexpr double kAspect = 1.6;                // grid width / height wanted
constexpr double kHotMm = 200;                 // annealing temperatures, as place.py
constexpr double kColdMm = 0.0005;
constexpr std::int64_t kMovesPerPart = 10'000;

struct Box {
    Point min, max;
};

Box Courtyard(const Footprint& footprint, Point origin, int angle = 0) {
    if (angle == 90) {  // KiCad's +90°: (x, y) → (y, −x), as kicad_board.cpp reads it
        return {{origin.x + footprint.courtyardMin.y, origin.y - footprint.courtyardMax.x},
                {origin.x + footprint.courtyardMax.y, origin.y - footprint.courtyardMin.x}};
    }
    return {{origin.x + footprint.courtyardMin.x, origin.y + footprint.courtyardMin.y},
            {origin.x + footprint.courtyardMax.x, origin.y + footprint.courtyardMax.y}};
}

Point Rotated(Point p, int angle) { return angle == 90 ? Point{p.y, -p.x} : p; }

// A PET 2001 location (grid pet, P5q): "U" + row A-J + column 1-9 → (row, column) from 0.
std::optional<std::pair<int, int>> PetCell(std::string_view reference) {
    if (reference.size() != 3 || reference[0] != 'U' || reference[1] < 'A' || reference[1] > 'J' ||
        reference[2] < '1' || reference[2] > '9') {
        return std::nullopt;
    }
    return std::pair{reference[1] - 'A', reference[2] - '1'};
}

constexpr int kPetRows = 10, kPetColumns = 9;

bool Overlap(const Box& a, const Box& b) {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

bool RunsAcross(Edge edge) { return edge == Edge::Back || edge == Edge::Front; }

// Along an edge: x for back and front, y for left and right.
std::int64_t Along(Edge edge, Point p) { return RunsAcross(edge) ? p.x : p.y; }

std::int64_t SnapDown(std::int64_t v) {
    const std::int64_t q = v / kSnap;
    return (q * kSnap > v ? q - 1 : q) * kSnap;
}
std::int64_t SnapUp(std::int64_t v) { return -SnapDown(-v); }
std::int64_t SnapNear(std::int64_t v) { return SnapDown(v + kSnap / 2); }

bool IsGridPackage(std::string_view package) {
    return package == "DIP-8" || package == "DIP-14" || package == "DIP-16" ||
           package == "DIP-18" || package == "DIP-20" || package == "axial";
}

// A rule's parts, positioned relative to its edge: the outer line of the band at 0 across,
// along from an arbitrary 0.
struct Group {
    Edge edge;
    double at;
    std::uint32_t line;
    std::vector<std::size_t> members;  // part indices
    std::size_t anchors;               // the first `anchors` members are the rule's own
};

struct Net {
    std::vector<std::pair<std::size_t, Point>> pins;  // part index, pad offset
    bool fixed;                                       // no grid part on it
};

class Placer {
public:
    Placer(const LoadedCircuit& loaded, const PlaceRules& rules, std::string_view file,
           const FootprintLibrary& footprints, const PlaceOptions& options)
        : loaded_(loaded), rules_(rules), file_(file), footprints_(footprints), options_(options) {}

    std::expected<Placement, std::vector<Diagnostic>> run() {
        if (rules_.petGrid) {
            return runPet();
        }
        sortParts();
        if (errors_.empty()) {
            buildGroups();
        }
        if (errors_.empty()) {
            layOut();
        }
        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        anneal();
        return result();
    }

private:
    void error(std::uint32_t line, std::string message) {
        errors_.push_back({std::string(file_), line, Severity::Error, std::move(message)});
    }

    void sortParts() {
        for (const LoadedPart& part : loaded_.parts) {
            const auto it = footprints_.find(part.info->package);
            if (it == footprints_.end()) {
                error(0, "no footprint for '" + part.reference + "' (" +
                             std::string(part.info->package) + ")");
                continue;
            }
            index_.emplace(part.reference, parts_.size());
            parts_.push_back({part.reference, &it->second, {}});
        }
        placed_.assign(parts_.size(), false);
        for (const PlaceRule& rule : rules_.rules) {
            const auto mark = [&](const std::string& reference) {
                if (!index_.contains(reference)) {
                    error(rule.line, "unknown reference '" + reference + "'");
                } else {
                    placed_[index_.at(reference)] = true;
                }
            };
            if (const auto* edge = std::get_if<EdgeRule>(&rule.rule)) {
                std::ranges::for_each(edge->references, mark);
            } else {
                mark(std::get<NextRule>(rule.rule).reference);
            }
        }
        for (std::size_t i = 0; i < loaded_.parts.size() && i < parts_.size(); ++i) {
            if (placed_[i]) {
                continue;
            }
            const std::string_view package = loaded_.parts[i].info->package;
            if (rules_.petGrid && PetCell(parts_[i].reference)) {
                parts_[i].angle = 90;
                petParts_.push_back(i);
            } else if (rules_.petGrid && IsGridPackage(package)) {
                parts_[i].angle = 90;
                freeParts_.push_back(i);
            } else if (!IsGridPackage(package)) {
                error(0, "'" + parts_[i].reference + "' (" + std::string(package) +
                             ") needs a placement rule");
            } else {
                gridParts_.push_back(i);
            }
        }
    }

    Box courtyard(std::size_t part) const {
        return Courtyard(*parts_[part].footprint, parts_[part].origin, parts_[part].angle);
    }

    void buildGroups() {
        std::map<std::size_t, std::size_t> groupOf;  // part → group
        for (const PlaceRule& rule : rules_.rules) {
            if (const auto* edge = std::get_if<EdgeRule>(&rule.rule)) {
                Group group{edge->edge, edge->at, rule.line, {}, edge->references.size()};
                std::int64_t cursor = 0;
                for (const std::string& reference : edge->references) {
                    const std::size_t part = index_.at(reference);
                    const Footprint& fp = *parts_[part].footprint;
                    Point& origin = parts_[part].origin;
                    if (RunsAcross(edge->edge)) {
                        origin.x = cursor - fp.courtyardMin.x;
                        cursor += fp.courtyardMax.x - fp.courtyardMin.x + kPartGap;
                        origin.y =
                            edge->edge == Edge::Front ? -fp.courtyardMax.y : -fp.courtyardMin.y;
                    } else {
                        origin.y = cursor - fp.courtyardMin.y;
                        cursor += fp.courtyardMax.y - fp.courtyardMin.y + kPartGap;
                        origin.x =
                            edge->edge == Edge::Right ? -fp.courtyardMax.x : -fp.courtyardMin.x;
                    }
                    groupOf[part] = groups_.size();
                    group.members.push_back(part);
                }
                groups_.push_back(std::move(group));
                continue;
            }
            const auto& next = std::get<NextRule>(rule.rule);
            const std::size_t part = index_.at(next.reference);
            const std::size_t other = index_.at(next.other);
            if (rules_.petGrid && !groupOf.contains(other)) {
                deferred_.push_back(&rule);  // beside a grid chip: once the grid is placed
                continue;
            }
            const std::size_t g = groupOf.at(other);
            const Footprint& fp = *parts_[part].footprint;
            const Box o = courtyard(other);
            const std::int64_t centreX = (o.min.x + o.max.x) / 2;
            const std::int64_t centreY = (o.min.y + o.max.y) / 2;
            const std::int64_t halfX = (fp.courtyardMin.x + fp.courtyardMax.x) / 2;
            const std::int64_t halfY = (fp.courtyardMin.y + fp.courtyardMax.y) / 2;
            Point& origin = parts_[part].origin;
            switch (next.side) {
            case Edge::Left:
                origin = {o.min.x - kPartGap - fp.courtyardMax.x, centreY - halfY};
                break;
            case Edge::Right:
                origin = {o.max.x + kPartGap - fp.courtyardMin.x, centreY - halfY};
                break;
            case Edge::Back:
                origin = {centreX - halfX, o.min.y - kPartGap - fp.courtyardMax.y};
                break;
            case Edge::Front:
                origin = {centreX - halfX, o.max.y + kPartGap - fp.courtyardMin.y};
                break;
            }
            const Box c = courtyard(part);
            const Edge edge = groups_[g].edge;
            const bool off =
                (edge == Edge::Front && c.max.y > 0) || (edge == Edge::Back && c.min.y < 0) ||
                (edge == Edge::Left && c.min.x < 0) || (edge == Edge::Right && c.max.x > 0);
            if (off) {
                error(rule.line, "'" + next.reference + "' falls off the " +
                                     std::string(EdgeName(edge)) + " edge");
            }
            groupOf[part] = g;
            groups_[g].members.push_back(part);
        }
    }

    // Courtyard extent of a group inwards from its edge's outer line.
    std::int64_t depth(const Group& group) const {
        std::int64_t deepest = 0;
        for (const std::size_t part : group.members) {
            const Box c = courtyard(part);
            switch (group.edge) {
            case Edge::Front:
                deepest = std::max(deepest, -c.min.y);
                break;
            case Edge::Back:
                deepest = std::max(deepest, c.max.y);
                break;
            case Edge::Left:
                deepest = std::max(deepest, c.max.x);
                break;
            case Edge::Right:
                deepest = std::max(deepest, -c.min.x);
                break;
            }
        }
        return deepest;
    }

    void layOut() {
        // Grid size and slot box.
        const auto n = static_cast<std::int32_t>(gridParts_.size());
        if (rules_.grid) {
            columns_ = rules_.grid->first;
            rows_ = rules_.grid->second;
            if (columns_ * rows_ < n) {
                return error(0, "grid " + std::to_string(columns_) + " " + std::to_string(rows_) +
                                    " has room for " + std::to_string(columns_ * rows_) +
                                    " parts, not " + std::to_string(n));
            }
        } else if (n > 0) {
            double best = 0;
            for (std::int32_t r = 1; r <= n; ++r) {
                const std::int32_t c = (n + r - 1) / r;
                const double aspect =
                    static_cast<double>(c * kColumnPitch) / static_cast<double>(r * kRowPitch);
                if (r == 1 || std::abs(aspect - kAspect) < best) {
                    best = std::abs(aspect - kAspect);
                    rows_ = r;
                    columns_ = c;
                }
            }
        }
        Box slot{{0, 0}, {0, 0}};
        for (std::size_t i = 0; i < gridParts_.size(); ++i) {
            const Box c = Courtyard(*parts_[gridParts_[i]].footprint, {0, 0});
            slot = i == 0 ? c
                          : Box{{std::min(slot.min.x, c.min.x), std::min(slot.min.y, c.min.y)},
                                {std::max(slot.max.x, c.max.x), std::max(slot.max.y, c.max.y)}};
        }

        // Bands.
        std::int64_t band[4]{kBareMargin, kBareMargin, kBareMargin, kBareMargin};
        bool used[4]{};
        for (const Group& group : groups_) {
            const auto e = static_cast<std::size_t>(group.edge);
            const std::int64_t d = kEdgeMargin + depth(group) + kBandGap;
            band[e] = used[e] ? std::max(band[e], d) : d;
            used[e] = true;
        }
        std::int64_t left = band[static_cast<std::size_t>(Edge::Left)];
        std::int64_t right = band[static_cast<std::size_t>(Edge::Right)];
        const std::int64_t back = band[static_cast<std::size_t>(Edge::Back)];
        const std::int64_t front = band[static_cast<std::size_t>(Edge::Front)];

        // Back and front groups wider than the grid widen the Board, the grid centred.
        std::int64_t gridWidth =
            columns_ > 0 ? (columns_ - 1) * kColumnPitch + slot.max.x - slot.min.x : 0;
        for (const Edge edge : {Edge::Back, Edge::Front}) {
            std::int64_t need = 2 * kEdgeMargin;
            bool first = true;
            for (const Group& group : groups_) {
                if (group.edge == edge) {
                    const Extent e = extent(group);
                    need += e.groupMax - e.groupMin + (first ? 0 : kPartGap);
                    first = false;
                }
            }
            const std::int64_t extra = need - (left + gridWidth + right);
            if (!first && extra > 0) {
                const std::int64_t half = SnapUp(extra / 2);
                left += half;
                right += std::max<std::int64_t>(extra - half, 0);
            }
        }

        Box grid{{kOrigin.x + left, kOrigin.y + back}, {kOrigin.x + left, kOrigin.y + back}};
        if (columns_ > 0) {
            gridOrigin_ = {SnapUp(kOrigin.x + left - slot.min.x),
                           SnapUp(kOrigin.y + back - slot.min.y)};
            grid.min = {gridOrigin_.x + slot.min.x, gridOrigin_.y + slot.min.y};
            grid.max = {gridOrigin_.x + (columns_ - 1) * kColumnPitch + slot.max.x,
                        gridOrigin_.y + (rows_ - 1) * kRowPitch + slot.max.y};
        }
        outline_ = {kOrigin, {grid.max.x + right, grid.max.y + front}};

        // Along the edges.
        for (const Group& group : groups_) {
            placeGroup(group, grid);
        }
        std::vector<std::size_t> panel;
        for (const Group& group : groups_) {
            panel.insert(panel.end(), group.members.begin(), group.members.end());
        }
        for (std::size_t i = 0; i < panel.size(); ++i) {
            for (std::size_t j = i + 1; j < panel.size(); ++j) {
                if (Overlap(courtyard(panel[i]), courtyard(panel[j]))) {
                    error(0, "'" + parts_[panel[i]].reference + "' overlaps '" +
                                 parts_[panel[j]].reference + "'");
                }
            }
        }
    }

    struct Extent {
        std::int64_t anchorMin, anchorMax, groupMin, groupMax;  // along the edge, local
    };

    Extent extent(const Group& group) const {
        Extent e{};
        for (std::size_t i = 0; i < group.members.size(); ++i) {
            const Box c = courtyard(group.members[i]);
            const std::int64_t a = Along(group.edge, c.min), b = Along(group.edge, c.max);
            if (i == 0) {
                e = {a, b, a, b};
            }
            if (i < group.anchors) {
                e.anchorMin = std::min(e.anchorMin, a);
                e.anchorMax = std::max(e.anchorMax, b);
            }
            e.groupMin = std::min(e.groupMin, a);
            e.groupMax = std::max(e.groupMax, b);
        }
        return e;
    }

    void placeGroup(const Group& group, const Box& grid) {
        const Edge edge = group.edge;
        const bool across = RunsAcross(edge);
        const std::int64_t start = across ? outline_.min.x : grid.min.y;
        const std::int64_t end = across ? outline_.max.x : grid.max.y;
        const std::int64_t low = start + kEdgeMargin;
        const std::int64_t high = end - kEdgeMargin;

        const auto [anchorMin, anchorMax, groupMin, groupMax] = extent(group);
        if (groupMax - groupMin > high - low) {
            return error(group.line, "'" + parts_[group.members[0]].reference + "' falls off the " +
                                         std::string(EdgeName(edge)) + " edge");
        }
        const auto target = low + std::llround(group.at * static_cast<double>(high - low));
        std::int64_t shift = target - (anchorMin + anchorMax) / 2;
        shift = std::clamp(shift, low - groupMin, high - groupMax);

        const std::int64_t outer = edge == Edge::Front  ? outline_.max.y - kEdgeMargin
                                   : edge == Edge::Back ? outline_.min.y + kEdgeMargin
                                   : edge == Edge::Left ? outline_.min.x + kEdgeMargin
                                                        : outline_.max.x - kEdgeMargin;
        for (const std::size_t part : group.members) {
            Point& origin = parts_[part].origin;
            // Snapped along the edge, then stepped back inside the margins if it left them.
            const Footprint& fp = *parts_[part].footprint;
            const auto inside = [&](std::int64_t v, std::int64_t min, std::int64_t max) {
                v = SnapNear(v);
                if (v + min < low) {
                    v += kSnap;
                } else if (v + max > high) {
                    v -= kSnap;
                }
                return v;
            };
            if (across) {
                origin.x = inside(origin.x + shift, fp.courtyardMin.x, fp.courtyardMax.x);
                origin.y =
                    edge == Edge::Front ? SnapDown(origin.y + outer) : SnapUp(origin.y + outer);
            } else {
                origin.y = inside(origin.y + shift, fp.courtyardMin.y, fp.courtyardMax.y);
                origin.x =
                    edge == Edge::Right ? SnapDown(origin.x + outer) : SnapUp(origin.x + outer);
            }
        }
    }

    Point slotOrigin(std::size_t slot) const {
        const auto c = static_cast<std::int64_t>(slot % static_cast<std::size_t>(columns_));
        const auto r = static_cast<std::int64_t>(slot / static_cast<std::size_t>(columns_));
        return {gridOrigin_.x + c * kColumnPitch, gridOrigin_.y + r * kRowPitch};
    }

    void buildNets() {
        std::map<std::string_view, Net> byName;
        std::vector<bool> inGrid(parts_.size(), false);
        for (const std::size_t part : gridParts_) {
            inGrid[part] = true;
        }
        for (const Connection& c : loaded_.connections) {
            if (IsRail(c.net)) {
                continue;
            }
            const auto it = index_.find(c.reference);
            if (it == index_.end()) {
                continue;
            }
            const std::string number = std::to_string(c.pin + 1);
            Net& net = byName[c.net];
            for (const FootprintPad& pad : parts_[it->second].footprint->pads) {
                if (pad.number == number) {
                    net.pins.emplace_back(it->second,
                                          Rotated(pad.position, parts_[it->second].angle));
                }
            }
        }
        for (auto& [name, net] : byName) {
            const std::size_t first = net.pins.empty() ? 0 : net.pins.front().first;
            const bool several = std::ranges::any_of(
                net.pins, [first](const auto& pin) { return pin.first != first; });
            if (!several) {
                continue;
            }
            net.fixed =
                std::ranges::none_of(net.pins, [&](const auto& pin) { return inGrid[pin.first]; });
            nets_.push_back(std::move(net));
        }
        netsOf_.assign(parts_.size(), {});
        for (std::size_t n = 0; n < nets_.size(); ++n) {
            for (const auto& [part, offset] : nets_[n].pins) {
                auto& list = netsOf_[part];
                if (list.empty() || list.back() != n) {
                    list.push_back(n);
                }
            }
        }
    }

    std::int64_t netCost(const Net& net, const std::vector<Point>& at) const {
        std::int64_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        bool first = true;
        for (const auto& [part, offset] : net.pins) {
            const std::int64_t x = at[part].x + offset.x, y = at[part].y + offset.y;
            if (first) {
                minX = maxX = x;
                minY = maxY = y;
                first = false;
            }
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
        return maxX - minX + maxY - minY;
    }

    // One annealing run; returns the slot of each grid part and the cost of the moving Nets.
    std::pair<std::vector<std::size_t>, std::int64_t> annealOnce(std::uint64_t seed) const {
        std::mt19937_64 rng(seed);
        const std::size_t n = gridParts_.size();
        const auto slots = static_cast<std::size_t>(columns_ * rows_);
        std::vector<std::size_t> order(slots);
        for (std::size_t i = 0; i < slots; ++i) {
            order[i] = i;
        }
        for (std::size_t i = slots; i > 1; --i) {
            std::swap(order[i - 1], order[rng() % i]);
        }
        constexpr std::size_t kEmpty = static_cast<std::size_t>(-1);
        std::vector<std::size_t> slotOf(n), occupant(slots, kEmpty);
        std::vector<Point> at(parts_.size());
        for (std::size_t p = 0; p < parts_.size(); ++p) {
            at[p] = parts_[p].origin;
        }
        for (std::size_t i = 0; i < n; ++i) {
            slotOf[i] = order[i];
            occupant[order[i]] = i;
            at[gridParts_[i]] = slotOrigin(order[i]);
        }
        std::vector<std::int64_t> cost(nets_.size());
        std::int64_t total = 0;
        for (std::size_t k = 0; k < nets_.size(); ++k) {
            cost[k] = nets_[k].fixed ? 0 : netCost(nets_[k], at);
            total += cost[k];
        }
        std::vector<std::uint64_t> stamp(nets_.size(), 0);
        std::vector<std::size_t> touched;
        std::vector<std::int64_t> fresh;
        const std::int64_t moves = kMovesPerPart * static_cast<std::int64_t>(n);
        for (std::int64_t move = 0; move < moves; ++move) {
            const double t = kHotMm * std::pow(kColdMm / kHotMm, static_cast<double>(move) /
                                                                     static_cast<double>(moves));
            const std::size_t a = rng() % n;
            const std::size_t to = rng() % slots;
            const std::size_t from = slotOf[a];
            if (to == from) {
                continue;
            }
            const std::size_t b = occupant[to];
            touched.clear();
            const auto touch = [&](std::size_t item) {
                for (const std::size_t k : netsOf_[gridParts_[item]]) {
                    if (stamp[k] != static_cast<std::uint64_t>(move) + 1) {
                        stamp[k] = static_cast<std::uint64_t>(move) + 1;
                        touched.push_back(k);
                    }
                }
            };
            touch(a);
            if (b != kEmpty) {
                touch(b);
            }
            at[gridParts_[a]] = slotOrigin(to);
            if (b != kEmpty) {
                at[gridParts_[b]] = slotOrigin(from);
            }
            std::int64_t delta = 0;
            fresh.clear();
            for (const std::size_t k : touched) {
                fresh.push_back(netCost(nets_[k], at));
                delta += fresh.back() - cost[k];
            }
            const double deltaMm = static_cast<double>(delta) / static_cast<double>(kMm);
            const double draw = static_cast<double>(rng() >> 11) * 0x1.0p-53;
            if (delta <= 0 || draw < std::exp(-deltaMm / t)) {
                slotOf[a] = to;
                occupant[to] = a;
                occupant[from] = b;
                if (b != kEmpty) {
                    slotOf[b] = from;
                }
                for (std::size_t i = 0; i < touched.size(); ++i) {
                    cost[touched[i]] = fresh[i];
                }
                total += delta;
            } else {
                at[gridParts_[a]] = slotOrigin(from);
                if (b != kEmpty) {
                    at[gridParts_[b]] = slotOrigin(to);
                }
            }
        }
        return {std::move(slotOf), total};
    }

    void anneal() {
        buildNets();
        std::vector<std::size_t> best;
        if (!gridParts_.empty()) {
            std::optional<std::int64_t> bestCost;
            for (std::int32_t i = 0; i < std::max(options_.tries, 1); ++i) {
                auto [slots, cost] = annealOnce(options_.seed + static_cast<std::uint64_t>(i));
                if (!bestCost || cost < *bestCost) {
                    bestCost = cost;
                    best = std::move(slots);
                }
            }
        }
        grid_.assign(static_cast<std::size_t>(columns_ * rows_), "");
        for (std::size_t i = 0; i < best.size(); ++i) {
            parts_[gridParts_[i]].origin = slotOrigin(best[i]);
            grid_[best[i]] = parts_[gridParts_[i]].reference;
        }
    }

    Placement result() const {
        Placement placement{.outlineMin = outline_.min,
                            .outlineMax = outline_.max,
                            .parts = parts_,
                            .columns = columns_,
                            .rows = rows_,
                            .grid = grid_};
        std::vector<Point> at(parts_.size());
        for (std::size_t p = 0; p < parts_.size(); ++p) {
            at[p] = parts_[p].origin;
        }
        for (const Net& net : nets_) {
            placement.wireLength += netCost(net, at);
        }
        return placement;
    }

    // --- grid pet (P5q) ---

    std::expected<Placement, std::vector<Diagnostic>> runPet() {
        sortParts();
        if (errors_.empty()) {
            buildGroups();
        }
        if (errors_.empty()) {
            layOutPet();
        }
        if (errors_.empty()) {
            placeDeferred();
        }
        if (errors_.empty()) {
            placeFree();
        }
        if (errors_.empty()) {
            checkOverlaps();
        }
        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        buildNets();
        columns_ = kPetColumns;
        rows_ = kPetRows;
        return result();
    }

    void layOutPet() {
        std::vector<std::vector<std::size_t>> cells(kPetRows * kPetColumns);
        for (const std::size_t part : petParts_) {
            const auto [r, c] = *PetCell(parts_[part].reference);
            cells[static_cast<std::size_t>(r * kPetColumns + c)].push_back(part);
        }
        for (std::size_t cell = 0; cell < cells.size(); ++cell) {
            if (cells[cell].size() > 1) {
                error(0, "'" + parts_[cells[cell][0]].reference + "' and '" +
                             parts_[cells[cell][1]].reference + "' are in one PET cell");
            }
        }
        if (!errors_.empty()) {
            return;
        }
        // Column widths: the longest chip lying in it (a DIP-14's at least); rows: the widest
        // chip across, then a band for a resistor beside it.
        const auto rotated = [&](std::size_t part) {
            return Courtyard(*parts_[part].footprint, {0, 0}, 90);
        };
        std::int64_t narrowest = 0, chipHeight = 0;
        for (const std::size_t part : petParts_) {
            const Box b = rotated(part);
            narrowest = narrowest == 0 ? b.max.x - b.min.x : std::min(narrowest, b.max.x - b.min.x);
            chipHeight = std::max(chipHeight, b.max.y - b.min.y);
        }
        for (const std::size_t part : freeParts_) {
            const Box b = rotated(part);
            chipHeight = std::max(chipHeight, b.max.y - b.min.y);
        }
        std::int64_t resistor = 0;
        for (std::size_t p = 0; p < parts_.size(); ++p) {
            if (loaded_.parts[p].info->package == "axial") {
                const Box b = Courtyard(*parts_[p].footprint, {0, 0});
                resistor = std::max(resistor, b.max.y - b.min.y);
            }
        }
        columnX_.assign(kPetColumns + 1, 0);
        for (int c = 0; c < kPetColumns; ++c) {
            std::int64_t width = narrowest;
            for (int r = 0; r < kPetRows; ++r) {
                for (const std::size_t part :
                     cells[static_cast<std::size_t>(r * kPetColumns + c)]) {
                    const Box b = rotated(part);
                    width = std::max(width, b.max.x - b.min.x);
                }
            }
            columnX_[static_cast<std::size_t>(c) + 1] =
                columnX_[static_cast<std::size_t>(c)] + SnapUp(width + 2 * kPartGap);
        }
        chipHeight_ = SnapUp(chipHeight + kPartGap);
        rowPitch_ = chipHeight_ + SnapUp(resistor + 2 * kPartGap);

        // Bands, as layOut.
        std::int64_t band[4]{kBareMargin, kBareMargin, kBareMargin, kBareMargin};
        bool used[4]{};
        for (const Group& group : groups_) {
            const auto e = static_cast<std::size_t>(group.edge);
            const std::int64_t d = kEdgeMargin + depth(group) + kBandGap;
            band[e] = used[e] ? std::max(band[e], d) : d;
            used[e] = true;
        }
        std::int64_t left = band[static_cast<std::size_t>(Edge::Left)];
        std::int64_t right = band[static_cast<std::size_t>(Edge::Right)];
        const std::int64_t back = band[static_cast<std::size_t>(Edge::Back)];
        const std::int64_t front = band[static_cast<std::size_t>(Edge::Front)];
        const std::int64_t width = columnX_.back();
        const std::int64_t height = kPetRows * rowPitch_;
        for (const Edge edge : {Edge::Back, Edge::Front}) {
            std::int64_t need = 2 * kEdgeMargin;
            bool first = true;
            for (const Group& group : groups_) {
                if (group.edge == edge) {
                    const Extent e = extent(group);
                    need += e.groupMax - e.groupMin + (first ? 0 : kPartGap);
                    first = false;
                }
            }
            const std::int64_t extra = need - (left + width + right);
            if (!first && extra > 0) {
                const std::int64_t half = SnapUp(extra / 2);
                left += half;
                right += std::max<std::int64_t>(extra - half, 0);
            }
        }
        gridOrigin_ = {SnapUp(kOrigin.x + left), SnapUp(kOrigin.y + back)};
        const Box grid{gridOrigin_, {gridOrigin_.x + width, gridOrigin_.y + height}};
        outline_ = {kOrigin, {grid.max.x + right, grid.max.y + front}};
        for (const Group& group : groups_) {
            placeGroup(group, grid);
        }
        grid_.assign(kPetRows * kPetColumns, "");
        for (const std::size_t part : petParts_) {
            const auto [r, c] = *PetCell(parts_[part].reference);
            putInCell(part, r, c);
        }
    }

    Box cellBox(int r, int c) const {
        const Point min{gridOrigin_.x + columnX_[static_cast<std::size_t>(c)],
                        gridOrigin_.y + r * rowPitch_};
        return {min,
                {gridOrigin_.x + columnX_[static_cast<std::size_t>(c) + 1], min.y + chipHeight_}};
    }

    // Centred in the cell's chip band, snapped so its pads are on the routing grid.
    void putInCell(std::size_t part, int r, int c) {
        const Box cell = cellBox(r, c);
        const Box b = Courtyard(*parts_[part].footprint, {0, 0}, parts_[part].angle);
        parts_[part].origin = {SnapNear((cell.min.x + cell.max.x) / 2 - (b.min.x + b.max.x) / 2),
                               SnapNear((cell.min.y + cell.max.y) / 2 - (b.min.y + b.max.y) / 2)};
        grid_[static_cast<std::size_t>(r * kPetColumns + c)] = parts_[part].reference;
        filled_.insert(part);
    }

    // `next` beside a grid chip (or a part placed so), in file order; the part keeps 0°.
    void placeDeferred() {
        for (const PlaceRule* rule : deferred_) {
            const auto& next = std::get<NextRule>(rule->rule);
            const std::size_t part = index_.at(next.reference);
            const std::size_t other = index_.at(next.other);
            if (!filled_.contains(other)) {
                error(rule->line,
                      "'" + next.other + "' is not placed before '" + next.reference + "'");
                continue;
            }
            const Footprint& fp = *parts_[part].footprint;
            const Box o = courtyard(other);
            const std::int64_t centreX = (o.min.x + o.max.x) / 2;
            const std::int64_t centreY = (o.min.y + o.max.y) / 2;
            const std::int64_t halfX = (fp.courtyardMin.x + fp.courtyardMax.x) / 2;
            const std::int64_t halfY = (fp.courtyardMin.y + fp.courtyardMax.y) / 2;
            Point origin{};
            switch (next.side) {
            case Edge::Left:
                origin = {o.min.x - kPartGap - fp.courtyardMax.x, centreY - halfY};
                break;
            case Edge::Right:
                origin = {o.max.x + kPartGap - fp.courtyardMin.x, centreY - halfY};
                break;
            case Edge::Back:
                origin = {centreX - halfX, o.min.y - kPartGap - fp.courtyardMax.y};
                break;
            case Edge::Front:
                origin = {centreX - halfX, o.max.y + kPartGap - fp.courtyardMin.y};
                break;
            }
            parts_[part].angle = 0;
            parts_[part].origin = {SnapNear(origin.x), SnapNear(origin.y)};
            filled_.insert(part);
        }
    }

    // The rest into empty cells: each, in netlist order, into the free cell nearest the parts
    // already placed that share its Nets.
    void placeFree() {
        std::map<std::string_view, std::vector<std::size_t>> partsOn;
        std::vector<std::vector<std::string_view>> netsOfPart(parts_.size());
        for (const Connection& c : loaded_.connections) {
            if (IsRail(c.net)) {
                continue;
            }
            if (const auto it = index_.find(c.reference); it != index_.end()) {
                partsOn[c.net].push_back(it->second);
                netsOfPart[it->second].push_back(c.net);
            }
        }
        std::vector<std::pair<int, int>> free;
        for (int r = 0; r < kPetRows; ++r) {
            for (int c = 0; c < kPetColumns; ++c) {
                if (grid_[static_cast<std::size_t>(r * kPetColumns + c)].empty()) {
                    free.emplace_back(r, c);
                }
            }
        }
        for (const std::size_t part : freeParts_) {
            if (free.empty()) {
                return error(0, "no empty PET cell left for '" + parts_[part].reference + "'");
            }
            double sx = 0, sy = 0, n = 0;
            for (const std::string_view net : netsOfPart[part]) {
                for (const std::size_t other : partsOn[net]) {
                    if (other != part && filled_.contains(other)) {
                        const Box b = courtyard(other);
                        sx += static_cast<double>(b.min.x + b.max.x) / 2;
                        sy += static_cast<double>(b.min.y + b.max.y) / 2;
                        n += 1;
                    }
                }
            }
            std::size_t best = 0;
            if (n > 0) {
                double bestDistance = 0;
                for (std::size_t i = 0; i < free.size(); ++i) {
                    const Box cell = cellBox(free[i].first, free[i].second);
                    const double dx = static_cast<double>(cell.min.x + cell.max.x) / 2 - sx / n;
                    const double dy = static_cast<double>(cell.min.y + cell.max.y) / 2 - sy / n;
                    const double d = dx * dx + dy * dy;
                    if (i == 0 || d < bestDistance) {
                        bestDistance = d;
                        best = i;
                    }
                }
            }
            putInCell(part, free[best].first, free[best].second);
            free.erase(free.begin() + static_cast<std::ptrdiff_t>(best));
        }
    }

    void checkOverlaps() {
        for (std::size_t i = 0; i < parts_.size(); ++i) {
            for (std::size_t j = i + 1; j < parts_.size(); ++j) {
                if (Overlap(courtyard(i), courtyard(j))) {
                    error(0,
                          "'" + parts_[i].reference + "' overlaps '" + parts_[j].reference + "'");
                }
            }
        }
    }

    std::vector<std::size_t> petParts_, freeParts_;  // grid pet: at a PET cell; in an empty one
    std::vector<const PlaceRule*> deferred_;         // grid pet: next beside a grid part
    std::vector<std::int64_t> columnX_;              // grid pet: column left edges, from the grid
    std::int64_t chipHeight_ = 0, rowPitch_ = 0;
    std::set<std::size_t> filled_;  // grid pet: placed in the grid or beside it

    const LoadedCircuit& loaded_;
    const PlaceRules& rules_;
    std::string_view file_;
    const FootprintLibrary& footprints_;
    PlaceOptions options_;

    std::vector<PlacedPart> parts_;  // netlist order
    std::map<std::string, std::size_t, std::less<>> index_;
    std::vector<bool> placed_;            // has a rule
    std::vector<std::size_t> gridParts_;  // part indices
    std::vector<Group> groups_;
    std::int32_t columns_ = 0, rows_ = 0;
    Point gridOrigin_{};
    Box outline_{};
    std::vector<Net> nets_;
    std::vector<std::vector<std::size_t>> netsOf_;  // part → Nets
    std::vector<std::string> grid_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

std::expected<Placement, std::vector<Diagnostic>>
PlaceBoard(const LoadedCircuit& loaded, const PlaceRules& rules, std::string_view rulesFile,
           const FootprintLibrary& footprints, const PlaceOptions& options) {
    return Placer(loaded, rules, rulesFile, footprints, options).run();
}

}  // namespace bw
