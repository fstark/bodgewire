#include "asm/asm6502.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <utility>

#include "parts/mos6502_core.hpp"

namespace bw {

namespace {

using enum Mos6502Operand;

struct AsmError {
    std::string message;
};

[[noreturn]] void Fail(std::string message) { throw AsmError{std::move(message)}; }

// A value from an expression: unknown while it names a symbol not yet defined (pass 1).
struct Value {
    std::int64_t number = 0;
    bool known = true;
};

std::string Upper(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// The length of a 'c' literal starting at text[i], or 0.
std::size_t CharLiteralAt(std::string_view text, std::size_t i) {
    return i + 2 < text.size() && text[i] == '\'' && text[i + 2] == '\'' ? 3 : 0;
}

// The line without its ; comment (a ; inside "text" or 'c' does not count).
std::string_view StripComment(std::string_view line) {
    bool inString = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inString) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
        } else if (c == '"') {
            inString = true;
        } else if (const std::size_t n = CharLiteralAt(line, i); n != 0) {
            i += n - 1;
        } else if (c == ';') {
            return line.substr(0, i);
        }
    }
    return line;
}

// Splits on commas outside "text", 'c' and parentheses.
std::vector<std::string_view> SplitArguments(std::string_view text) {
    std::vector<std::string_view> parts;
    int depth = 0;
    bool inString = false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inString) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
        } else if (c == '"') {
            inString = true;
        } else if (const std::size_t n = CharLiteralAt(text, i); n != 0) {
            i += n - 1;
        } else if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
        } else if (c == ',' && depth == 0) {
            parts.push_back(Trim(text.substr(start, i - start)));
            start = i + 1;
        }
    }
    parts.push_back(Trim(text.substr(start)));
    return parts;
}

// The operand text without blanks outside 'c' literals, upper case outside them, for matching
// the forms; the expression is taken from the original text.
std::string Squeeze(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (const std::size_t n = CharLiteralAt(text, i); n != 0) {
            out.append(text.substr(i, n));
            i += n - 1;
        } else if (!std::isspace(static_cast<unsigned char>(text[i]))) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(text[i]))));
        }
    }
    return out;
}

std::string_view FormName(Mos6502Operand form) {
    switch (form) {
    case None:
        return "implied";
    case Accumulator:
        return "A";
    case Immediate:
        return "#";
    case ZeroPage:
        return "zp";
    case ZeroPageX:
        return "zp,X";
    case ZeroPageY:
        return "zp,Y";
    case Absolute:
        return "abs";
    case AbsoluteX:
        return "abs,X";
    case AbsoluteY:
        return "abs,Y";
    case Indirect:
        return "(abs)";
    case IndirectX:
        return "(zp,X)";
    case IndirectY:
        return "(zp),Y";
    case Relative:
        return "relative";
    }
    return "?";
}

std::size_t OperandBytes(Mos6502Operand form) {
    switch (form) {
    case None:
    case Accumulator:
        return 0;
    case Absolute:
    case AbsoluteX:
    case AbsoluteY:
    case Indirect:
        return 2;
    default:
        return 1;
    }
}

