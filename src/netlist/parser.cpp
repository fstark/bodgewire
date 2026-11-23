#include "netlist/parser.hpp"

#include <cstddef>
#include <utility>

#include "text/text.hpp"

namespace bw {

namespace {

bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool IsLetter(char c) { return IsUpper(c) || (c >= 'a' && c <= 'z'); }
bool IsAlnum(char c) { return IsLetter(c) || IsDigit(c); }

// [A-Z]+[0-9]+
bool IsReference(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size() && IsUpper(text[i])) {
        ++i;
    }
    const std::size_t letters = i;
    while (i < text.size() && IsDigit(text[i])) {
        ++i;
    }
    return letters > 0 && i > letters && i == text.size();
}

// [A-Za-z0-9]+
bool IsPartName(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (!IsAlnum(c)) {
            return false;
        }
    }
    return true;
}

// [A-Za-z_][A-Za-z0-9_]*
bool IsNetName(std::string_view text) {
    if (text.empty() || IsDigit(text[0])) {
        return false;
    }
    for (const char c : text) {
        if (!IsAlnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

class Parser {
public:
    explicit Parser(std::string file) { netlist_.file = std::move(file); }

    void line(std::uint32_t number, std::string_view text) {
        if (const std::size_t hash = text.find('#'); hash != std::string_view::npos) {
            text = text.substr(0, hash);
        }
        const std::vector<std::string_view> tokens = SplitWords(text);
        if (tokens.empty()) {
            return;
        }
        if (tokens[0] == "part") {
            part(number, tokens);
        } else if (tokens[0] == "net") {
            net(number, tokens);
        } else {
            error(number, "unknown statement '" + std::string(tokens[0]) + "'");
        }
    }

    std::expected<Netlist, std::vector<Diagnostic>> finish() {
        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        return std::move(netlist_);
    }

private:
    void error(std::uint32_t line, std::string message) {
        errors_.push_back({netlist_.file, line, Severity::Error, std::move(message)});
    }

    void part(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() < 3) {
            error(line, "part: expected 'part <ref> <part> [key=value ...]'");
            return;
        }
        PartDecl decl{line, std::string(tokens[1]), std::string(tokens[2]), {}};
        const std::size_t errorCount = errors_.size();
        if (!IsReference(tokens[1])) {
            error(line, "bad reference '" + decl.reference + "': letters then digits, e.g. U1");
        }
        if (!IsPartName(tokens[2])) {
            error(line, "bad part name '" + decl.part + "'");
        }
        for (std::size_t i = 3; i < tokens.size(); ++i) {
            const std::string_view token = tokens[i];
            const std::size_t equals = token.find('=');
            if (equals == 0 || equals == std::string_view::npos || equals + 1 == token.size()) {
                error(line, "bad parameter '" + std::string(token) + "': expected key=value");
                continue;
            }
            decl.params.push_back(
                {std::string(token.substr(0, equals)), std::string(token.substr(equals + 1))});
        }
        if (errors_.size() == errorCount) {
            netlist_.parts.push_back(std::move(decl));
        }
    }

    void net(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() < 3) {
            error(line, "net: expected 'net <name> <ref>.<pin> ...'");
            return;
        }
        NetDecl decl{line, std::string(tokens[1]), {}};
        const std::size_t errorCount = errors_.size();
        if (!IsNetName(tokens[1])) {
            error(line, "bad net name '" + decl.name + "'");
        }
        for (std::size_t i = 2; i < tokens.size(); ++i) {
            const std::string_view token = tokens[i];
            const std::size_t dot = token.find('.');
            if (dot == 0 || dot == std::string_view::npos || dot + 1 == token.size() ||
                token.find('.', dot + 1) != std::string_view::npos) {
                error(line, "bad pin reference '" + std::string(token) + "': expected <ref>.<pin>");
                continue;
            }
            decl.pins.push_back(
                {std::string(token.substr(0, dot)), std::string(token.substr(dot + 1))});
        }
        if (errors_.size() == errorCount) {
            netlist_.nets.push_back(std::move(decl));
        }
    }

    Netlist netlist_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

std::expected<Netlist, std::vector<Diagnostic>> ParseNetlist(std::string_view text,
                                                             std::string file) {
    Parser parser(std::move(file));
    std::uint32_t number = 1;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t stop = end == std::string_view::npos ? text.size() : end;
        parser.line(number, text.substr(start, stop - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        ++number;
    }
    return parser.finish();
}

}  // namespace bw
