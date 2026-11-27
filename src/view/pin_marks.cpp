#include "view/pin_marks.hpp"

#include <cassert>

#include "text/text.hpp"

namespace bw {

namespace {

// "CLK", "CLK1", "1CLK", "CKA", "CKB", "PHI", "PHI0"–"PHI9".
bool ClockName(std::string_view name) {
    const auto digits = [](std::string_view s) {
        for (const char c : s) {
            if (!IsDigit(c)) {
                return false;
            }
        }
        return true;
    };
    if (name.ends_with("CLK") || name == "CKA" || name == "CKB") {
        return true;
    }
    if (name.starts_with("CLK")) {
        return digits(name.substr(3));
    }
    if (name.starts_with("PHI")) {
        return name.size() <= 4 && digits(name.substr(3));
    }
    return false;
}

// "3A" → "3", "1A1" → "1", "12Q" → "12"; "" for a shared pin ("CLK") or a number alone ("1").
std::string_view Section(std::string_view name) {
    std::size_t n = 0;
    while (n < name.size() && IsDigit(name[n])) {
        ++n;
    }
    return n > 0 && n < name.size() ? name.substr(0, n) : std::string_view{};
}

bool Marked(std::optional<PinKind> kind) {
    return kind && *kind != PinKind::Power && *kind != PinKind::NoConnect;
}

}  // namespace

PinLook LookOf(std::string_view name, std::optional<PinKind> kind, bool chip) {
    PinLook look;
    look.activeLow = name.ends_with("_N");
    look.clock = ClockName(name);
    if (!kind) {
        return look;
    }
    if (!chip) {
        const bool power =
            *kind == PinKind::Power || name.starts_with("VCC") || name.starts_with("GND");
        look.mark = power ? PinMark::Power : PinMark::Passive;
        return look;
    }
    switch (*kind) {
    case PinKind::Input:
        look.mark = PinMark::In;
        break;
    case PinKind::Output:
        look.mark = PinMark::Out;
        break;
    case PinKind::OpenCollector:
        look.mark = PinMark::OpenCollector;
        break;
    case PinKind::Bidirectional:
    case PinKind::PullUp:
        look.mark = PinMark::Both;
        break;
    case PinKind::Power:
        look.mark = PinMark::Power;
        break;
    case PinKind::NoConnect:
        look.mark = PinMark::NoConnect;
        break;
    case PinKind::Switched:
    case PinKind::Joined:
        look.mark = PinMark::Passive;
        break;
    }
    return look;
}

std::vector<std::size_t> WorksWith(std::span<const std::string> names,
                                   std::span<const std::optional<PinKind>> kinds, std::size_t pin) {
    assert(names.size() == kinds.size());
    if (pin >= names.size() || !Marked(kinds[pin])) {
        return {};
    }
    bool sections = false;
    for (std::size_t i = 0; i < names.size(); ++i) {
        sections = sections || (Marked(kinds[i]) && !Section(names[i]).empty());
    }
    if (!sections) {
        return {};
    }
    const std::string_view mine = Section(names[pin]);
    std::vector<std::size_t> with;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!Marked(kinds[i])) {
            continue;
        }
        const std::string_view theirs = Section(names[i]);
        // A section's pin: its own section and the shared pins; a shared pin: every section.
        const bool joined =
            mine.empty() ? (!theirs.empty() || i == pin) : (theirs == mine || theirs.empty());
        if (joined) {
            with.push_back(i);
        }
    }
    return with;
}

}  // namespace bw
