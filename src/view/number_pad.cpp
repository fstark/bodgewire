#include "view/number_pad.hpp"

#include <array>

#include "text/text.hpp"

namespace bw {

namespace {

constexpr std::array<std::string_view, 20> kHex{"C", "D", "E", "F", "8", "9", "A", "B", "4",  "5",
                                                "6", "7", "0", "1", "2", "3", "",  "<", "OK", "OK"};
constexpr std::array<std::string_view, 20> kPattern{"C", "D", "E", "F", "8",  "9", "A",
                                                    "B", "4", "5", "6", "7",  "0", "1",
                                                    "2", "3", "x", "<", "OK", "OK"};
constexpr std::array<std::string_view, 16> kDuration{"7", "8", "9", "<",  "4", "5", "6",  "ns",
                                                     "1", "2", "3", "us", "0", ".", "OK", "ms"};

bool IsHexDigit(char c) { return IsDigit(c) || (c >= 'A' && c <= 'F'); }

}  // namespace

std::span<const std::string_view> PadKeys(PadKind kind) {
    switch (kind) {
    case PadKind::Hex:
        return kHex;
    case PadKind::Pattern:
        return kPattern;
    case PadKind::Duration:
        return kDuration;
    }
    return {};
}

bool IsPadUnit(std::string_view key) { return key == "ns" || key == "us" || key == "ms"; }

std::string PadTyped(std::string text, std::string_view key, PadKind kind, std::size_t limit,
                     bool fresh) {
    if (key == kPadBackspace) {
        if (fresh) {
            return {};
        }
        if (!text.empty()) {
            text.pop_back();
        }
        return text;
    }
    if (IsPadUnit(key)) {
        if (kind != PadKind::Duration) {
            return text;
        }
        // The number the text starts with, then the unit: "3.2 µs" + "ns" is "3.2ns".
        std::size_t end = 0;
        while (end < text.size() && (IsDigit(text[end]) || text[end] == '.')) {
            ++end;
        }
        return end == 0 ? text : text.substr(0, end) + std::string(key);
    }
    if (key.size() != 1) {
        return text;  // OK, an empty cell
    }
    const char c = key.front();
    const bool allowed = kind == PadKind::Duration  ? IsDigit(c) || c == '.'
                         : kind == PadKind::Pattern ? IsHexDigit(c) || c == 'x'
                                                    : IsHexDigit(c);
    if (!allowed) {
        return text;
    }
    std::string base = fresh ? std::string() : text;
    if (kind == PadKind::Duration) {
        if (!base.empty() && !IsDigit(base.back()) && base.back() != '.') {
            return text;  // after its unit: nothing more
        }
        if (c == '.') {
            if (base.contains('.')) {
                return text;
            }
            if (base.empty()) {
                base = "0";
            }
        }
    }
    if (base.size() >= limit) {
        return text;
    }
    return base + c;
}

}  // namespace bw
