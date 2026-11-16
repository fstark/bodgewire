#include "parts/zx80_keys.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace bw {

namespace {

constexpr std::array<Zx80Key, kZx80KeyCount> kKeys{{
    {0, 0, "SHIFT", "", ""},
    {0, 1, "Z", ":", ""},
    {0, 2, "X", ";", "CLEAR"},
    {0, 3, "C", "?", "CLS"},
    {0, 4, "V", "/", "GOSUB"},
    {1, 0, "A", "GRAPHIC", "LIST"},
    {1, 1, "S", "GRAPHIC", "STOP"},
    {1, 2, "D", "GRAPHIC", "DIM"},
    {1, 3, "F", "GRAPHIC", "FOR"},
    {1, 4, "G", "GRAPHIC", "GOTO"},
    {2, 0, "Q", "GRAPHIC", "NEW"},
    {2, 1, "W", "GRAPHIC", "LOAD"},
    {2, 2, "E", "GRAPHIC", "SAVE"},
    {2, 3, "R", "GRAPHIC", "RUN"},
    {2, 4, "T", "GRAPHIC", "CONT"},
    {3, 0, "1", "NOT", ""},
    {3, 1, "2", "AND", ""},
    {3, 2, "3", "THEN", ""},
    {3, 3, "4", "TO", ""},
    {3, 4, "5", "\xe2\x86\x90", ""},  // ←
    {4, 0, "0", "RUBOUT", ""},
    {4, 1, "9", "HOME", ""},
    {4, 2, "8", "\xe2\x86\x92", ""},  // →
    {4, 3, "7", "\xe2\x86\x91", ""},  // ↑
    {4, 4, "6", "\xe2\x86\x93", ""},  // ↓
    {5, 0, "P", "*", ""},
    {5, 1, "O", ")", "PRINT"},
    {5, 2, "I", "(", "INPUT"},
    {5, 3, "U", "$", "IF"},
    {5, 4, "Y", "\"", "REM"},
    {6, 0, "NEW LINE", "EDIT", ""},
    {6, 1, "L", "=", ""},
    {6, 2, "K", "+", "LET"},
    {6, 3, "J", "-", "RAND"},
    {6, 4, "H", "**", "POKE"},
    {7, 0, "SPACE", "\xc2\xa3", "BREAK"},  // £; BREAK is SPACE read without SHIFT
    {7, 1, ".", ",", ""},
    {7, 2, "M", ">", ""},
    {7, 3, "N", "<", "NEXT"},
    {7, 4, "B", "OR", "RET"},
}};

// A keyword's spellings in a listing, and the key's keyword legend they mean.
struct Spelling {
    std::string_view word, keyword;
};
constexpr std::array<Spelling, 6> kSpellings{{
    {"GOTO", "GOTO"},
    {"GOSUB", "GOSUB"},
    {"CONTINUE", "CONT"},
    {"RANDOMISE", "RAND"},
    {"RANDOMIZE", "RAND"},
    {"RETURN", "RET"},
}};

std::size_t KeyNamed(std::string_view legend) {
    const auto it = std::ranges::find(kKeys, legend, &Zx80Key::legend);
    return static_cast<std::size_t>(it - kKeys.begin());
}

char32_t Upper(char32_t c) { return c >= U'a' && c <= U'z' ? c - (U'a' - U'A') : c; }
bool IsLetter(char32_t c) { return Upper(c) >= U'A' && Upper(c) <= U'Z'; }

// The key whose keyword legend `word` is (the legends on the caps: GOSUB stands for V's GO SUB).
std::optional<std::size_t> KeywordKey(std::string_view word) {
    for (const Spelling& s : kSpellings) {
        if (s.word == word) {
            word = s.keyword;
            break;
        }
    }
    const auto it = std::ranges::find(kKeys, word, &Zx80Key::keyword);
    if (it == kKeys.end() || word.empty()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - kKeys.begin());
}

// The SHIFT key typing `word` as a token in L mode (THEN, TO, AND, OR, NOT).
std::optional<std::size_t> ShiftWordKey(std::string_view word) {
    const auto it = std::ranges::find(kKeys, word, &Zx80Key::shifted);
    if (it == kKeys.end() || it->shifted.size() < 2 || it->shifted[0] == '\xe2') {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - kKeys.begin());
}

}  // namespace

std::span<const Zx80Key> Zx80Keys() { return kKeys; }

std::optional<std::size_t> Zx80KeyAt(int row, int column) {
    for (std::size_t k = 0; k < kKeys.size(); ++k) {
        if (kKeys[k].row == row && kKeys[k].column == column) {
            return k;
        }
    }
    return std::nullopt;
}

