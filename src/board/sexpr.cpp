#include "board/sexpr.hpp"

#include <utility>

namespace bw {

std::string_view SExpr::head() const {
    if (!isList || list.empty() || list[0].isList) {
        return {};
    }
    return list[0].atom;
}

const SExpr* SExpr::find(std::string_view name) const {
    for (const SExpr& child : list) {
        if (child.head() == name) {
            return &child;
        }
    }
    return nullptr;
}

std::string_view SExpr::atomAt(std::size_t index) const {
    if (index >= list.size() || list[index].isList) {
        return {};
    }
    return list[index].atom;
}

namespace {

class Reader {
public:
    Reader(std::string_view text, std::string_view file) : text_(text), file_(file) {}

    std::expected<SExpr, Diagnostic> read() {
        skipSpace();
        if (atEnd() || text_[pos_] != '(') {
            return fail("expected one top-level list");
        }
        auto root = readList();
        if (!root) {
            return root;
        }
        skipSpace();
        if (!atEnd()) {
            return fail("expected one top-level list");
        }
        return root;
    }

private:
    bool atEnd() const { return pos_ >= text_.size(); }

    std::unexpected<Diagnostic> fail(std::string message) const {
        return std::unexpected(
            Diagnostic{std::string(file_), line_, Severity::Error, std::move(message)});
    }

    void skipSpace() {
        while (!atEnd()) {
            const char c = text_[pos_];
            if (c == '\n') {
                ++line_;
            } else if (c != ' ' && c != '\t' && c != '\r') {
                return;
            }
            ++pos_;
        }
    }

    // At '('.
    std::expected<SExpr, Diagnostic> readList() {
        SExpr node{.line = line_, .isList = true};
        ++pos_;
        while (true) {
            skipSpace();
            if (atEnd()) {
                return fail("unbalanced parentheses");
            }
            const char c = text_[pos_];
            if (c == ')') {
                ++pos_;
                return node;
            }
            if (c == '(') {
                auto child = readList();
                if (!child) {
                    return child;
                }
                node.list.push_back(std::move(*child));
                continue;
            }
            auto atom = c == '"' ? readString() : readAtom();
            if (!atom) {
                return atom;
            }
            node.list.push_back(std::move(*atom));
        }
    }

    std::expected<SExpr, Diagnostic> readString() {
        SExpr node{.line = line_};
        ++pos_;
        while (true) {
            if (atEnd()) {
                return fail("unterminated string");
            }
            char c = text_[pos_++];
            if (c == '"') {
                return node;
            }
            if (c == '\n') {
                ++line_;
            } else if (c == '\\' && !atEnd()) {
                c = text_[pos_++];
                if (c == 'n') {
                    c = '\n';
                }
            }
            node.atom += c;
        }
    }

    std::expected<SExpr, Diagnostic> readAtom() {
        SExpr node{.line = line_};
        const std::size_t start = pos_;
        while (!atEnd()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '(' || c == ')' ||
                c == '"') {
                break;
            }
            ++pos_;
        }
        node.atom = text_.substr(start, pos_ - start);
        return node;
    }

    std::string_view text_;
    std::string_view file_;
    std::size_t pos_ = 0;
    std::uint32_t line_ = 1;
};

}  // namespace

std::expected<SExpr, Diagnostic> ParseSExpr(std::string_view text, std::string_view file) {
    return Reader(text, file).read();
}

}  // namespace bw
