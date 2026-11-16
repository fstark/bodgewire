#include "parts/matrix_keyboard.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "parts/connector.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

constexpr std::array<MatrixKey, kBw2001KeyCount> kKeys{{
    {0, 0, "!", ""},       {0, 1, "#", ""},
    {0, 2, "%", ""},       {0, 3, "&", ""},
    {0, 4, "(", ""},       {0, 5, "_", ""},
    {0, 6, "HOME", "CLR"}, {0, 7, "CRSR RT", "CRSR LF"},
    {1, 0, "\"", ""},      {1, 1, "$", ""},
    {1, 2, "'", ""},       {1, 3, "\\", ""},
    {1, 4, ")", ""},       {1, 6, "CRSR DN", "CRSR UP"},
    {1, 7, "DEL", "INST"}, {2, 0, "Q", ""},
    {2, 1, "E", ""},       {2, 2, "T", ""},
    {2, 3, "U", ""},       {2, 4, "O", ""},
    {2, 5, "^", ""},       {2, 6, "7", ""},
    {2, 7, "9", ""},       {3, 0, "W", ""},
    {3, 1, "R", ""},       {3, 2, "Y", ""},
    {3, 3, "I", ""},       {3, 4, "P", ""},
    {3, 6, "8", ""},       {3, 7, "/", ""},
    {4, 0, "A", ""},       {4, 1, "D", ""},
    {4, 2, "G", ""},       {4, 3, "J", ""},
    {4, 4, "L", ""},       {4, 6, "4", ""},
    {4, 7, "6", ""},       {5, 0, "S", ""},
    {5, 1, "F", ""},       {5, 2, "H", ""},
    {5, 3, "K", ""},       {5, 4, ":", ""},
    {5, 6, "5", ""},       {5, 7, "*", ""},
    {6, 0, "Z", ""},       {6, 1, "C", ""},
    {6, 2, "B", ""},       {6, 3, "M", ""},
    {6, 4, ";", ""},       {6, 5, "RETURN", ""},
    {6, 6, "1", ""},       {6, 7, "3", ""},
    {7, 0, "X", ""},       {7, 1, "V", ""},
    {7, 2, "N", ""},       {7, 3, ",", ""},
    {7, 4, "?", ""},       {7, 6, "2", ""},
    {7, 7, "+", ""},       {8, 0, "SHIFT", ""},
    {8, 1, "@", ""},       {8, 2, "]", ""},
    {8, 4, ">", ""},       {8, 5, "SHIFT", ""},
    {8, 6, "0", ""},       {8, 7, "-", ""},
    {9, 0, "RVS", ""},     {9, 1, "[", ""},
    {9, 2, "SPACE", ""},   {9, 3, "<", ""},
    {9, 4, "STOP", ""},    {9, 6, ".", ""},
    {9, 7, "=", ""},
}};

std::size_t KeyNamed(std::string_view legend) {
    const auto it = std::ranges::find(kKeys, legend, &MatrixKey::legend);
    Check(it != kKeys.end(), "matrix keyboard: no such key");
    return static_cast<std::size_t>(it - kKeys.begin());
}

}  // namespace

std::span<const MatrixKey> Bw2001Keys() { return kKeys; }

std::optional<std::size_t> Bw2001KeyAt(int row, int column) {
    for (std::size_t k = 0; k < kKeys.size(); ++k) {
        if (kKeys[k].row == row && kKeys[k].column == column) {
            return k;
        }
    }
    return std::nullopt;
}

std::optional<MatrixPress> MatrixKeyFor(char32_t c) {
    switch (c) {
    case kKeyHome:
        return MatrixPress{KeyNamed("HOME")};
    case kKeyClr:
        return MatrixPress{KeyNamed("HOME"), true};
    case kKeyDown:
        return MatrixPress{KeyNamed("CRSR DN")};
    case kKeyUp:
        return MatrixPress{KeyNamed("CRSR DN"), true};
    case kKeyRight:
        return MatrixPress{KeyNamed("CRSR RT")};
    case kKeyLeft:
        return MatrixPress{KeyNamed("CRSR RT"), true};
    case kKeyDel:
    case '\b':
        return MatrixPress{KeyNamed("DEL")};
    case kKeyInst:
        return MatrixPress{KeyNamed("DEL"), true};
    case kKeyStop:
        return MatrixPress{KeyNamed("STOP")};
    case kKeyRvs:
        return MatrixPress{KeyNamed("RVS")};
    case '\n':
    case '\r':
        return MatrixPress{KeyNamed("RETURN")};
    case ' ':
        return MatrixPress{KeyNamed("SPACE")};
    default:
        break;
    }
    if (c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    }
    if (c < 0x21 || c > 0x5F) {
        return std::nullopt;
    }
    const char ch = static_cast<char>(c);
    for (std::size_t k = 0; k < kKeys.size(); ++k) {
        if (kKeys[k].legend.size() == 1 && kKeys[k].legend[0] == ch) {
            return MatrixPress{k};
        }
    }
    return std::nullopt;
}

MatrixKeyboard::MatrixKeyboard(std::string reference)
    : Component(std::move(reference), ConnectorPins("MATRIXKB")), contacts_(kKeys.size()) {}

std::size_t MatrixKeyboard::shiftKey() const {
    static const std::size_t shift = KeyNamed("SHIFT");  // the left one: row 8, column 0
    return shift;
}

bool MatrixKeyboard::type(char32_t c) {
    const auto press = MatrixKeyFor(c);
    return press && queue_.push(*press);
}

bool MatrixKeyboard::type(std::u32string_view text) {
    bool any = false;
    for (const char32_t c : text) {
        any = type(c) || any;
    }
    return any;
}

void MatrixKeyboard::powerOn(PowerOnContext& /*ctx*/) {
    queue_.reset();
    for (SwitchContact& contact : contacts_) {
        contact.reset(false);
    }
}

void MatrixKeyboard::evaluate(EvalContext& ctx) {
    if (const auto wake = queue_.step(ctx.now())) {
        ctx.wakeAfter(*wake);
    }
    const auto current = queue_.current();
    const std::size_t shift = shiftKey();
    for (std::size_t k = 0; k < contacts_.size(); ++k) {
        const bool scripted = current && (k == current->key || (current->shift && k == shift));
        contacts_[k].set(ctx, held(k) || scripted);
    }
}

}  // namespace bw
