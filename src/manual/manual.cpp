#include "manual/manual.hpp"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <utility>

#include "parts/mos6530.hpp"
#include "parts/rom.hpp"
#include "sim/circuit.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

constexpr std::string_view kPinWord = " pin ";

bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }

const LoadedPart* FindLoadedPart(const LoadedCircuit& design, std::string_view reference) {
    for (const LoadedPart& part : design.parts) {
        if (part.reference == reference) {
            return &part;
        }
    }
    return nullptr;
}

// "U12" → {"U", 12}; nullopt unless LooksLikeReference.
std::optional<std::pair<std::string_view, int>> SplitReference(std::string_view text) {
    if (!IsReference(text)) {
        return std::nullopt;
    }
    std::size_t letters = 0;
    while (IsUpper(text[letters])) {
        ++letters;
    }
    return std::pair{text.substr(0, letters), std::stoi(std::string(text.substr(letters)))};
}

constexpr std::string_view kEnDash = "\u2013";

// `A`–`B` at spans[i]: both parts? Then their letters and numbers.
struct RunEnds {
    std::string_view letters;
    int first, last;
};
std::optional<RunEnds> RunAt(std::span<const Span> spans, std::size_t i,
                             const LoadedCircuit& design);

// Pins named with upper case letters, digits and `_` (`ENT`, `CLR_N`, `1A`).
bool LooksLikePinName(std::string_view text) {
    return !text.empty() &&
           std::ranges::all_of(text, [](char c) { return IsUpper(c) || IsDigit(c) || c == '_'; });
}

// `REF.PIN` → {REF, PIN} when REF looks like a reference and PIN like a pin name.
std::optional<std::pair<std::string_view, std::string_view>> SplitPinName(std::string_view text) {
    const std::size_t dot = text.find('.');
    if (dot == std::string_view::npos || !IsReference(text.substr(0, dot)) ||
        !LooksLikePinName(text.substr(dot + 1))) {
        return std::nullopt;
    }
    return std::pair{text.substr(0, dot), text.substr(dot + 1)};
}

// The number (1-based) of the pin named `name` on a chip or connector; resistors have none.
std::optional<std::size_t> PinNumber(const LoadedCircuit& design, std::string_view reference,
                                     std::string_view name) {
    const auto it = design.components.find(reference);
    if (it == design.components.end()) {
        return std::nullopt;
    }
    const auto pins = it->second->pins();
    for (std::size_t p = 0; p < pins.size(); ++p) {
        if (pins[p].name == name) {
            return p + 1;
        }
    }
    return std::nullopt;
}

std::size_t PinCount(const LoadedCircuit& design, const LoadedPart& part) {
    if (const auto it = design.components.find(part.reference); it != design.components.end()) {
        return it->second->pins().size();
    }
    return 2;  // resistor
}

// Digits after " pin " at the start of `text`; nullopt if it does not start so.
std::optional<std::string> PinAfter(std::string_view text) {
    if (!text.starts_with(kPinWord)) {
        return std::nullopt;
    }
    std::size_t end = kPinWord.size();
    while (end < text.size() && IsDigit(text[end])) {
        ++end;
    }
    if (end == kPinWord.size()) {
        return std::nullopt;
    }
    return std::string(text.substr(kPinWord.size(), end - kPinWord.size()));
}

std::optional<RunEnds> RunAt(std::span<const Span> spans, std::size_t i,
                             const LoadedCircuit& design) {
    if (i + 2 >= spans.size() || spans[i].style != SpanStyle::Code ||
        spans[i + 1].style != SpanStyle::Text || spans[i + 1].text != kEnDash ||
        spans[i + 2].style != SpanStyle::Code || !FindLoadedPart(design, spans[i].text) ||
        !FindLoadedPart(design, spans[i + 2].text)) {
        return std::nullopt;
    }
    const auto a = SplitReference(spans[i].text);
    const auto b = SplitReference(spans[i + 2].text);
    if (!a || !b) {
        return std::nullopt;
    }
    if (a->first != b->first) {
        return RunEnds{{}, a->second, b->second};  // reported by the check: letters differ
    }
    return RunEnds{a->first, a->second, b->second};
}

