#include "manual/markdown.hpp"

#include <optional>
#include <utility>

namespace bw {

namespace {

bool IsBlank(std::string_view line) {
    return line.find_first_not_of(" \t") == std::string_view::npos;
}

std::string_view Trim(std::string_view text) {
    const std::size_t start = text.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t");
    return text.substr(start, end - start + 1);
}

void Append(std::vector<Span>& spans, SpanStyle style, std::string_view text) {
    if (text.empty() && style == SpanStyle::Text) {
        return;
    }
    if (style == SpanStyle::Text && !spans.empty() && spans.back().style == SpanStyle::Text) {
        spans.back().text += text;
        return;
    }
    spans.push_back({style, std::string(text)});
}

std::vector<Span> Inline(std::string_view text) {
    std::vector<Span> spans;
    std::size_t plain = 0;  // start of pending Text
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '`') {
            if (const std::size_t close = text.find('`', i + 1); close != std::string_view::npos) {
                Append(spans, SpanStyle::Text, text.substr(plain, i - plain));
                Append(spans, SpanStyle::Code, text.substr(i + 1, close - i - 1));
                i = plain = close + 1;
                continue;
            }
        } else if (text.substr(i).starts_with("**")) {
            if (const std::size_t close = text.find("**", i + 2); close != std::string_view::npos) {
                Append(spans, SpanStyle::Text, text.substr(plain, i - plain));
                Append(spans, SpanStyle::Bold, text.substr(i + 2, close - i - 2));
                i = plain = close + 2;
                continue;
            }
        }
        ++i;
    }
    Append(spans, SpanStyle::Text, text.substr(plain));
    return spans;
}

class Builder {
public:
    void close() {
        if (open_) {
            doc_.blocks.push_back({open_->kind, open_->line, Inline(open_->text)});
            open_.reset();
        }
    }

    void start(BlockKind kind, std::uint32_t line, std::string_view text) {
        close();
        open_ = Open{kind, line, std::string(Trim(text))};
    }

    void append(std::string_view text) {
        open_->text += ' ';
        open_->text += Trim(text);
    }

    std::optional<BlockKind> openKind() const {
        return open_ ? std::optional(open_->kind) : std::nullopt;
    }

    Markdown finish() {
        close();
        return std::move(doc_);
    }

private:
    struct Open {
        BlockKind kind;
        std::uint32_t line;
        std::string text;
    };

    Markdown doc_;
    std::optional<Open> open_;
};

}  // namespace

Markdown ParseMarkdown(std::string_view text) {
    Builder builder;
    std::uint32_t number = 0;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        std::string_view line = text.substr(0, newline);
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
        ++number;
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        if (IsBlank(line)) {
            builder.close();
        } else if (line.starts_with("# ")) {
            builder.start(BlockKind::Title, number, line.substr(2));
            builder.close();
        } else if (line.starts_with("## ")) {
            builder.start(BlockKind::Heading, number, line.substr(3));
            builder.close();
        } else if (line.starts_with("- ")) {
            builder.start(BlockKind::Bullet, number, line.substr(2));
        } else if (builder.openKind() == BlockKind::Bullet && line.starts_with("  ")) {
            builder.append(line);
        } else if (builder.openKind() == BlockKind::Paragraph) {
            builder.append(line);
        } else {
            builder.start(BlockKind::Paragraph, number, line);
        }
    }
    return builder.finish();
}

std::string PlainText(std::span<const Span> spans) {
    std::string text;
    for (const Span& span : spans) {
        text += span.text;
    }
    return text;
}

std::string Title(const Markdown& doc) {
    for (const Block& block : doc.blocks) {
        if (block.kind == BlockKind::Title) {
            return PlainText(block.spans);
        }
    }
    return {};
}

std::span<const Block> Section(const Markdown& doc, std::string_view heading) {
    const std::span<const Block> blocks = doc.blocks;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].kind != BlockKind::Heading || PlainText(blocks[i].spans) != heading) {
            continue;
        }
        std::size_t end = i + 1;
        while (end < blocks.size() && blocks[end].kind != BlockKind::Title &&
               blocks[end].kind != BlockKind::Heading) {
            ++end;
        }
        return blocks.subspan(i + 1, end - i - 1);
    }
    return {};
}

}  // namespace bw