// (mnemonic, form) → opcode, documented opcodes only.
class Encodings {
public:
    Encodings() {
        for (int code = 0; code < 256; ++code) {
            const Mos6502Instruction i = Mos6502Decode(static_cast<std::uint8_t>(code));
            if (i.documented) {
                table_.emplace(Key(i.mnemonic, i.operand), static_cast<std::uint8_t>(code));
            }
        }
    }
    std::optional<std::uint8_t> find(std::string_view mnemonic, Mos6502Operand form) const {
        const auto it = table_.find(Key(mnemonic, form));
        if (it == table_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    bool knows(std::string_view mnemonic) const {
        const auto it = table_.lower_bound(Key(mnemonic, None));
        return it != table_.end() && it->first.first == mnemonic;
    }

private:
    using KeyType = std::pair<std::string, int>;
    static KeyType Key(std::string_view mnemonic, Mos6502Operand form) {
        return {std::string(mnemonic), static_cast<int>(form)};
    }
    std::map<KeyType, std::uint8_t> table_;
};

const Encodings& TheEncodings() {
    static const Encodings encodings;
    return encodings;
}

class Assembler {
public:
    Assembler(std::string_view source, std::string_view file,
              std::optional<std::filesystem::path> includeDir)
        : file_(file) {
        load(source, file_, includeDir, 0);
        forms_.resize(lines_.size(), None);
    }

    AssembledRom run() {
        pass(1);
        pass(2);
        if (!rom_) {
            Fail(std::format("{}: no .rom BASE, SIZE", file_));
        }
        if (!checksums_.empty()) {
            // Each byte making its window sum to 0; pass 2 again writes them into the text.
            for (Checksum& c : checksums_) {
                unsigned sum = 0;
                for (std::size_t i = c.base; i < c.base + c.size; ++i) {
                    sum += i == c.at ? 0u : words_[i];
                }
                c.value = static_cast<std::uint8_t>((256u - (sum & 0xFFu)) & 0xFFu);
            }
            pass(2);
        }
        return {static_cast<std::uint16_t>(base_), std::move(words_), std::move(text_)};
    }

private:
    // A source line: its text, the file it came from and its number there.
    struct Line {
        std::string_view text;
        std::string_view file;
        std::size_t number = 0;
        bool include = false;  // an .include line, already expanded below it
    };

    // The lines of `source`, each `.include "name"` followed by the named file's lines (resolved
    // against `dir`, nested up to 8 deep).
    void load(std::string_view source, std::string_view file,
              const std::optional<std::filesystem::path>& dir, int depth) {
        std::vector<std::string_view> lines;
        std::size_t start = 0;
        while (start <= source.size()) {
            const std::size_t end = source.find('\n', start);
            std::string_view line = source.substr(
                start, end == std::string_view::npos ? std::string_view::npos : end - start);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            lines.push_back(line);
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }
        if (!lines.empty() && lines.back().empty()) {
            lines.pop_back();  // the text's final newline
        }
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::string_view code = Trim(StripComment(lines[i]));
            const bool include = code.size() >= 8 && Upper(code.substr(0, 8)) == ".INCLUDE" &&
                                 (code.size() == 8 || !IsIdentChar(code[8]));
            lines_.push_back({lines[i], file, i + 1, include});
            if (!include) {
                continue;
            }
            const auto where = [&](std::string_view what) {
                return std::format("{} line {}: {}", file, i + 1, what);
            };
            const std::string_view arg = Trim(code.substr(8));
            if (arg.size() < 2 || !arg.starts_with('"') || !arg.ends_with('"')) {
                Fail(where(".include needs \"file\""));
            }
            if (!dir) {
                Fail(where(".include needs a file on disk"));
            }
            if (depth >= 8) {
                Fail(where(".include nested too deep"));
            }
            const std::string_view name = arg.substr(1, arg.size() - 2);
            const std::filesystem::path path = *dir / std::string(name);
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                Fail(where(std::format("cannot read '{}'", name)));
            }
            std::ostringstream text;
            text << in.rdbuf();
            texts_.push_back(text.str());
            names_.push_back(path.filename().string());
            std::filesystem::path inner = path.parent_path();
            load(texts_.back(), names_.back(), inner.empty() ? "." : inner, depth + 1);
        }
    }

