#include "board/place_rules.hpp"

#include <array>
#include <charconv>
#include <set>

namespace bw {

namespace {

constexpr std::array<std::string_view, 4> kEdgeNames{"left", "right", "back", "front"};

std::optional<Edge> ParseEdge(std::string_view word) {
    for (std::size_t i = 0; i < kEdgeNames.size(); ++i) {
        if (word == kEdgeNames[i]) {
            return static_cast<Edge>(i);
        }
    }
    return std::nullopt;
}

bool RunsAcross(Edge edge) { return edge == Edge::Back || edge == Edge::Front; }

std::vector<std::string_view> Words(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) {
            ++i;
        }
        std::size_t end = i;
        while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '\r') {
            ++end;
        }
        if (end > i) {
            words.push_back(line.substr(i, end - i));
        }
        i = end;
    }
    return words;
}

class Parser {
public:
    explicit Parser(std::string file) : file_(std::move(file)) {}

    void line(std::uint32_t number, std::string_view text) {
        line_ = number;
        if (const std::size_t hash = text.find('#'); hash != std::string_view::npos) {
            text = text.substr(0, hash);
        }
        const std::vector<std::string_view> words = Words(text);
        if (words.empty()) {
            return;
        }
        const std::string_view rule = words[0];
        if (rule == "edge") {
            if (words.size() != 4) {
                return error("expected: edge REF EDGE AT");
            }
            edgeRule(words[2], words[3], {words.begin() + 1, words.begin() + 2});
        } else if (rule == "row") {
            if (words.size() < 4) {
                return error("expected: row EDGE AT REF...");
            }
            edgeRule(words[1], words[2], {words.begin() + 3, words.end()});
        } else if (rule == "next") {
            if (words.size() != 4) {
                return error("expected: next REF OTHER SIDE");
            }
            nextRule(words[1], words[2], words[3]);
        } else if (rule == "grid") {
            gridRule(words);
        } else if (rule == "upright") {
            uprightRule(words);
        } else if (rule == "size") {
            sizeRule(words);
        } else if (rule == "at") {
            atRule(words);
        } else {
            error("unknown rule '" + std::string(rule) + "'");
        }
    }

    std::expected<PlaceRules, std::vector<Diagnostic>> result() {
        if (firstAt_ && !rules_.size) {
            line_ = *firstAt_;
            error("at needs size");
        }
        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        return std::move(rules_);
    }

private:
    void error(std::string message) {
        errors_.push_back({file_, line_, Severity::Error, std::move(message)});
    }

    bool claim(std::string_view reference) {
        if (!placed_.emplace(reference).second) {
            error("'" + std::string(reference) + "' has a rule already");
            return false;
        }
        return true;
    }

    void edgeRule(std::string_view edgeWord, std::string_view atWord,
                  std::vector<std::string_view> references) {
        const auto edge = ParseEdge(edgeWord);
        if (!edge) {
            return error("unknown edge '" + std::string(edgeWord) + "'");
        }
        const auto at = parseAt(*edge, atWord);
        if (!at) {
            return;
        }
        EdgeRule rule{*edge, *at, {}};
        for (const std::string_view reference : references) {
            if (claim(reference)) {
                rule.references.emplace_back(reference);
            }
        }
        rules_.rules.push_back({line_, std::move(rule)});
    }

    std::optional<double> parseAt(Edge edge, std::string_view word) {
        if (word == "middle") {
            return 0.5;
        }
        if (const auto end = ParseEdge(word)) {
            if (RunsAcross(*end) == RunsAcross(edge)) {
                error("'" + std::string(word) + "' is not an end of the " +
                      std::string(EdgeName(edge)) + " edge");
                return std::nullopt;
            }
            return *end == Edge::Left || *end == Edge::Back ? 0.0 : 1.0;
        }
        if (word.ends_with('%')) {
            double percent = 0;
            const char* last = word.data() + word.size() - 1;
            const auto [end, ec] = std::from_chars(word.data(), last, percent);
            if (ec == std::errc{} && end == last && percent >= 0 && percent <= 100) {
                return percent / 100;
            }
        }
        error("bad position '" + std::string(word) + "': an end, middle or 0%-100%");
        return std::nullopt;
    }

    void nextRule(std::string_view reference, std::string_view other, std::string_view sideWord) {
        const auto side = ParseEdge(sideWord);
        if (!side) {
            return error("unknown side '" + std::string(sideWord) + "'");
        }
        // Under grid pet a chip at a PET location ("UC8") is placed by the grid itself.
        const bool petChip = rules_.petGrid && other.size() == 3 && other[0] == 'U' &&
                             other[1] >= 'A' && other[1] <= 'J' && other[2] >= '1' &&
                             other[2] <= '9';
        if (!placed_.contains(other) && !petChip) {
            return error("'" + std::string(other) + "' is not placed by an earlier rule");
        }
        if (claim(reference)) {
            rules_.rules.push_back(
                {line_, NextRule{std::string(reference), std::string(other), *side}});
        }
    }