std::optional<MatrixPress> Zx80KeyFor(char32_t c) {
    switch (c) {
    case '\n':
    case '\r':
        return MatrixPress{KeyNamed("NEW LINE")};
    case ' ':
        return MatrixPress{KeyNamed("SPACE")};
    case '\b':
    case kKeyDel:
        return MatrixPress{KeyNamed("0"), true};
    case kKeyHome:
        return MatrixPress{KeyNamed("9"), true};
    case kKeyLeft:
        return MatrixPress{KeyNamed("5"), true};
    case kKeyDown:
        return MatrixPress{KeyNamed("6"), true};
    case kKeyUp:
        return MatrixPress{KeyNamed("7"), true};
    case kKeyRight:
        return MatrixPress{KeyNamed("8"), true};
    case kKeyEdit:
        return MatrixPress{KeyNamed("NEW LINE"), true};
    case kKeyBreak:
        return MatrixPress{KeyNamed("SPACE")};  // read without SHIFT while a program runs
    case kPoundSign:
        return MatrixPress{KeyNamed("SPACE"), true};
    default:
        break;
    }
    c = Upper(c);
    if (c < 0x21 || c > 0x5A) {
        return std::nullopt;
    }
    const char ch = static_cast<char>(c);
    for (std::size_t k = 0; k < kKeys.size(); ++k) {
        if (kKeys[k].legend.size() == 1 && kKeys[k].legend[0] == ch) {
            return MatrixPress{k};
        }
        if (kKeys[k].shifted.size() == 1 && kKeys[k].shifted[0] == ch) {
            return MatrixPress{k, true};
        }
    }
    return std::nullopt;
}

std::vector<MatrixPress> Zx80Presses(std::u32string_view text) {
    std::vector<MatrixPress> presses;
    bool k = true;            // the cursor is K: a keyword expected
    bool inString = false;    // between quotes: letters are letters, spaces are typed
    bool afterToken = false;  // the last press was a keyword or a SHIFT word
    const auto type = [&](char32_t c) {
        if (const auto press = Zx80KeyFor(c)) {
            presses.push_back(*press);
        }
        afterToken = false;
    };
    // The letters from `from`, upper case; `end` past them.
    const auto wordAt = [&](std::size_t from, std::size_t& end) {
        std::string word;
        end = from;
        while (end < text.size() && IsLetter(text[end])) {
            word.push_back(static_cast<char>(Upper(text[end++])));
        }
        return word;
    };
    for (std::size_t i = 0; i < text.size();) {
        const char32_t c = text[i];
        if (c == U'\n' || c == U'\r') {
            type(c);
            k = true;
            inString = false;
            ++i;
            continue;
        }
        if (c == U'"') {
            type(c);
            inString = !inString;
            k = false;
            ++i;
            continue;
        }
        if (inString) {
            type(c);
            ++i;
            continue;
        }
        if (c == U' ') {
            // The editor spaces keywords itself: a space is typed only between two plain words.
            std::size_t end = 0;
            const std::string next = wordAt(i + 1, end);
            const bool beforeToken = !next.empty() && ShiftWordKey(next).has_value();
            if (!k && !beforeToken && !afterToken) {
                type(c);
            }
            ++i;
            continue;
        }
        if (c == U'*' && i + 1 < text.size() && text[i + 1] == U'*') {
            presses.push_back({KeyNamed("H"), true});
            afterToken = true;
            k = false;
            i += 2;
            continue;
        }
        if (!IsLetter(c)) {
            type(c);
            if (!(c >= U'0' && c <= U'9')) {
                k = false;  // a line number keeps K; a symbol ends it
            }
            ++i;
            continue;
        }
        std::size_t end = 0;
        std::string word = wordAt(i, end);
        if (k && word == "GO" && end + 1 < text.size() && text[end] == U' ') {
            // GO TO and GO SUB: one inner space.
            std::size_t more = 0;
            const std::string rest = wordAt(end + 1, more);
            if (rest == "TO" || rest == "SUB") {
                word += rest;
                end = more;
            }
        }
        if (k) {
            if (const auto key = KeywordKey(word)) {
                presses.push_back({*key});
                k = false;
                afterToken = true;
                i = end;
                continue;
            }
        } else if (const auto key = ShiftWordKey(word)) {
            presses.push_back({*key, true});
            k = word == "THEN";
            afterToken = true;
            i = end;
            continue;
        }
        for (const char ch : word) {
            type(static_cast<char32_t>(ch));
        }
        k = false;
        i = end;
    }
    return presses;
}

}  // namespace bw