    void pass(int which) {
        pass_ = which;
        pc_.reset();
        scope_.clear();
        emitted_.clear();
        if (pass_ == 2) {
            std::fill(words_.begin(), words_.end(), std::uint8_t{0xFF});
            std::fill(written_.begin(), written_.end(), false);
            text_.clear();
        }
        checksumIndex_ = 0;
        for (line_ = 0; line_ < lines_.size(); ++line_) {
            try {
                if (!lines_[line_].include) {
                    statement(lines_[line_].text);
                }
            } catch (const AsmError& error) {
                Fail(std::format("{} line {}: {}", lines_[line_].file, lines_[line_].number,
                                 error.message));
            }
            if (pass_ == 2) {
                writeLine();
            }
            emitted_.clear();
        }
    }

    // --- statements -------------------------------------------------------------------------

    void statement(std::string_view line) {
        std::string_view code = Trim(StripComment(line));
        if (const auto label = labelAt(code)) {
            defineLabel(label->first);
            code = Trim(code.substr(label->second));
        }
        if (code.empty()) {
            return;
        }
        if (code.front() == '.') {
            directive(code);
            return;
        }
        std::size_t word = 0;
        while (word < code.size() && IsIdentChar(code[word])) {
            ++word;
        }
        const std::string_view rest = Trim(code.substr(word));
        if (word > 0 && IsIdentStart(code.front()) && !rest.empty() && rest.front() == '=') {
            defineConstant(code.substr(0, word), Trim(rest.substr(1)));
            return;
        }
        instruction(code.substr(0, word), rest);
    }

    // A label at the start of `code`: its name and the length up to and with the colon.
    static std::optional<std::pair<std::string_view, std::size_t>> labelAt(std::string_view code) {
        std::size_t i = code.starts_with('@') ? 1 : 0;
        if (i >= code.size() || !IsIdentStart(code[i])) {
            return std::nullopt;
        }
        while (i < code.size() && IsIdentChar(code[i])) {
            ++i;
        }
        if (i < code.size() && code[i] == ':') {
            return std::pair{code.substr(0, i), i + 1};
        }
        return std::nullopt;
    }

    std::string symbolName(std::string_view name) const {
        if (!name.starts_with('@')) {
            return std::string(name);
        }
        if (scope_.empty()) {
            Fail(std::format("local label '{}' before any label", name));
        }
        return scope_ + std::string(name);
    }

    void defineLabel(std::string_view name) {
        if (!name.starts_with('@')) {
            scope_ = std::string(name);
        }
        if (!pc_) {
            Fail(std::format("label '{}' before any .org", name));
        }
        define(symbolName(name), *pc_);
    }

    void defineConstant(std::string_view name, std::string_view text) {
        const Value value = expression(text);
        if (!value.known) {
            Fail(std::format("'{}' needs a value known where it is defined", name));
        }
        define(std::string(name), value.number);
    }

    void define(const std::string& name, std::int64_t value) {
        if (pass_ == 1) {
            if (!symbols_.emplace(name, value).second) {
                Fail(std::format("'{}' defined twice", name));
            }
        }
    }

    void directive(std::string_view code) {
        std::size_t word = 1;
        while (word < code.size() && IsIdentChar(code[word])) {
            ++word;
        }
        const std::string name = Upper(code.substr(0, word));
        const std::string_view args = Trim(code.substr(word));
        if (name == ".ORG") {
            const Value v = expression(args);
            if (!v.known) {
                Fail(".org needs a value known here");
            }
            checkRange(v.number, 0, 0xFFFF, ".org");
            pc_ = v.number;
        } else if (name == ".ROM") {
            romDirective(args);
        } else if (name == ".BYTE") {
            for (const std::string_view arg : SplitArguments(args)) {
                if (arg.starts_with('"')) {
                    for (const char c : stringLiteral(arg)) {
                        emit(static_cast<std::uint8_t>(c));
                    }
                } else {
                    const Value v = expression(arg);
                    checkKnownRange(v, -128, 255, ".byte");
                    emit(static_cast<std::uint8_t>(v.number & 0xFF));
                }
            }
        } else if (name == ".CHECKSUM") {
            checksumDirective(args);
        } else if (name == ".WORD") {
            for (const std::string_view arg : SplitArguments(args)) {
                const Value v = expression(arg);
                checkKnownRange(v, 0, 0xFFFF, ".word");
                emit(static_cast<std::uint8_t>(v.number & 0xFF));
                emit(static_cast<std::uint8_t>((v.number >> 8) & 0xFF));
            }
        } else {
            Fail(std::format("unknown directive '{}'", code.substr(0, word)));
        }
    }

