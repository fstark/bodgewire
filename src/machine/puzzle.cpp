#include "machine/puzzle.hpp"

#include <optional>
#include <utility>

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

std::optional<PinRef> ParsePin(std::string_view token) {
    const std::size_t dot = token.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == token.size()) {
        return std::nullopt;
    }
    return PinRef{std::string(token.substr(0, dot)), std::string(token.substr(dot + 1))};
}

// "N%", "N.D%" or "N.DD%", at most 100.
std::optional<double> ParsePercent(std::string_view text) {
    if (text.size() < 2 || text.back() != '%') {
        return std::nullopt;
    }
    text.remove_suffix(1);
    const std::size_t dot = text.find('.');
    const std::string_view whole = text.substr(0, dot);
    const std::string_view decimals =
        dot == std::string_view::npos ? std::string_view() : text.substr(dot + 1);
    if (whole.empty() || whole.size() > 3 || decimals.size() > 2 ||
        (dot != std::string_view::npos && decimals.empty())) {
        return std::nullopt;
    }
    double value = 0;
    for (const char c : whole) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    double scale = 1;
    for (const char c : decimals) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        scale /= 10;
        value += (c - '0') * scale;
    }
    if (value > 100) {
        return std::nullopt;
    }
    return value;
}

// "$1FF" (1-8 hex digits, either case) or "511" (1-9 decimal digits).
std::optional<std::uint32_t> ParseWord(std::string_view text) {
    const bool hex = text.starts_with('$');
    if (hex) {
        text.remove_prefix(1);
    }
    if (text.empty() || text.size() > (hex ? 8u : 9u)) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (const char c : text) {
        unsigned digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned>(c - '0');
        } else if (hex && c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned>(c - 'a' + 10);
        } else if (hex && c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned>(c - 'A' + 10);
        } else {
            return std::nullopt;
        }
        value = value * (hex ? 16u : 10u) + digit;
    }
    return value;
}

// `"TEXT"` then only spaces or a # comment; nullopt otherwise.
std::optional<std::string> QuotedValue(std::string_view rest) {
    std::size_t i = 0;
    while (i < rest.size() && IsSpace(rest[i])) {
        ++i;
    }
    if (i == rest.size() || rest[i] != '"') {
        return std::nullopt;
    }
    const std::size_t close = rest.find('"', i + 1);
    if (close == std::string_view::npos) {
        return std::nullopt;
    }
    for (std::size_t j = close + 1; j < rest.size(); ++j) {
        if (rest[j] == '#') {
            break;
        }
        if (!IsSpace(rest[j])) {
            return std::nullopt;
        }
    }
    return std::string(rest.substr(i + 1, close - i - 1));
}

class Parser {
public:
    explicit Parser(std::string file) { puzzle_.file = std::move(file); }

    void line(std::uint32_t number, std::string_view text) {
        if (const std::vector<std::string_view> raw = Tokens(text);
            !raw.empty() && (raw[0] == "title" || raw[0] == "brief")) {
            const std::size_t start = static_cast<std::size_t>(raw[0].data() - text.data());
            quoted(number, raw[0], text.substr(start + raw[0].size()));
            return;
        }
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
        } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "bridge") {
            bridge(number, tokens);
        } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "joint") {
            joint(number, tokens);
        } else if (tokens[0] == "fault" && tokens.size() >= 2 &&
                   (tokens[1] == "cell" || tokens[1] == "column")) {
            cell(number, tokens);
        } else if (tokens[0] == "fault") {
            error(number, "expected: fault stuck|crack|bridge|joint|cell|column ...");
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

    void quoted(std::uint32_t line, std::string_view keyword, std::string_view rest) {
        const bool isTitle = keyword == "title";
        std::uint32_t& given = isTitle ? puzzle_.titleLine : puzzle_.briefLine;
        const auto value = QuotedValue(rest);
        if (!value) {
            error(line, "expected: " + std::string(keyword) + " \"TEXT\"");
            return;
        }
        if (given != 0) {
            error(line, std::string(keyword) + " already given at line " + std::to_string(given));
            return;
        }
        given = line;
        (isTitle ? puzzle_.title : puzzle_.brief) = *value;
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
        const auto a = tokens.size() == 5 ? ParsePin(tokens[2]) : std::nullopt;
        const auto b = tokens.size() == 5 ? ParsePin(tokens[3]) : std::nullopt;
        if (!a || !b || !tokens[4].ends_with('%')) {
            error(line, "expected: fault crack REF.PIN REF.PIN N%");
            return;
        }
        const auto percent = ParsePercent(tokens[4]);
        if (!percent) {
            error(line, "bad position '" + std::string(tokens[4]) + "'");
            return;
        }
        puzzle_.cracks.push_back({line, *a, *b, *percent});
    }

    void bridge(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const auto a = tokens.size() == 4 ? ParsePin(tokens[2]) : std::nullopt;
        const auto b = tokens.size() == 4 ? ParsePin(tokens[3]) : std::nullopt;
        if (!a || !b) {
            error(line, "expected: fault bridge REF.PIN REF.PIN");
            return;
        }
        puzzle_.bridges.push_back({line, *a, *b});
    }

    void joint(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const auto pin = tokens.size() == 3 ? ParsePin(tokens[2]) : std::nullopt;
        if (!pin) {
            error(line, "expected: fault joint REF.PIN");
            return;
        }
        puzzle_.joints.push_back({line, *pin});
    }

    // `fault cell REF.PIN WORD 0|1` or `fault column REF.PIN 0|1`.
    void cell(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const bool column = tokens[1] == "column";
        const std::size_t count = column ? 4 : 5;
        const auto pin = tokens.size() == count ? ParsePin(tokens[2]) : std::nullopt;
        if (!pin || (tokens.back() != "0" && tokens.back() != "1")) {
            error(line, column ? "expected: fault column REF.PIN 0|1"
                               : "expected: fault cell REF.PIN WORD 0|1");
            return;
        }
        std::optional<std::uint32_t> word;
        if (!column) {
            word = ParseWord(tokens[3]);
            if (!word) {
                error(line, "bad word '" + std::string(tokens[3]) + "'");
                return;
            }
        }
        puzzle_.cells.push_back(
            {line, *pin, word, tokens.back() == "1" ? LogicValue::One : LogicValue::Zero});
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
