#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bw {

enum class SpanStyle : std::uint8_t { Text, Code, Bold };

struct Span {
    SpanStyle style;
    std::string text;  // without the ` or ** markers
};

// #, ##, text, -, | a | b | (a header row, a separator row |---|, then rows).
enum class BlockKind : std::uint8_t { Title, Heading, Paragraph, Bullet, Table };

struct Block {
    BlockKind kind;
    std::uint32_t line;                                // first source line, for diagnostics
    std::vector<Span> spans;                           // empty for a Table
    std::vector<std::vector<std::vector<Span>>> rows;  // Table: rows of cells, the header first
};

struct Markdown {
    std::vector<Block> blocks;
};

Markdown ParseMarkdown(std::string_view text);  // never fails: unknown syntax stays text

std::string PlainText(std::span<const Span> spans);
std::string Title(const Markdown& doc);  // first Title block's text, "" if none
// Blocks after "## heading" up to the next Title/Heading; empty if absent.
std::span<const Block> Section(const Markdown& doc, std::string_view heading);

}  // namespace bw