    // .checksum (the whole .rom window) or .checksum BASE, SIZE (that window): one byte making
    // the window sum to 0 mod 256. Windows are taken in pass 1; pass 2 emits their values.
    void checksumDirective(std::string_view args) {
        const bool whole = args.empty();
        const auto parts = whole ? std::vector<std::string_view>{} : SplitArguments(args);
        if (!whole && parts.size() != 2) {
            Fail(".checksum takes nothing or BASE, SIZE");
        }
        if (pass_ == 2) {
            emit(checksumIndex_ < checksums_.size() ? checksums_[checksumIndex_++].value : 0);
            return;
        }
        if (pc_ && rom_) {
            if (whole ? !checksums_.empty()
                      : std::any_of(checksums_.begin(), checksums_.end(),
                                    [](const Checksum& c) { return c.whole; })) {
                Fail(".checksum given twice");
            }
            std::int64_t base = base_;
            std::int64_t size = static_cast<std::int64_t>(words_.size());
            if (!whole) {
                const Value b = expression(parts[0]);
                const Value n = expression(parts[1]);
                if (!b.known || !n.known) {
                    Fail(".checksum needs values known here");
                }
                base = b.number;
                size = n.number;
                if (size < 1 || base < base_ ||
                    base + size > base_ + static_cast<std::int64_t>(words_.size())) {
                    Fail(".checksum window outside .rom");
                }
                if (*pc_ < base || *pc_ >= base + size) {
                    Fail(".checksum byte outside its window");
                }
            }
            const auto from = static_cast<std::size_t>(base - base_);
            const auto count = static_cast<std::size_t>(size);
            for (const Checksum& c : checksums_) {
                if (from < c.base + c.size && c.base < from + count) {
                    Fail(".checksum windows overlap");
                }
            }
            checksums_.push_back({static_cast<std::size_t>(*pc_ - base_), from, count, whole});
        }
        emit(0);
    }

    void romDirective(std::string_view args) {
        const auto parts = SplitArguments(args);
        if (parts.size() != 2) {
            Fail(".rom needs BASE, SIZE");
        }
        if (rom_ && pass_ == 1) {
            Fail(".rom given twice");
        }
        const Value base = expression(parts[0]);
        const Value size = expression(parts[1]);
        if (!base.known || !size.known) {
            Fail(".rom needs values known here");
        }
        if (size.number < 16 || size.number > 0x10000 || (size.number & (size.number - 1)) != 0) {
            Fail(".rom SIZE must be a power of two from 16 to 65536");
        }
        if (base.number < 0 || base.number + size.number > 0x10000) {
            Fail(".rom window must lie in $0000-$FFFF");
        }
        if (pass_ == 1) {
            rom_ = true;
            base_ = base.number;
            words_.assign(static_cast<std::size_t>(size.number), 0xFF);
            written_.assign(words_.size(), false);
            digits_ = 3;
            while ((std::int64_t{1} << (4 * digits_)) < size.number) {
                ++digits_;
            }
        }
    }

    std::string stringLiteral(std::string_view arg) const {
        if (arg.size() < 2 || !arg.ends_with('"')) {
            Fail("unterminated string");
        }
        std::string out;
        for (std::size_t i = 1; i + 1 < arg.size(); ++i) {
            char c = arg[i];
            if (c == '\\') {
                if (i + 2 >= arg.size() || (arg[i + 1] != '"' && arg[i + 1] != '\\')) {
                    Fail("only \\\" and \\\\ escapes in a string");
                }
                c = arg[++i];
            } else if (c == '"') {
                Fail("unescaped \" in a string");
            }
            out.push_back(c);
        }
        return out;
    }

