#include "machine/puzzle.hpp"

#include <optional>
#include <utility>

#include "board/kicad_board.hpp"

namespace bw {

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f'; }

std::vector<std::string_view> Tokens(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && IsSpace(line[i])) {
            ++i;
        }
        const std::size_t start = i;
        while (i < line.size() && !IsSpace(line[i])) {
            ++i;
        }
        if (i > start) {
            tokens.push_back(line.substr(start, i - start));
        }
    }
    return tokens;
}

bool IsMachineName(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

std::optional<Layer> ParseLayer(std::string_view text) {
    if (text == "F.Cu") {
        return Layer::Front;
    }
    if (text == "B.Cu") {
        return Layer::Back;
    }
    return std::nullopt;
}

class Parser {
public:
    explicit Parser(std::string file) { puzzle_.file = std::move(file); }

    void line(std::uint32_t number, std::string_view text) {
        if (const std::size_t hash = text.find('#'); hash != std::string_view::npos) {
            text = text.substr(0, hash);
        }
        const std::vector<std::string_view> tokens = Tokens(text);
        if (tokens.empty()) {
            return;
        }
        if (tokens[0] == "machine") {
            machine(number, tokens);
        } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "stuck") {
            stuck(number, tokens);
        } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "crack") {
            crack(number, tokens);
        } else if (tokens[0] == "fault") {
            error(number, "expected: fault stuck|crack ...");
        } else {
            error(number, "unknown directive '" + std::string(tokens[0]) + "'");
        }
    }

    std::expected<Puzzle, std::vector<Diagnostic>> finish() {
        if (puzzle_.machineLine == 0) {
            errors_.insert(errors_.begin(), {puzzle_.file, 0, Severity::Error, "no machine line"});
        }
        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        return std::move(puzzle_);
    }

private:
    void error(std::uint32_t line, std::string message) {
        errors_.push_back({puzzle_.file, line, Severity::Error, std::move(message)});
    }

    void machine(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() != 2) {
            error(line, "expected: machine NAME");
            return;
        }
        if (!IsMachineName(tokens[1])) {
            error(line, "bad machine name '" + std::string(tokens[1]) + "'");
            return;
        }
        if (puzzle_.machineLine != 0) {
            error(line, "machine already given at line " + std::to_string(puzzle_.machineLine));
            return;
        }
        puzzle_.machineLine = line;
        puzzle_.machine = tokens[1];
    }

    void stuck(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const std::string expected = "expected: fault stuck REF.PIN 0|1";
        if (tokens.size() != 4) {
            error(line, expected);
            return;
        }
        const std::size_t dot = tokens[2].find('.');
        if (dot == std::string_view::npos || dot == 0 || dot + 1 == tokens[2].size() ||
            (tokens[3] != "0" && tokens[3] != "1")) {
            error(line, expected);
            return;
        }
        puzzle_.stuck.push_back({line, std::string(tokens[2].substr(0, dot)),
                                 std::string(tokens[2].substr(dot + 1)),
                                 tokens[3] == "1" ? LogicValue::One : LogicValue::Zero});
    }

    void crack(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const auto layer = tokens.size() == 5 ? ParseLayer(tokens[2]) : std::nullopt;
        if (!layer) {
            error(line, "expected: fault crack F.Cu|B.Cu X Y");
            return;
        }
        const auto x = ParseMillimetres(tokens[3]);
        const auto y = ParseMillimetres(tokens[4]);
        if (!x || !y) {
            error(line, "bad coordinate '" + std::string(!x ? tokens[3] : tokens[4]) + "'");
            return;
        }
        puzzle_.cracks.push_back({line, *layer, {*x, *y}});
    }

    Puzzle puzzle_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

std::expected<Puzzle, std::vector<Diagnostic>> ParsePuzzle(std::string_view text,
                                                           std::string file) {
    Parser parser(std::move(file));
    std::uint32_t number = 1;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        parser.line(number++, text.substr(0, newline));
        if (newline == std::string_view::npos) {
            break;
        }
        text.remove_prefix(newline + 1);
    }
    return parser.finish();
}

std::string_view LayerName(Layer layer) { return layer == Layer::Front ? "F.Cu" : "B.Cu"; }

}  // namespace bw
