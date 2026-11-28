#include "machine/puzzle.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

#include "machine/repair.hpp"
#include "sim/time.hpp"
#include "text/text.hpp"

namespace bw {

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

namespace {

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

class Parser {
public:
    explicit Parser(std::string file) { puzzle_.file = std::move(file); }

    void line(std::uint32_t number, std::string_view text) {
        if (const std::vector<std::string_view> raw = SplitWords(text);
            !raw.empty() && (raw[0] == "title" || raw[0] == "brief")) {
            const std::size_t start = static_cast<std::size_t>(raw[0].data() - text.data());
            quoted(number, raw[0], text.substr(start + raw[0].size()));
            return;
        }
        if (const std::size_t hash = text.find('#'); hash != std::string_view::npos) {
            text = text.substr(0, hash);
        }
        const std::vector<std::string_view> tokens = SplitWords(text);
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
        } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "switch") {
            switchFault(number, tokens);
        } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "clock") {
            clockFault(number, tokens);
        } else if (tokens[0] == "fault") {
            error(number, "expected: fault stuck|crack|bridge|joint|cell|column|switch|clock ...");
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
        const std::string expected = "expected: fault stuck REF.PIN 0|1 [input]";
        const bool input = tokens.size() == 5 && tokens[4] == "input";
        if (tokens.size() != 4 && !input) {
            error(line, expected);
            return;
        }
        auto pin = ParsePinRef(tokens[2]);
        if (!pin || (tokens[3] != "0" && tokens[3] != "1")) {
            error(line, expected);
            return;
        }
        puzzle_.faults.push_back(StuckAtDecl{line, std::move(pin->reference), std::move(pin->pin),
                                             tokens[3] == "1" ? LogicValue::One : LogicValue::Zero,
                                             input});
    }

    void crack(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const auto a = tokens.size() == 5 ? ParsePinRef(tokens[2]) : std::nullopt;
        const auto b = tokens.size() == 5 ? ParsePinRef(tokens[3]) : std::nullopt;
        if (!a || !b || !tokens[4].ends_with('%')) {
            error(line, "expected: fault crack REF.PIN REF.PIN N%");
            return;
        }
        const auto percent = ParsePercent(tokens[4], 2);  // N%, N.D% or N.DD%
        if (!percent) {
            error(line, "bad position '" + std::string(tokens[4]) + "'");
            return;
        }
        puzzle_.faults.push_back(CrackDecl{line, *a, *b, *percent});
    }

    void bridge(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const auto a = tokens.size() == 4 ? ParsePinRef(tokens[2]) : std::nullopt;
        const auto b = tokens.size() == 4 ? ParsePinRef(tokens[3]) : std::nullopt;
        if (!a || !b) {
            error(line, "expected: fault bridge REF.PIN REF.PIN");
            return;
        }
        puzzle_.faults.push_back(BridgeDecl{line, *a, *b});
    }

    void joint(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const auto pin = tokens.size() == 3 ? ParsePinRef(tokens[2]) : std::nullopt;
        if (!pin) {
            error(line, "expected: fault joint REF.PIN");
            return;
        }
        puzzle_.faults.push_back(JointDecl{line, *pin});
    }

    // `fault switch REF closed|open`.
    void switchFault(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() != 4 || tokens[2].empty() ||
            (tokens[3] != "closed" && tokens[3] != "open")) {
            error(line, "expected: fault switch REF closed|open");
            return;
        }
        puzzle_.faults.push_back(SwitchDecl{line, std::string(tokens[2]), tokens[3] == "closed"});
    }

    // `fault clock REF FREQUENCY` (P8k): the frequency as an OSC's freq= takes it.
    void clockFault(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() != 4 || tokens[2].empty()) {
            error(line, "expected: fault clock REF FREQUENCY");
            return;
        }
        if (const auto period = ParseFrequencyPeriod(tokens[3]); !period) {
            error(line, period.error());
            return;
        }
        puzzle_.faults.push_back(ClockDecl{line, std::string(tokens[2]), std::string(tokens[3])});
    }

    // `fault cell REF.PIN [MEMORY] WORD 0|1` or `fault column REF.PIN [MEMORY] 0|1`.
    void cell(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        const bool column = tokens[1] == "column";
        const std::size_t count = column ? 4 : 5;
        const bool named = tokens.size() == count + 1;
        const auto pin = tokens.size() == count || named ? ParsePinRef(tokens[2]) : std::nullopt;
        const auto lower = [](std::string_view text) {
            return !text.empty() &&
                   std::ranges::all_of(text, [](char c) { return c >= 'a' && c <= 'z'; });
        };
        if (!pin || (named && !lower(tokens[3])) ||
            (tokens.back() != "0" && tokens.back() != "1")) {
            error(line, column ? "expected: fault column REF.PIN [MEMORY] 0|1"
                               : "expected: fault cell REF.PIN [MEMORY] WORD 0|1");
            return;
        }
        std::optional<std::uint32_t> word;
        if (!column) {
            const std::string_view text = tokens[tokens.size() - 2];
            word = ParseWord(text);
            if (!word) {
                error(line, "bad word '" + std::string(text) + "'");
                return;
            }
        }
        puzzle_.faults.push_back(CellDecl{line, *pin, word,
                                          tokens.back() == "1" ? LogicValue::One : LogicValue::Zero,
                                          named ? std::string(tokens[3]) : std::string()});
    }

    Puzzle puzzle_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

std::uint32_t LineOf(const FaultDecl& fault) {
    return std::visit([](const auto& decl) { return decl.line; }, fault);
}

namespace {

std::string PinText(const PinRef& pin) { return pin.reference + "." + pin.pin; }

char BitText(LogicValue value) { return value == LogicValue::One ? '1' : '0'; }

// "51", "51.5", "51.25": two decimals at most, as ParsePercent reads.
std::string PercentText(double percent) {
    std::string text = std::format("{:.2f}", percent);
    while (text.back() == '0') {
        text.pop_back();
    }
    if (text.back() == '.') {
        text.pop_back();
    }
    return text;
}

}  // namespace

std::string FormatFault(const FaultDecl& fault) {
    return std::visit(
        Overloaded{
            [](const StuckAtDecl& f) {
                return std::format("fault stuck {}.{} {}{}", f.reference, f.pin, BitText(f.value),
                                   f.input ? " input" : "");
            },
            [](const CrackDecl& f) {
                return std::format("fault crack {} {} {}%", PinText(f.a), PinText(f.b),
                                   PercentText(f.percent));
            },
            [](const BridgeDecl& f) {
                return std::format("fault bridge {} {}", PinText(f.a), PinText(f.b));
            },
            [](const JointDecl& f) { return std::format("fault joint {}", PinText(f.pin)); },
            [](const CellDecl& f) {
                const std::string memory = f.memory.empty() ? "" : " " + f.memory;
                return f.word ? std::format("fault cell {}{} ${:X} {}", PinText(f.pin), memory,
                                            *f.word, BitText(f.value))
                              : std::format("fault column {}{} {}", PinText(f.pin), memory,
                                            BitText(f.value));
            },
            [](const SwitchDecl& f) {
                return std::format("fault switch {} {}", f.reference, f.closed ? "closed" : "open");
            },
            [](const ClockDecl& f) {
                return std::format("fault clock {} {}", f.reference, f.frequency);
            },
        },
        fault);
}

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

}  // namespace bw