    void instruction(std::string_view word, std::string_view operandText) {
        const Encodings& encodings = TheEncodings();
        const std::string mnemonic = Upper(word);
        if (word.empty() || !encodings.knows(mnemonic)) {
            const std::string_view shown = word.empty() ? Trim(operandText) : word;
            Fail(std::format("unknown instruction '{}'", shown));
        }
        Mos6502Operand form = None;
        Value value{};
        if (pass_ == 1) {
            const auto [chosen, v] = chooseForm(mnemonic, operandText);
            form = chosen;
            value = v;
            forms_[line_] = form;
        } else {
            form = forms_[line_];
            value = operandValue(form, operandText);
        }
        const std::optional<std::uint8_t> opcode = encodings.find(mnemonic, form);
        if (!opcode) {
            Fail(std::format("{} has no {} form", mnemonic, FormName(form)));
        }
        if (!pc_) {
            Fail("instruction before any .org");
        }
        const std::int64_t at = *pc_;
        emit(*opcode);
        switch (OperandBytes(form)) {
        case 0:
            break;
        case 1: {
            std::int64_t byte = value.number;
            if (form == Relative) {
                checkKnownRange(value, 0, 0xFFFF, "branch target");
                byte = value.number - (at + 2);
                if (pass_ == 2 && (byte < -128 || byte > 127)) {
                    Fail(std::format("branch out of range ({} bytes)", byte));
                }
            } else if (form == Immediate) {
                checkKnownRange(value, -128, 255, "immediate");
            } else {
                checkKnownRange(value, 0, 255, FormName(form));
            }
            emit(static_cast<std::uint8_t>(byte & 0xFF));
            break;
        }
        default:
            checkKnownRange(value, 0, 0xFFFF, FormName(form));
            emit(static_cast<std::uint8_t>(value.number & 0xFF));
            emit(static_cast<std::uint8_t>((value.number >> 8) & 0xFF));
        }
    }

    // Pass 1: the form from the operand's syntax and, between zero page and absolute, its value.
    std::pair<Mos6502Operand, Value> chooseForm(const std::string& mnemonic,
                                                std::string_view text) {
        const Encodings& encodings = TheEncodings();
        const auto has = [&](Mos6502Operand form) { return encodings.find(mnemonic, form); };
        const std::string s = Squeeze(text);
        if (s.empty()) {
            return {has(None) || !has(Accumulator) ? None : Accumulator, {}};
        }
        if (s == "A") {
            return {Accumulator, {}};
        }
        const auto [syntax, expr] = split(text);
        const Value v = expression(expr);
        const bool small = v.known && v.number >= 0 && v.number <= 0xFF;
        const auto pick = [&](Mos6502Operand zp, Mos6502Operand abs) {
            if (small && has(zp)) {
                return zp;
            }
            if (!has(abs) && has(zp) && !v.known) {
                return zp;  // a forward reference: checked in pass 2
            }
            return abs;
        };
        switch (syntax) {
        case ZeroPage:  // plain
            if (has(Relative)) {
                return {Relative, v};
            }
            return {pick(ZeroPage, Absolute), v};
        case ZeroPageX:
            return {pick(ZeroPageX, AbsoluteX), v};
        case ZeroPageY:
            return {pick(ZeroPageY, AbsoluteY), v};
        default:
            return {syntax, v};
        }
    }

    Value operandValue(Mos6502Operand form, std::string_view text) {
        if (form == None || form == Accumulator) {
            return {};
        }
        return expression(split(text).second);
    }

