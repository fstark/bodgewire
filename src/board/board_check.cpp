#include "board/board_check.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "board/copper.hpp"
#include "board/kicad_netlist.hpp"

namespace bw {

namespace {

// Orders pads by reference, then pad number as a number.
struct PadKey {
    std::string reference;
    std::string number;

    int value() const {
        int n = 0;
        std::from_chars(number.data(), number.data() + number.size(), n);
        return n;
    }
    bool operator<(const PadKey& other) const {
        return std::pair(std::string_view(reference), value()) <
                   std::pair(std::string_view(other.reference), other.value()) ||
               (reference == other.reference && value() == other.value() && number < other.number);
    }
    std::string label() const { return reference + "." + number; }
};

std::string JoinPads(const std::set<PadKey>& pads) {
    std::string text;
    for (const PadKey& pad : pads) {
        text += (text.empty() ? "" : ", ") + pad.label();
    }
    return text;
}

}  // namespace

std::vector<std::string> CheckBoard(const LoadedCircuit& loaded, const Board& board) {
    std::vector<std::string> problems;

    std::map<std::string_view, std::size_t> onBoard;
    for (std::size_t i = 0; i < board.components.size(); ++i) {
        const BoardComponent& component = board.components[i];
        if (!onBoard.emplace(component.reference, i).second) {
            problems.push_back(component.reference + ": on the board twice");
        }
    }
    std::map<std::string_view, const LoadedPart*> inNetlist;
    std::vector<std::string> components;
    for (const LoadedPart& part : loaded.parts) {
        inNetlist.emplace(part.reference, &part);
        const auto it = onBoard.find(part.reference);
        if (it == onBoard.end()) {
            components.push_back(part.reference + ": missing from board");
            continue;
        }
        const std::string_view expected = KicadFootprint(part.info->package);
        const std::string& actual = board.components[it->second].footprint;
        if (actual != expected) {
            components.push_back(part.reference + ": footprint " + actual + ", expected " +
                                 std::string(expected));
        }
    }
    for (const auto& [reference, index] : onBoard) {
        if (!inNetlist.contains(reference)) {
            components.push_back(std::string(reference) + ": not in netlist");
        }
    }

    std::map<PadKey, std::string_view> expectedNet;
    for (const Connection& c : loaded.connections) {
        PadKey key{c.reference, std::to_string(c.pin + 1)};
        if (const auto it = onBoard.find(c.reference); it != onBoard.end()) {
            const BoardComponent& component = board.components[it->second];
            const bool found = std::ranges::any_of(
                component.pads, [&](const Pad& pad) { return pad.number == key.number; });
            if (!found) {
                components.push_back(key.label() + ": no pad " + key.number + " on footprint " +
                                     component.footprint);
            }
        }
        expectedNet.emplace(std::move(key), c.net);
    }
    std::ranges::sort(components);
    problems.insert(problems.end(), components.begin(), components.end());

    const Islands islands = FindIslands(board);
    std::map<std::uint32_t, std::map<std::string_view, PadKey>> netsOfIsland;
    std::map<std::string_view, std::map<std::uint32_t, std::set<PadKey>>> islandsOfNet;
    std::vector<std::string> shorts;
    std::vector<std::string> unconnected;
    for (std::size_t c = 0; c < board.components.size(); ++c) {
        const BoardComponent& component = board.components[c];
        if (!inNetlist.contains(component.reference)) {
            continue;
        }
        for (std::size_t p = 0; p < component.pads.size(); ++p) {
            const std::uint32_t island = islands.padIsland[c][p];
            PadKey key{component.reference, component.pads[p].number};
            const auto net = expectedNet.find(key);
            if (net == expectedNet.end()) {
                if (islands.islandSize[island] > 1) {
                    unconnected.push_back(key.label() + ": copper on an unconnected pad");
                }
                continue;
            }
            auto& first = netsOfIsland[island];
            if (const auto it = first.find(net->second); it == first.end() || key < it->second) {
                first.insert_or_assign(net->second, key);
            }
            islandsOfNet[net->second][island].insert(std::move(key));
        }
    }
    for (const auto& [island, nets] : netsOfIsland) {
        if (nets.size() < 2) {
            continue;
        }
        std::string text = "short:";
        for (const auto& [net, pad] : nets) {
            text += (text.size() > 6 ? ", " : " ") + std::string(net) + " (" + pad.label() + ")";
        }
        shorts.push_back(std::move(text));
    }
    std::ranges::sort(shorts);
    std::ranges::sort(unconnected);
    problems.insert(problems.end(), shorts.begin(), shorts.end());
    problems.insert(problems.end(), unconnected.begin(), unconnected.end());

    for (const auto& [net, byIsland] : islandsOfNet) {
        if (byIsland.size() < 2) {
            continue;
        }
        std::vector<const std::set<PadKey>*> groups;
        for (const auto& [island, pads] : byIsland) {
            groups.push_back(&pads);
        }
        std::ranges::sort(groups,
                          [](const auto* a, const auto* b) { return *a->begin() < *b->begin(); });
        std::string text = "open " + std::string(net) + ":";
        for (std::size_t i = 0; i < groups.size(); ++i) {
            text += (i == 0 ? " {" : " | {") + JoinPads(*groups[i]) + "}";
        }
        problems.push_back(std::move(text));
    }
    return problems;
}

}  // namespace bw
