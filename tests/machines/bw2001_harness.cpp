#include "bw2001_harness.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <stdexcept>

namespace bw {

namespace {

// The spec's matrix, row 0 to 9, PB0 to PB7 (the real 901447-05 table at $E75C, MAME pet.cpp).
// Printable keys by their character; the others by name; "" unused.
constexpr std::array<std::array<std::string_view, 8>, 10> kLegends{{
    {"!", "#", "%", "&", "(", "_", "HOME", "CRSR RT"},
    {"\"", "$", "'", "\\", ")", "", "CRSR DN", "DEL"},
    {"Q", "E", "T", "U", "O", "^", "7", "9"},
    {"W", "R", "Y", "I", "P", "", "8", "/"},
    {"A", "D", "G", "J", "L", "", "4", "6"},
    {"S", "F", "H", "K", ":", "", "5", "*"},
    {"Z", "C", "B", "M", ";", "RETURN", "1", "3"},
    {"X", "V", "N", ",", "?", "", "2", "+"},
    {"SHIFT L", "@", "]", "", ">", "SHIFT R", "0", "-"},
    {"RVS", "[", "SPACE", "<", "STOP", "", ".", "="},
}};

constexpr std::uint64_t kHoldUs = 30'000, kGapUs = 30'000;

std::string Trimmed(std::string text) {
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

void DropBlankEnd(std::vector<std::string>& rows) {
    while (!rows.empty() && rows.back().empty()) {
        rows.pop_back();
    }
}

std::string Replace(std::string text, std::string_view from, std::string_view to) {
    for (std::size_t at = text.find(from); at != std::string::npos;
         at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
    return text;
}

}  // namespace

std::optional<Bw2001Key> Bw2001KeyFor(char c) {
    if (c < 0x20 || c > 0x5F) {
        return std::nullopt;
    }
    const std::string one(1, c);
    const std::string_view name = c == ' ' ? std::string_view("SPACE") : std::string_view(one);
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 8; ++column) {
            if (kLegends[row][column] == name) {
                return Bw2001Key{row, column};
            }
        }
    }
    return std::nullopt;
}

std::string_view Bw2001KeyLegend(Bw2001Key key) {
    if (key.row < 0 || key.row > 9 || key.column < 0 || key.column > 7) {
        throw std::invalid_argument("Bw2001KeyLegend: row 0-9, column 0-7");
    }
    return kLegends[key.row][key.column];
}

char Bw2001ScreenChar(std::uint8_t code) {
    code &= 0x7F;
    if (code < 0x20) {
        return static_cast<char>(code + 0x40);
    }
    if (code < 0x40) {
        return static_cast<char>(code);
    }
    return '?';  // graphics: nothing BASIC prints
}

void Bw2001Typist::press(Bw2001Key key, bool shift) {
    if (shift) {
        target_.setKey(kBw2001ShiftLeft, true);
    }
    target_.setKey(key, true);
    target_.run(kHoldUs);
    target_.setKey(key, false);
    if (shift) {
        target_.setKey(kBw2001ShiftLeft, false);
    }
    target_.run(kGapUs);
}

void Bw2001Typist::type(std::string_view text) {
    for (const char c : text) {
        const auto key = Bw2001KeyFor(c);
        if (!key) {
            throw std::invalid_argument(std::format("Bw2001Typist: no key types '{}'", c));
        }
        press(*key);
    }
}

void Bw2001Typist::enter(std::string_view text) {
    type(text);
    last_ = target_.ram(kBw2001Lines);
    press(kBw2001Return);
}

bool Bw2001Typist::waitForLine(std::uint64_t limitUs) {
    for (std::uint64_t waited = 0; waited <= limitUs; waited += 1'000) {
        if (target_.ram(kBw2001Waiting) == 1 && (!last_ || target_.ram(kBw2001Lines) != *last_)) {
            return true;
        }
        target_.run(1'000);
    }
    return false;
}

std::vector<std::string> Bw2001Typist::rows() const {
    std::vector<std::string> rows;
    for (std::size_t row = 0; row < 25; ++row) {
        std::string text;
        for (std::size_t column = 0; column < 40; ++column) {
            text += Bw2001ScreenChar(target_.screen(row * 40 + column));
        }
        rows.push_back(Trimmed(std::move(text)));
    }
    DropBlankEnd(rows);
    return rows;
}

std::optional<std::string> RunBw2001Session(Bw2001Target& target,
                                            const std::filesystem::path& path) {
    // P5o's format (docs/p5o-tiny-basic/TINY_BASIC.md, Conformance set); the lint is Python's.
    struct Row {
        std::string printed;
        std::optional<std::string> typed;
        bool breakAfter = false;
    };
    std::ifstream in(path);
    if (!in) {
        return std::format("cannot read {}", path.string());
    }
    std::vector<Row> rows;
    std::size_t cleared = 0;
    const std::string ram = std::to_string(kBw2001SessionRam);
    for (std::string line; std::getline(in, line);) {
        if (line == "~" || line.starts_with("~ ") || line == "~typed") {
            continue;
        }
        if (line == "~clear") {
            cleared = rows.size();
            continue;
        }
        if (line == "~break") {
            const auto typed = std::find_if(rows.rbegin(), rows.rend(),
                                            [](const Row& r) { return r.typed.has_value(); });
            if (typed == rows.rend()) {
                return "~break before any typed line";
            }
            typed->breakAfter = true;
            continue;
        }
        if (line.starts_with('~')) {
            return std::format("unknown directive '{}'", line);
        }
        line = Replace(line, "{RAM}", ram);
        const std::size_t bar = line.find('|');
        if (bar == std::string::npos) {
            rows.push_back({line, std::nullopt, false});
        } else {
            rows.push_back({line.substr(0, bar), line.substr(bar + 1), false});
        }
    }

    Bw2001Typist typist(target);
    if (!typist.waitForLine()) {
        return "BASIC never asked for a line after power-on";
    }
    typist.enter("CLS");
    for (const Row& row : rows) {
        if (!row.typed) {
            continue;
        }
        if (!typist.waitForLine()) {
            return std::format("BASIC never asked for a line before '{}'", *row.typed);
        }
        typist.enter(*row.typed);
        if (row.breakAfter) {
            target.run(100'000);
            typist.press(kBw2001Stop);
        }
    }
    if (!typist.waitForLine()) {
        return "BASIC never asked for a line at the end";
    }

    std::vector<std::string> expected;
    for (std::size_t i = cleared; i < rows.size(); ++i) {
        expected.push_back(Trimmed(rows[i].printed + rows[i].typed.value_or("")));
    }
    DropBlankEnd(expected);
    const std::vector<std::string> actual = typist.rows();
    for (std::size_t i = 0; i < std::max(expected.size(), actual.size()); ++i) {
        const std::string want = i < expected.size() ? expected[i] : "(nothing)";
        const std::string got = i < actual.size() ? actual[i] : "(nothing)";
        if (want != got) {
            std::string screen;
            for (const std::string& r : actual) {
                screen += r + "\n";
            }
            return std::format("row {}: expected '{}', got '{}'\n{}", i, want, got, screen);
        }
    }
    return std::nullopt;
}

std::vector<std::filesystem::path> Bw2001Sessions() {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(
             std::filesystem::path(BW_ROOT_DIR) / "tests/data/basic")) {
        if (entry.path().extension() == ".txt") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

}  // namespace bw