    void uprightRule(const std::vector<std::string_view>& words) {
        if (words.size() != 3 && words.size() != 4) {
            return error("expected: upright REF up|down|in [0|180]");
        }
        if (!rules_.petGrid) {
            return error("upright needs grid pet first");
        }
        const std::string_view reach = words[2];
        if (reach != "up" && reach != "down" && reach != "in") {
            return error("bad reach '" + std::string(reach) + "': up, down or in");
        }
        int angle = 180;
        if (words.size() == 4) {
            if (words[3] != "0" && words[3] != "180") {
                return error("bad angle '" + std::string(words[3]) + "': 0 or 180");
            }
            angle = words[3] == "0" ? 0 : 180;
        }
        const auto [it, added] = rules_.upright.emplace(
            std::string(words[1]), Upright{reach == "up"     ? UprightReach::Up
                                           : reach == "down" ? UprightReach::Down
                                                             : UprightReach::In,
                                           angle, line_});
        if (!added) {
            error("'" + std::string(words[1]) + "' is upright already");
        }
    }

    std::optional<double> parseMm(std::string_view word) {
        double value = 0;
        const auto [end, ec] = std::from_chars(word.data(), word.data() + word.size(), value);
        if (ec != std::errc{} || end != word.data() + word.size() || value <= 0) {
            error("bad size '" + std::string(word) + "': millimetres");
            return std::nullopt;
        }
        return value;
    }

    void sizeRule(const std::vector<std::string_view>& words) {
        if (words.size() != 3) {
            return error("expected: size WIDTH DEPTH");
        }
        if (rules_.size) {
            return error("size given twice");
        }
        if (rules_.grid || rules_.petGrid) {
            return error("size does not go with grid");
        }
        const auto width = parseMm(words[1]);
        const auto depth = parseMm(words[2]);
        if (width && depth) {
            rules_.size = std::pair{*width, *depth};
        }
    }

    void atRule(const std::vector<std::string_view>& words) {
        if (words.size() != 4 && words.size() != 5) {
            return error("expected: at REF X Y [ANGLE]");
        }
        int angle = 0;
        if (words.size() == 5) {
            if (words[4] == "90") {
                angle = 90;
            } else if (words[4] == "180") {
                angle = 180;
            } else if (words[4] != "0") {
                return error("bad angle '" + std::string(words[4]) + "': 0, 90 or 180");
            }
        }
        double xy[2]{};
        for (int i = 0; i < 2; ++i) {
            const std::string_view word = words[2 + i];
            const auto [end, ec] = std::from_chars(word.data(), word.data() + word.size(), xy[i]);
            if (ec != std::errc{} || end != word.data() + word.size() || xy[i] < 0) {
                return error("bad position '" + std::string(word) + "': millimetres");
            }
        }
        if (!firstAt_) {
            firstAt_ = line_;
        }
        if (claim(words[1])) {
            rules_.rules.push_back({line_, AtRule{std::string(words[1]), xy[0], xy[1], angle}});
        }
    }

    void gridRule(const std::vector<std::string_view>& words) {
        if (words.size() == 2 && words[1] == "pet") {
            if (rules_.grid || rules_.petGrid) {
                return error("grid given twice");
            }
            if (rules_.size) {
                return error("grid does not go with size");
            }
            rules_.petGrid = true;
            return;
        }
        if (words.size() != 3) {
            return error("expected: grid COLUMNS ROWS, or grid pet");
        }
        if (rules_.grid || rules_.petGrid) {
            return error("grid given twice");
        }
        if (rules_.size) {
            return error("grid does not go with size");
        }
        std::int32_t size[2]{};
        for (int i = 0; i < 2; ++i) {
            const std::string_view word = words[1 + i];
            const auto [end, ec] = std::from_chars(word.data(), word.data() + word.size(), size[i]);
            if (ec != std::errc{} || end != word.data() + word.size() || size[i] < 1) {
                return error("bad grid size '" + std::string(word) + "'");
            }
        }
        rules_.grid = std::pair{size[0], size[1]};
    }

    std::string file_;
    std::uint32_t line_ = 0;
    PlaceRules rules_;
    std::set<std::string, std::less<>> placed_;
    std::optional<std::uint32_t> firstAt_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

std::string_view EdgeName(Edge edge) { return kEdgeNames[static_cast<std::size_t>(edge)]; }

std::expected<PlaceRules, std::vector<Diagnostic>> ParsePlaceRules(std::string_view text,
                                                                   std::string file) {
    Parser parser(std::move(file));
    std::uint32_t number = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        parser.line(++number, text.substr(start, end - start));
        start = end + 1;
    }
    return parser.result();
}

}  // namespace bw