    // The operand's syntax (ZeroPage standing for plain, ZeroPageX/Y for indexed) and its
    // expression text.
    static std::pair<Mos6502Operand, std::string_view> split(std::string_view text) {
        text = Trim(text);
        const std::string s = Squeeze(text);
        if (text.starts_with('#')) {
            return {Immediate, text.substr(1)};
        }
        if (text.starts_with('(')) {
            const std::size_t close = text.rfind(')');
            if (close == std::string_view::npos) {
                Fail("missing )");
            }
            if (s.ends_with(",X)")) {
                const std::size_t comma = text.rfind(',');
                return {IndirectX, text.substr(1, comma - 1)};
            }
            if (s.ends_with("),Y")) {
                return {IndirectY, text.substr(1, close - 1)};
            }
            if (s.ends_with(")")) {
                return {Indirect, text.substr(1, close - 1)};
            }
            Fail(std::format("bad operand '{}'", text));
        }
        if (s.ends_with(",X") || s.ends_with(",Y")) {
            const std::size_t comma = text.rfind(',');
            return {s.back() == 'X' ? ZeroPageX : ZeroPageY, text.substr(0, comma)};
        }
        return {ZeroPage, text};
    }

    // --- expressions ------------------------------------------------------------------------

    Value expression(std::string_view text) {
        text_in_ = text;
        pos_ = 0;
        skipBlanks();
        if (pos_ >= text_in_.size()) {
            Fail("missing value");
        }
        const Value v = binary(0);
        skipBlanks();
        if (pos_ < text_in_.size()) {
            Fail(std::format("unexpected '{}' in '{}'", text_in_.substr(pos_), Trim(text)));
        }
        return v;
    }

    void skipBlanks() {
        while (pos_ < text_in_.size() && std::isspace(static_cast<unsigned char>(text_in_[pos_]))) {
            ++pos_;
        }
    }

    bool accept(std::string_view token) {
        skipBlanks();
        if (text_in_.substr(pos_).starts_with(token)) {
            pos_ += token.size();
            return true;
        }
        return false;
    }

    // The binary operator at pos_ of `level` (0 = |, 1 = ^, 2 = &, 3 = << >>, 4 = + -, 5 = * /).
    std::string_view binaryOperator(int level) {
        skipBlanks();
        const std::string_view rest = text_in_.substr(pos_);
        static constexpr std::array<std::array<std::string_view, 2>, 6> kLevels{
            {{"|", ""}, {"^", ""}, {"&", ""}, {"<<", ">>"}, {"+", "-"}, {"*", "/"}}};
        for (const std::string_view op : kLevels[static_cast<std::size_t>(level)]) {
            if (!op.empty() && rest.starts_with(op)) {
                return op;
            }
        }
        return {};
    }

    Value binary(int level) {
        if (level == 6) {
            return unary();
        }
        Value left = binary(level + 1);
        for (std::string_view op = binaryOperator(level); !op.empty(); op = binaryOperator(level)) {
            pos_ += op.size();
            const Value right = binary(level + 1);
            left = combine(left, op, right);
        }
        return left;
    }

    static Value combine(Value a, std::string_view op, Value b) {
        const bool known = a.known && b.known;
        const std::int64_t x = a.number, y = b.number;
        std::int64_t r = 0;
        if (op == "|") {
            r = x | y;
        } else if (op == "^") {
            r = x ^ y;
        } else if (op == "&") {
            r = x & y;
        } else if (op == "<<") {
            r = y >= 0 && y < 32 ? x << y : 0;
        } else if (op == ">>") {
            r = y >= 0 && y < 32 ? x >> y : 0;
        } else if (op == "+") {
            r = x + y;
        } else if (op == "-") {
            r = x - y;
        } else if (op == "*") {
            r = x * y;
        } else {  // "/"
            if (known && y == 0) {
                Fail("division by zero");
            }
            r = y == 0 ? 0 : x / y;
        }
        return {known ? r : 0, known};
    }

