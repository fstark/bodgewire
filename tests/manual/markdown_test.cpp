#include "manual/markdown.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("markdown: blocks", "[manual]") {
    const Markdown doc = ParseMarkdown("# Title here\n"
                                       "\n"
                                       "## Section\n"
                                       "First line\n"
                                       "second line.\n"
                                       "\n"
                                       "- one\n"
                                       "- two\n");
    REQUIRE(doc.blocks.size() == 5);
    CHECK(doc.blocks[0].kind == BlockKind::Title);
    CHECK(doc.blocks[0].line == 1);
    CHECK(PlainText(doc.blocks[0].spans) == "Title here");
    CHECK(doc.blocks[1].kind == BlockKind::Heading);
    CHECK(doc.blocks[1].line == 3);
    CHECK(doc.blocks[2].kind == BlockKind::Paragraph);
    CHECK(doc.blocks[2].line == 4);
    CHECK(PlainText(doc.blocks[2].spans) == "First line second line.");
    CHECK(doc.blocks[3].kind == BlockKind::Bullet);
    CHECK(PlainText(doc.blocks[3].spans) == "one");
    CHECK(doc.blocks[4].line == 8);
    CHECK(Title(doc) == "Title here");
}

TEST_CASE("markdown: bullet continuation", "[manual]") {
    const Markdown doc = ParseMarkdown("- first\n  continued\nnot indented\n");
    REQUIRE(doc.blocks.size() == 2);
    CHECK(PlainText(doc.blocks[0].spans) == "first continued");
    CHECK(doc.blocks[1].kind == BlockKind::Paragraph);
    CHECK(doc.blocks[1].line == 3);
}

TEST_CASE("markdown: inline spans", "[manual]") {
    const Markdown doc = ParseMarkdown("check `U1` pin 15 is **high** now\n");
    REQUIRE(doc.blocks.size() == 1);
    const auto& spans = doc.blocks[0].spans;
    REQUIRE(spans.size() == 5);
    CHECK(spans[0].style == SpanStyle::Text);
    CHECK(spans[0].text == "check ");
    CHECK(spans[1].style == SpanStyle::Code);
    CHECK(spans[1].text == "U1");
    CHECK(spans[2].text == " pin 15 is ");
    CHECK(spans[3].style == SpanStyle::Bold);
    CHECK(spans[3].text == "high");
    CHECK(spans[4].text == " now");
}

TEST_CASE("markdown: unmatched markers and unknown syntax", "[manual]") {
    const Markdown doc = ParseMarkdown("a ` b ** c\n\n### deep\n\n1. item\n");
    REQUIRE(doc.blocks.size() == 3);
    REQUIRE(doc.blocks[0].spans.size() == 1);
    CHECK(doc.blocks[0].spans[0].style == SpanStyle::Text);
    CHECK(doc.blocks[0].spans[0].text == "a ` b ** c");
    CHECK(doc.blocks[1].kind == BlockKind::Paragraph);
    CHECK(PlainText(doc.blocks[1].spans) == "### deep");
    CHECK(PlainText(doc.blocks[2].spans) == "1. item");
}

TEST_CASE("markdown: section", "[manual]") {
    const Markdown doc = ParseMarkdown("# T\n## A\npara\n\n- b\n## B\n- c\n# U\n");
    const auto a = Section(doc, "A");
    REQUIRE(a.size() == 2);
    CHECK(PlainText(a[0].spans) == "para");
    CHECK(Section(doc, "B").size() == 1);
    CHECK(Section(doc, "missing").empty());
}
