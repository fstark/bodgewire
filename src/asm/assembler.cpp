#include "asm/assembler.hpp"

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

#include "asm/asm6502.hpp"
#include "asm/asmz80.hpp"
#include "asm/instruction_set.hpp"

namespace bw {

namespace asm_detail {

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

namespace {

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

}  // namespace

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

namespace {

class Assembler final : public AsmContext {
public:
    Assembler(std::string_view source, std::string_view file,
              std::optional<std::filesystem::path> includeDir)
        : file_(file) {
        load(source, file_, includeDir, 0);
        memos_.resize(lines_.size(), 0);
    }

    AssembledRom run() {
        runPass(1);
        runPass(2);
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
            runPass(2);
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

    void runPass(int which) {
        pass_ = which;
        pc_.reset();
        scope_.clear();
        emitted_.clear();
        cpu_ = &Mos6502Instructions();
        instructionSeen_ = false;
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
        const AsmValue value = expression(text);
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
        if (name == ".CPU") {
            cpuDirective(args);
        } else if (name == ".ORG") {
            const AsmValue v = expression(args);
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
                    const AsmValue v = expression(arg);
                    checkKnownRange(v, -128, 255, ".byte");
                    emit(static_cast<std::uint8_t>(v.number & 0xFF));
                }
            }
        } else if (name == ".CHECKSUM") {
            checksumDirective(args);
        } else if (name == ".WORD") {
            for (const std::string_view arg : SplitArguments(args)) {
                const AsmValue v = expression(arg);
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
                const AsmValue b = expression(parts[0]);
                const AsmValue n = expression(parts[1]);
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

    // .cpu 6502 or .cpu z80: once, before any instruction.
    void cpuDirective(std::string_view args) {
        const std::string name = Upper(args);
        const InstructionSet* cpu = nullptr;
        if (name == "6502") {
            cpu = &Mos6502Instructions();
        } else if (name == "Z80") {
            cpu = &Z80Instructions();
        } else {
            Fail(std::format("unknown CPU '{}'", args));
        }
        if (pass_ == 1) {
            if (instructionSeen_) {
                Fail(".cpu must come before any instruction");
            }
            if (cpuGiven_) {
                Fail(".cpu given twice");
            }
            cpuGiven_ = true;
        }
        cpu_ = cpu;
    }

    void instruction(std::string_view word, std::string_view operandText) {
        const std::string mnemonic = Upper(word);
        if (word.empty() || !cpu_->knows(mnemonic)) {
            const std::string_view shown = word.empty() ? Trim(operandText) : word;
            Fail(std::format("unknown instruction '{}'", shown));
        }
        instructionSeen_ = true;
        cpu_->assemble(*this, mnemonic, operandText);
    }

    // --- AsmContext ---------------------------------------------------------------------------

    int pass() const override { return pass_; }
    std::int64_t pc() const override {
        if (!pc_) {
            Fail("instruction before any .org");
        }
        return *pc_;
    }
    void check(AsmValue v, std::int64_t low, std::int64_t high, std::string_view what) override {
        checkKnownRange(v, low, high, what);
    }
    int& memo() override { return memos_[line_]; }

    void romDirective(std::string_view args) {
        const auto parts = SplitArguments(args);
        if (parts.size() != 2) {
            Fail(".rom needs BASE, SIZE");
        }
        if (rom_ && pass_ == 1) {
            Fail(".rom given twice");
        }
        const AsmValue base = expression(parts[0]);
        const AsmValue size = expression(parts[1]);
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

    // --- expressions ------------------------------------------------------------------------

    AsmValue expression(std::string_view text) override {
        text_in_ = text;
        pos_ = 0;
        skipBlanks();
        if (pos_ >= text_in_.size()) {
            Fail("missing value");
        }
        const AsmValue v = binary(0);
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

    AsmValue binary(int level) {
        if (level == 6) {
            return unary();
        }
        AsmValue left = binary(level + 1);
        for (std::string_view op = binaryOperator(level); !op.empty(); op = binaryOperator(level)) {
            pos_ += op.size();
            const AsmValue right = binary(level + 1);
            left = combine(left, op, right);
        }
        return left;
    }

    static AsmValue combine(AsmValue a, std::string_view op, AsmValue b) {
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

    AsmValue unary() {
        skipBlanks();
        if (pos_ < text_in_.size()) {
            const char c = text_in_[pos_];
            const bool shift = text_in_.substr(pos_).starts_with("<<");
            if (c == '-' || c == '~' || c == '>' || (c == '<' && !shift)) {
                ++pos_;
                const AsmValue v = unary();
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

    AsmValue primary() {
        skipBlanks();
        if (pos_ >= text_in_.size()) {
            Fail("missing value");
        }
        const std::string_view rest = text_in_.substr(pos_);
        const char c = rest.front();
        if (c == '(') {
            ++pos_;
            const AsmValue v = binary(0);
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

    void checkKnownRange(AsmValue v, std::int64_t low, std::int64_t high,
                         std::string_view what) const {
        if (pass_ == 2) {
            checkRange(v.number, low, high, what);
        }
    }

    void emit(std::uint8_t byte) override {
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
    std::deque<std::string> texts_;        // included files' texts, which lines_ views
    std::deque<std::string> names_;        // their file names
    std::vector<int> memos_;               // per line, kept from pass 1 to pass 2 (memo())
    const InstructionSet* cpu_ = nullptr;  // .cpu, the 6502 until it says otherwise
    bool cpuGiven_ = false;                // .cpu seen in pass 1
    bool instructionSeen_ = false;         // an instruction before, this pass
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

}  // namespace asm_detail

std::expected<AssembledRom, std::string> Assemble(std::string_view source, std::string_view file,
                                                  std::optional<std::filesystem::path> includeDir) {
    try {
        return asm_detail::Assembler(source, file, std::move(includeDir)).run();
    } catch (const asm_detail::AsmError& error) {
        return std::unexpected(error.message);
    }
}

std::expected<AssembledRom, std::string> AssembleFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(std::format("cannot read '{}'", path.string()));
    }
    std::ostringstream text;
    text << in.rdbuf();
    const std::filesystem::path dir = path.parent_path();
    return Assemble(text.str(), path.filename().string(), dir.empty() ? "." : dir);
}

}  // namespace bw