    Value unary() {
        skipBlanks();
        if (pos_ < text_in_.size()) {
            const char c = text_in_[pos_];
            const bool shift = text_in_.substr(pos_).starts_with("<<");
            if (c == '-' || c == '~' || c == '>' || (c == '<' && !shift)) {
                ++pos_;
                const Value v = unary();
                std::int64_t r = v.number;
                if (c == '-') {
                    r = -r;
                } else if (c == '~') {
                    r = ~r;
                } else if (c == '<') {
                    r &= 0xFF;
                } else {
                    r = (r >> 8) & 0xFF;
                }
                return {v.known ? r : 0, v.known};
            }
        }
        return primary();
    }

    Value primary() {
        skipBlanks();
        if (pos_ >= text_in_.size()) {
            Fail("missing value");
        }
        const std::string_view rest = text_in_.substr(pos_);
        const char c = rest.front();
        if (c == '(') {
            ++pos_;
            const Value v = binary(0);
            if (!accept(")")) {
                Fail("missing )");
            }
            return v;
        }
        if (c == '*') {
            ++pos_;
            if (!pc_) {
                Fail("'*' before any .org");
            }
            return {*pc_, true};
        }
        if (CharLiteralAt(rest, 0) != 0) {
            pos_ += 3;
            return {static_cast<unsigned char>(rest[1]), true};
        }
        if (c == '$' || c == '%') {
            const int radix = c == '$' ? 16 : 2;
            std::size_t i = 1;
            std::int64_t n = 0;
            for (; i < rest.size(); ++i) {
                const int digit = DigitValue(rest[i]);
                if (digit < 0 || digit >= radix) {
                    break;
                }
                n = n * radix + digit;
                if (n > 0xFFFFFFFF) {
                    Fail("number too large");
                }
            }
            if (i == 1) {
                Fail(std::format("bad number '{}'", rest.substr(0, 2)));
            }
            pos_ += i;
            return {n, true};
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t i = 0;
            std::int64_t n = 0;
            for (; i < rest.size() && std::isdigit(static_cast<unsigned char>(rest[i])); ++i) {
                n = n * 10 + (rest[i] - '0');
                if (n > 0xFFFFFFFF) {
                    Fail("number too large");
                }
            }
            if (i < rest.size() && IsIdentChar(rest[i])) {
                Fail(std::format("bad number '{}'", rest.substr(0, i + 1)));
            }
            pos_ += i;
            return {n, true};
        }
        if (IsIdentStart(c) || (c == '@' && rest.size() > 1 && IsIdentStart(rest[1]))) {
            std::size_t i = 1;
            while (i < rest.size() && IsIdentChar(rest[i])) {
                ++i;
            }
            pos_ += i;
            const std::string name = symbolName(rest.substr(0, i));
            const auto it = symbols_.find(name);
            if (it != symbols_.end()) {
                return {it->second, true};
            }
            if (pass_ == 2) {
                Fail(std::format("undefined symbol '{}'", rest.substr(0, i)));
            }
            return {0, false};
        }
        Fail(std::format("unexpected '{}'", rest));
    }

    static int DigitValue(char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    }

    // --- output -----------------------------------------------------------------------------

    void checkRange(std::int64_t v, std::int64_t low, std::int64_t high,
                    std::string_view what) const {
        if (v < low || v > high) {
            Fail(std::format("{} value {} out of range", what, v));
        }
    }

    void checkKnownRange(Value v, std::int64_t low, std::int64_t high,
                         std::string_view what) const {
        if (pass_ == 2) {
            checkRange(v.number, low, high, what);
        }
    }