class Problems {
public:
    explicit Problems(std::string_view file) : file_(file) {}

    void add(std::uint32_t line, std::string message) {
        list_.push_back({file_, line, Severity::Warning, std::move(message)});
    }
    std::vector<Diagnostic> take() { return std::move(list_); }

private:
    std::string file_;
    std::vector<Diagnostic> list_;
};

struct SectionRule {
    std::string_view name;
    bool optional = false;
};

// A Title first, then the sections in order: each required one exactly once, each optional one
// at most once, in its place.
void CheckShape(const Markdown& doc, Problems& problems, std::span<const SectionRule> sections) {
    if (doc.blocks.empty() || doc.blocks.front().kind != BlockKind::Title) {
        problems.add(doc.blocks.empty() ? 0 : doc.blocks.front().line, "must start with a # title");
    }
    std::vector<std::pair<std::string, std::uint32_t>> headings;
    for (const Block& block : doc.blocks) {
        if (block.kind == BlockKind::Heading) {
            headings.emplace_back(PlainText(block.spans), block.line);
        }
    }
    std::string expected;
    for (const SectionRule& rule : sections) {
        const std::string comma = expected.empty() ? "" : ", ";
        expected += rule.optional ? "[" + comma + std::string(rule.name) + "]"
                                  : comma + std::string(rule.name);
    }
    std::size_t next = 0;  // the first heading not yet matched
    bool ok = true;
    for (const SectionRule& rule : sections) {
        if (next < headings.size() && headings[next].first == rule.name) {
            ++next;
        } else if (!rule.optional) {
            ok = false;
            break;
        }
    }
    if (!ok || next != headings.size()) {
        problems.add(headings.empty() ? 0 : headings.front().second,
                     "sections must be: " + expected);
    }
}

void CheckSpans(std::span<const Span> spans, std::uint32_t line, Problems& problems,
                const LoadedCircuit& design) {
    for (std::size_t i = 0; i < spans.size(); ++i) {
        const Span& span = spans[i];
        if (span.style != SpanStyle::Code) {
            continue;
        }
        if (const auto run = RunAt(spans, i, design);
            run && (run->letters.empty() || run->first >= run->last)) {
            problems.add(line, "bad run " + span.text + std::string(kEnDash) + spans[i + 2].text);
        }
        if (const auto named = SplitPinName(span.text)) {
            const std::string reference(named->first);
            if (!FindLoadedPart(design, reference)) {
                problems.add(line, "unknown reference '" + reference + "'");
            } else if (!PinNumber(design, reference, named->second)) {
                problems.add(line, reference + " has no pin " + std::string(named->second));
            }
            continue;
        }
        if (!IsReference(span.text) || design.nets.contains(span.text)) {
            continue;
        }
        const LoadedPart* part = FindLoadedPart(design, span.text);
        if (!part) {
            problems.add(line, "unknown reference '" + span.text + "'");
            continue;
        }
        if (i + 1 >= spans.size() || spans[i + 1].style != SpanStyle::Text) {
            continue;
        }
        if (const auto pin = PinAfter(spans[i + 1].text)) {
            const std::size_t n = std::stoul(*pin);
            if (n == 0 || n > PinCount(design, *part)) {
                problems.add(line, span.text + " has no pin " + *pin);
            }
        }
    }
}

// Every Code span that looks like a reference (and is no Net name) names a Component, and a
// following " pin N" an existing pin; in paragraphs, bullets and table cells.
void CheckReferences(const Markdown& doc, Problems& problems, const LoadedCircuit& design) {
    for (const Block& block : doc.blocks) {
        CheckSpans(block.spans, block.line, problems, design);
        for (const auto& row : block.rows) {
            for (const auto& cell : row) {
                CheckSpans(cell, block.line, problems, design);
            }
        }
    }
}

}  // namespace

