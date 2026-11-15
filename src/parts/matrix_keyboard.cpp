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

constexpr std::uint16_t kShiftBit = 0x8000;

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
    : Component(std::move(reference), ConnectorPins("MATRIXKB")), contacts_(kKeys.size()),
      waiting_(kMatrixWaiting) {}

bool MatrixKeyboard::type(char32_t c) {
    const auto press = MatrixKeyFor(c);
    if (!press) {
        return false;
    }
    const std::uint32_t head = head_.load(std::memory_order_relaxed);
    if (head - tail_.load(std::memory_order_acquire) >= kMatrixWaiting) {
        return false;
    }
    waiting_[head % kMatrixWaiting] =
        static_cast<std::uint16_t>(press->key | (press->shift ? kShiftBit : 0));
    head_.store(head + 1, std::memory_order_release);
    return true;
}

void MatrixKeyboard::dropWaiting() {
    dropBefore_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
}

bool MatrixKeyboard::scripted(std::size_t key) const {
    const std::uint32_t showing = showing_.load(std::memory_order_relaxed);
    if (showing == kNotShowing) {
        return false;
    }
    static const std::size_t shift = *Bw2001KeyAt(8, 0);
    return key == (showing & ~kShiftBit) || ((showing & kShiftBit) != 0 && key == shift);
}

void MatrixKeyboard::powerOn(PowerOnContext& /*ctx*/) {
    showing_.store(kNotShowing, std::memory_order_relaxed);
    phase_ = Phase::Idle;
    nextAt_ = 0;
    for (SwitchContact& contact : contacts_) {
        contact.reset(false);
    }
}

void MatrixKeyboard::evaluate(EvalContext& ctx) {
    // The scripted press's own timing; a wake from outside mid-press changes only the held keys.
    if (phase_ != Phase::Idle && ctx.now() >= nextAt_) {
        if (phase_ == Phase::Hold) {
            phase_ = Phase::Gap;
            nextAt_ = ctx.now() + kMatrixGap;
            ctx.wakeAfter(kMatrixGap);
        } else {
            phase_ = Phase::Idle;
        }
    }
    if (phase_ == Phase::Idle) {
        std::uint32_t tail = tail_.load(std::memory_order_relaxed);
        const std::uint32_t head = head_.load(std::memory_order_acquire);
        const std::uint32_t drop = dropBefore_.load(std::memory_order_acquire);
        if (static_cast<std::int32_t>(drop - tail) > 0) {
            tail = drop;  // wrap-safe: the counters only grow
        }
        if (tail != head) {
            const std::uint16_t code = waiting_[tail % kMatrixWaiting];
            ++tail;
            current_ = {static_cast<std::size_t>(code & ~kShiftBit), (code & kShiftBit) != 0};
            phase_ = Phase::Hold;
            nextAt_ = ctx.now() + kMatrixHold;
            ctx.wakeAfter(kMatrixHold);
        }
        tail_.store(tail, std::memory_order_release);
    }
    showing_.store(phase_ == Phase::Hold
                       ? static_cast<std::uint32_t>(current_.key | (current_.shift ? kShiftBit : 0))
                       : kNotShowing,
                   std::memory_order_relaxed);
    static const std::size_t shift = KeyNamed("SHIFT");  // the left one: row 8, column 0
    for (std::size_t k = 0; k < contacts_.size(); ++k) {
        const bool scripted =
            phase_ == Phase::Hold && (k == current_.key || (current_.shift && k == shift));
        contacts_[k].set(ctx, held(k) || scripted);
    }
}

}  // namespace bw