    void emit(std::uint8_t byte) {
        if (!pc_) {
            Fail("byte before any .org");
        }
        if (*pc_ > 0xFFFF) {
            Fail("past $FFFF");
        }
        if (!rom_) {
            Fail("no .rom BASE, SIZE before the first byte");
        }
        if (pass_ == 2) {
            const std::int64_t offset = *pc_ - base_;
            if (offset < 0 || offset >= static_cast<std::int64_t>(words_.size())) {
                Fail(std::format("${:04X} is outside the .rom window ${:04X}-${:04X}", *pc_, base_,
                                 base_ + static_cast<std::int64_t>(words_.size()) - 1));
            }
            const auto at = static_cast<std::size_t>(offset);
            if (written_[at]) {
                Fail(std::format("${:04X} written twice", *pc_));
            }
            written_[at] = true;
            words_[at] = byte;
            if (emitted_.empty()) {
                lineAddress_ = *pc_;
            }
            emitted_.push_back(byte);
        }
        ++*pc_;
    }

    void writeLine() {
        const std::string_view source =
            Trim(lines_[line_].text).empty() ? std::string_view{} : lines_[line_].text;
        std::string_view shown = source;
        while (!shown.empty() && std::isspace(static_cast<unsigned char>(shown.back()))) {
            shown.remove_suffix(1);
        }
        if (line_ == 0) {
            text_ += std::format(
                "; Generated by bw asm from {}: do not edit. Edit the .asm and run bw asm again.\n",
                file_);
        }
        if (emitted_.empty()) {
            text_ += shown.empty() ? "\n" : std::format("; {}\n", shown);
            return;
        }
        const std::size_t width = 1 + static_cast<std::size_t>(digits_) + 3 * 3;
        for (std::size_t i = 0; i < emitted_.size(); i += 16) {
            std::string row = std::format(
                "@{:0{}X}", lineAddress_ - base_ + static_cast<std::int64_t>(i), digits_);
            for (std::size_t j = i; j < std::min(emitted_.size(), i + 16); ++j) {
                row += std::format(" {:02X}", emitted_[j]);
            }
            if (i == 0) {
                row.resize(std::max(row.size(), width), ' ');
                row += std::format("  ; {:04X}  {}", lineAddress_, shown);
            }
            text_ += row + "\n";
        }
    }

    std::string_view file_;
    std::vector<Line> lines_;
    std::deque<std::string> texts_;      // included files' texts, which lines_ views
    std::deque<std::string> names_;      // their file names
    std::vector<Mos6502Operand> forms_;  // per line, chosen in pass 1
    std::map<std::string, std::int64_t> symbols_;
    int pass_ = 1;
    std::size_t line_ = 0;
    std::optional<std::int64_t> pc_;
    std::string scope_;

    bool rom_ = false;
    std::int64_t base_ = 0;
    int digits_ = 3;
    std::vector<std::uint8_t> words_;
    std::vector<bool> written_;
    struct Checksum {
        std::size_t at = 0;    // offset of the byte in words_
        std::size_t base = 0;  // its window, as offsets in words_
        std::size_t size = 0;
        bool whole = false;      // the plain form
        std::uint8_t value = 0;  // 0 until the windows have been summed
    };
    std::vector<Checksum> checksums_;  // in source order
    std::size_t checksumIndex_ = 0;    // the next one pass 2 emits

    std::vector<std::uint8_t> emitted_;  // this line's bytes (pass 2)
    std::int64_t lineAddress_ = 0;
    std::string text_;

    std::string_view text_in_;  // the expression being parsed
    std::size_t pos_ = 0;
};

}  // namespace

std::expected<AssembledRom, std::string>
Assemble6502(std::string_view source, std::string_view file,
             std::optional<std::filesystem::path> includeDir) {
    try {
        return Assembler(source, file, std::move(includeDir)).run();
    } catch (const AsmError& error) {
        return std::unexpected(error.message);
    }
}

std::expected<AssembledRom, std::string> Assemble6502File(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(std::format("cannot read '{}'", path.string()));
    }
    std::ostringstream text;
    text << in.rdbuf();
    const std::filesystem::path dir = path.parent_path();
    return Assemble6502(text.str(), path.filename().string(), dir.empty() ? "." : dir);
}

}  // namespace bw