std::vector<Diagnostic> CheckUserManual(const Markdown& doc, std::string_view file,
                                        const LoadedCircuit& design) {
    Problems problems(file);
    constexpr SectionRule kSections[] = {{"What it does"}, {"Using it", true}, {"How it works"}};
    CheckShape(doc, problems, kSections);
    CheckReferences(doc, problems, design);
    return problems.take();
}

std::vector<Diagnostic> CheckServiceManual(const Markdown& doc, std::string_view file,
                                           const LoadedCircuit& design) {
    Problems problems(file);
    constexpr SectionRule kSections[] = {{"Components"}, {"Test points"}, {"Troubleshooting"}};
    CheckShape(doc, problems, kSections);
    CheckReferences(doc, problems, design);

    std::uint32_t headingLine = 0;
    for (const Block& block : doc.blocks) {
        if (block.kind == BlockKind::Heading && PlainText(block.spans) == "Components") {
            headingLine = block.line;
        }
    }
    std::set<std::string, std::less<>> listed;
    for (const Block& block : Section(doc, "Components")) {
        if (block.kind != BlockKind::Bullet || block.spans.empty() ||
            block.spans.front().style != SpanStyle::Code) {
            problems.add(block.line, "Components: each bullet starts with a `REF`");
            continue;
        }
        const std::string& reference = block.spans.front().text;
        if (!FindLoadedPart(design, reference)) {
            continue;  // reported by CheckReferences
        }
        if (!listed.insert(reference).second) {
            problems.add(block.line, "Components: " + reference + " listed twice");
        }
    }
    for (const LoadedPart& part : design.parts) {
        if (!listed.contains(part.reference)) {
            problems.add(headingLine, "Components: missing " + part.reference);
        }
    }
    return problems.take();
}

std::vector<Diagnostic> CheckDatasheet(const Markdown& doc, std::string_view file,
                                       const PartInfo& part) {
    Problems problems(file);
    constexpr SectionRule kSections[] = {{"Function"}, {"Pins"}};
    CheckShape(doc, problems, kSections);
    const std::string title = Title(doc);
    if (!title.starts_with(part.name) ||
        (title.size() > part.name.size() && title[part.name.size()] != ' ')) {
        problems.add(doc.blocks.empty() ? 0 : doc.blocks.front().line,
                     "title must start with " + std::string(part.name));
    }
    std::set<std::string, std::less<>> covered;
    for (const Block& block : Section(doc, "Pins")) {
        if (block.kind == BlockKind::Bullet) {
            for (std::string& pin : BulletPins(block)) {
                covered.insert(std::move(pin));
            }
        }
    }
    for (const std::string& pin : PartPinNames(part)) {
        if (!covered.contains(pin)) {
            problems.add(0, "Pins: missing " + pin);
        }
    }
    return problems.take();
}

std::vector<IndexEntry> DatasheetIndex(const LoadedCircuit& design) {
    std::vector<IndexEntry> index;
    for (const LoadedPart& part : design.parts) {
        if (part.info == nullptr) {
            continue;
        }
        const auto entry = std::ranges::find(index, part.info->name, &IndexEntry::part);
        if (entry == index.end()) {
            index.push_back({std::string(part.info->name), {part.reference}});
        } else {
            entry->references.push_back(part.reference);
        }
    }
    return index;
}

std::optional<std::vector<Span>> ComponentRoleSpans(const Markdown& service,
                                                    std::string_view reference) {
    for (const Block& block : Section(service, "Components")) {
        if (block.kind != BlockKind::Bullet || block.spans.empty() ||
            block.spans.front().style != SpanStyle::Code || block.spans.front().text != reference) {
            continue;
        }
        std::vector<Span> spans(block.spans.begin() + 1, block.spans.end());
        if (!spans.empty() && spans.front().style == SpanStyle::Text) {
            std::string& text = spans.front().text;
            text.erase(0, text.find_first_not_of(' '));
            if (text.empty()) {
                spans.erase(spans.begin());
            }
        }
        if (!spans.empty() && spans.front().style == SpanStyle::Text) {
            char& first = spans.front().text.front();
            if (first >= 'a' && first <= 'z') {
                first = static_cast<char>(first - 'a' + 'A');
            }
        }
        return spans;
    }
    return std::nullopt;
}

std::vector<std::string> ExpandRange(std::string_view a, std::string_view b) {
    // One character apart: `1A`–`4A`, `A`–`D`.
    std::size_t differ = a.size();
    std::size_t count = 0;
    for (std::size_t i = 0; a.size() == b.size() && i < a.size(); ++i) {
        if (a[i] != b[i]) {
            differ = i;
            ++count;
        }
    }
    if (a.size() == b.size() && count == 1 && a[differ] < b[differ] &&
        ((IsUpper(a[differ]) && IsUpper(b[differ])) ||
         (IsDigit(a[differ]) && IsDigit(b[differ])))) {
        std::vector<std::string> names;
        for (char c = a[differ]; c <= b[differ]; ++c) {
            std::string name(a);
            name[differ] = c;
            names.push_back(std::move(name));
        }
        return names;
    }
    // One prefix before two numbers: `A0`–`A15`.
    const auto split = [](std::string_view s) {
        std::size_t n = s.size();
        while (n > 0 && IsDigit(s[n - 1])) {
            --n;
        }
        return std::pair{s.substr(0, n), s.substr(n)};
    };
    const auto [prefixA, numberA] = split(a);
    const auto [prefixB, numberB] = split(b);
    if (!prefixA.empty() && prefixA == prefixB && !numberA.empty() && !numberB.empty() &&
        numberA.size() <= 4 && numberB.size() <= 4) {
        const int from = std::stoi(std::string(numberA));
        const int to = std::stoi(std::string(numberB));
        if (from < to) {
            std::vector<std::string> names;
            for (int n = from; n <= to; ++n) {
                names.push_back(std::string(prefixA) + std::to_string(n));
            }
            return names;
        }
    }
    return {std::string(a), std::string(b)};
}

std::vector<std::string> BulletPins(const Block& bullet) {
    std::vector<std::string> names;
    bool range = false;  // the Text since the last Code was a dash
    for (const Span& span : bullet.spans) {
        if (span.style == SpanStyle::Text) {
            if (span.text.find(':') != std::string::npos) {
                break;
            }
            const std::string_view dash = Trim(span.text);
            range = !names.empty() && (dash == "\u2013" || dash == "-");
            continue;
        }
        if (span.style != SpanStyle::Code) {
            range = false;
            continue;
        }
        if (range) {
            std::vector<std::string> expanded = ExpandRange(names.back(), span.text);
            names.pop_back();
            names.insert(names.end(), std::make_move_iterator(expanded.begin()),
                         std::make_move_iterator(expanded.end()));
            range = false;
            continue;
        }
        names.push_back(span.text);
        range = false;
    }
    return names;
}

const Block* PinBullet(const Markdown& datasheet, std::string_view pin) {
    for (const Block& block : Section(datasheet, "Pins")) {
        if (block.kind != BlockKind::Bullet) {
            continue;
        }
        const auto pins = BulletPins(block);
        if (std::ranges::find(pins, pin) != pins.end()) {
            return &block;
        }
    }
    return nullptr;
}

namespace {

// The Chip scope at spans[i] (a code span): a range of the chip's pins (`A`–`D`: three spans)
// or one of them.
std::optional<ManualLink> ChipPinLink(std::span<const Span> spans, std::size_t i,
                                      const LoadedCircuit& design, std::string_view chip) {
    const std::string reference(chip);
    if (i + 2 < spans.size() && spans[i + 1].style == SpanStyle::Text &&
        spans[i + 2].style == SpanStyle::Code) {
        const std::string_view dash = Trim(spans[i + 1].text);
        if (dash == "\u2013" || dash == "-") {
            ManualLink link{{reference}, std::nullopt, std::nullopt, 3, 0, {}};
            for (const std::string& name : ExpandRange(spans[i].text, spans[i + 2].text)) {
                const auto number = PinNumber(design, chip, name);
                if (!number) {
                    link.pins.clear();
                    break;
                }
                link.pins.push_back(std::to_string(*number));
            }
            if (link.pins.size() > 1) {
                return link;
            }
        }
    }
    if (const auto number = PinNumber(design, chip, spans[i].text)) {
        ManualLink link{{reference}, std::to_string(*number), std::nullopt, 1, 0, {}};
        link.pins.push_back(*link.pin);
        return link;
    }
    return std::nullopt;
}

}  // namespace

std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design, LinkScope scope,
                                 std::string_view chip) {
    if (i >= spans.size() || spans[i].style != SpanStyle::Code) {
        return std::nullopt;
    }
    const std::string& text = spans[i].text;
    if (scope == LinkScope::Chip) {
        return ChipPinLink(spans, i, design, chip);
    }
    if (const auto run = RunAt(spans, i, design);
        run && !run->letters.empty() && run->first < run->last) {
        ManualLink link;
        for (const LoadedPart& part : design.parts) {
            const auto split = SplitReference(part.reference);
            if (split && split->first == run->letters && split->second >= run->first &&
                split->second <= run->last) {
                link.references.push_back(part.reference);
            }
        }
        link.spansUsed = 3;
        return link;
    }
    if (FindLoadedPart(design, text)) {
        ManualLink link{{text}, std::nullopt, std::nullopt, 1, 0, {}};
        if (i + 1 < spans.size() && spans[i + 1].style == SpanStyle::Text) {
            if (auto pin = PinAfter(spans[i + 1].text)) {
                link.pinChars = kPinWord.size() + pin->size();
                link.pin = std::move(pin);
            }
        }
        return link;
    }
    if (const auto named = SplitPinName(text)) {
        const std::string reference(named->first);
        if (const auto number = PinNumber(design, reference, named->second);
            number && FindLoadedPart(design, reference)) {
            return ManualLink{{reference}, std::to_string(*number), std::nullopt, 1, 0, {}};
        }
        return std::nullopt;
    }
    if (design.nets.contains(text)) {
        return ManualLink{{}, std::nullopt, text, 1, 0, {}};
    }
    return std::nullopt;
}

std::filesystem::path DatasheetDir(const std::filesystem::path& machineDir) {
    return std::filesystem::absolute(machineDir).parent_path().parent_path() / "datasheets";
}

LoadedManuals LoadManuals(const std::filesystem::path& dir, const std::filesystem::path& datasheets,
                          const LoadedCircuit& design) {
    LoadedManuals loaded;
    // `shown`: the file's name in problems ("manuals/user.md").
    const auto load = [&](const std::filesystem::path& path,
                          const std::string& shown) -> std::optional<Markdown> {
        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }
        const auto text = ReadTextFile(path);
        if (!text) {
            loaded.problems.push_back({shown, 0, Severity::Warning, "cannot read file"});
            return std::nullopt;
        }
        return ParseMarkdown(*text);
    };
    const auto report = [&](std::vector<Diagnostic> problems) {
        loaded.problems.insert(loaded.problems.end(), std::make_move_iterator(problems.begin()),
                               std::make_move_iterator(problems.end()));
    };

    if ((loaded.manuals.user = load(dir / "manuals" / "user.md", "manuals/user.md"))) {
        report(CheckUserManual(*loaded.manuals.user, "manuals/user.md", design));
    }
    if ((loaded.manuals.service = load(dir / "manuals" / "service.md", "manuals/service.md"))) {
        report(CheckServiceManual(*loaded.manuals.service, "manuals/service.md", design));
    }
    for (const LoadedPart& part : design.parts) {
        const std::string name(part.info->name);
        if (loaded.manuals.datasheets.contains(name)) {
            continue;
        }
        const std::string shown = "datasheets/" + name + ".md";
        if (auto doc = load(datasheets / (name + ".md"), shown)) {
            report(CheckDatasheet(*doc, shown, *part.info));
            loaded.manuals.datasheets.emplace(name, std::move(*doc));
        }
    }
    return loaded;
}

}  // namespace bw
