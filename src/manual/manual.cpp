#include "manual/manual.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <utility>

#include "parts/mos6530.hpp"
#include "parts/rom.hpp"
#include "sim/circuit.hpp"

namespace bw {

namespace {

constexpr std::string_view kPinWord = " pin ";

bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// [A-Z]+[0-9]+
bool LooksLikeReference(std::string_view text) {
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

const LoadedPart* FindLoadedPart(const LoadedCircuit& design, std::string_view reference) {
    for (const LoadedPart& part : design.parts) {
        if (part.reference == reference) {
            return &part;
        }
    }
    return nullptr;
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

std::string Trim(std::string_view text) {
    const std::size_t start = text.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t");
    return std::string(text.substr(start, end - start + 1));
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

// A Title first, then exactly `required` headings, in order; with `more`, any headings after them.
void CheckShape(const Markdown& doc, Problems& problems, std::span<const std::string_view> required,
                bool more = false) {
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
    for (const std::string_view name : required) {
        expected += (expected.empty() ? "" : ", ") + std::string(name);
    }
    if (more) {
        expected += ", then any";
    }
    bool ok = more ? headings.size() >= required.size() : headings.size() == required.size();
    for (std::size_t i = 0; ok && i < required.size(); ++i) {
        ok = headings[i].first == required[i];
    }
    if (!ok) {
        problems.add(headings.empty() ? 0 : headings.front().second,
                     "sections must be: " + expected);
    }
}

// Every Code span that looks like a reference (and is no Net name) names a Component, and a
// following " pin N" an existing pin.
void CheckReferences(const Markdown& doc, Problems& problems, const LoadedCircuit& design) {
    for (const Block& block : doc.blocks) {
        for (std::size_t i = 0; i < block.spans.size(); ++i) {
            const Span& span = block.spans[i];
            if (span.style != SpanStyle::Code || !LooksLikeReference(span.text) ||
                design.nets.contains(span.text)) {
                continue;
            }
            const LoadedPart* part = FindLoadedPart(design, span.text);
            if (!part) {
                problems.add(block.line, "unknown reference '" + span.text + "'");
                continue;
            }
            if (i + 1 >= block.spans.size() || block.spans[i + 1].style != SpanStyle::Text) {
                continue;
            }
            if (const auto pin = PinAfter(block.spans[i + 1].text)) {
                const std::size_t n = std::stoul(*pin);
                if (n == 0 || n > PinCount(design, *part)) {
                    problems.add(block.line, span.text + " has no pin " + *pin);
                }
            }
        }
    }
}

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

std::vector<Diagnostic> CheckUserManual(const Markdown& doc, std::string_view file,
                                        const LoadedCircuit& design) {
    Problems problems(file);
    constexpr std::string_view kSections[] = {"What it does"};
    CheckShape(doc, problems, kSections);
    CheckReferences(doc, problems, design);
    return problems.take();
}

std::vector<Diagnostic> CheckServiceManual(const Markdown& doc, std::string_view file,
                                           const LoadedCircuit& design) {
    Problems problems(file);
    constexpr std::string_view kSections[] = {"Components"};  // then a Machine's own (Self-test)
    CheckShape(doc, problems, kSections, true);
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
    constexpr std::string_view kSections[] = {"Function", "Pins"};
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

std::vector<std::string> PartPinNames(const PartInfo& part) {
    if (!part.create) {
        return {};
    }
    if (const RomSpec* rom = FindRomSpec(part.name)) {  // a ROM needs a contents file to build
        return {rom->names.begin(), rom->names.end()};
    }
    if (part.name == "6530") {  // so does a 6530
        std::vector<std::string> names;
        for (const PinSpec& pin : Mos6530::Pins()) {
            names.push_back(pin.name);
        }
        return names;
    }
    Circuit circuit;
    std::vector<PartParam> params;
    if (part.name == "OSC") {
        params.push_back({"freq", "1MHz"});
    }
    if (part.name == "POR") {
        params.push_back({"time", "1ms"});
    }
    if (part.name == "PADDLE") {
        params.push_back({"min", "30us"});
        params.push_back({"max", "9.93ms"});
    }
    const auto component = part.create(circuit, "X", params);
    std::vector<std::string> names;
    if (component) {
        for (const PinSpec& spec : (*component)->pins()) {
            names.push_back(spec.name);
        }
    }
    return names;
}

std::optional<Role> ComponentRole(const Markdown& service, std::string_view reference) {
    for (const Block& block : Section(service, "Components")) {
        if (block.kind != BlockKind::Bullet || block.spans.empty() ||
            block.spans.front().style != SpanStyle::Code || block.spans.front().text != reference) {
            continue;
        }
        const std::string rest = PlainText(std::span(block.spans).subspan(1));
        const std::size_t colon = rest.find(':');
        if (colon == std::string::npos) {
            return Role{Trim(rest), ""};
        }
        return Role{Trim(std::string_view(rest).substr(0, colon)),
                    Trim(std::string_view(rest).substr(colon + 1))};
    }
    return std::nullopt;
}

std::vector<std::string> BulletPins(const Block& bullet) {
    std::vector<std::string> names;
    bool range = false;  // the Text since the last Code was a dash
    for (const Span& span : bullet.spans) {
        if (span.style == SpanStyle::Text) {
            if (span.text.find(':') != std::string::npos) {
                break;
            }
            const std::string dash = Trim(span.text);
            range = !names.empty() && (dash == "\u2013" || dash == "-");
            continue;
        }
        if (span.style != SpanStyle::Code) {
            range = false;
            continue;
        }
        if (range) {
            const std::string a = names.back();
            const std::string& b = span.text;
            std::size_t differ = a.size();
            std::size_t count = 0;
            for (std::size_t i = 0; a.size() == b.size() && i < a.size(); ++i) {
                if (a[i] != b[i]) {
                    differ = i;
                    ++count;
                }
            }
            const bool expandable = a.size() == b.size() && count == 1 && a[differ] < b[differ] &&
                                    ((IsUpper(a[differ]) && IsUpper(b[differ])) ||
                                     (IsDigit(a[differ]) && IsDigit(b[differ])));
            if (expandable) {
                for (char c = static_cast<char>(a[differ] + 1); c <= b[differ]; ++c) {
                    std::string name = a;
                    name[differ] = c;
                    names.push_back(std::move(name));
                }
                range = false;
                continue;
            }
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

std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design) {
    if (i >= spans.size() || spans[i].style != SpanStyle::Code ||
        !FindLoadedPart(design, spans[i].text)) {
        return std::nullopt;
    }
    ManualLink link{spans[i].text, std::nullopt, 0};
    if (i + 1 < spans.size() && spans[i + 1].style == SpanStyle::Text) {
        if (auto pin = PinAfter(spans[i + 1].text)) {
            link.pinChars = kPinWord.size() + pin->size();
            link.pin = std::move(pin);
        }
    }
    return link;
}

LoadedManuals LoadManuals(const std::filesystem::path& root, std::string_view machine,
                          const LoadedCircuit& design) {
    LoadedManuals loaded;
    const auto load = [&](const std::filesystem::path& relative) -> std::optional<Markdown> {
        const auto path = root / relative;
        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }
        const auto text = ReadFile(path);
        if (!text) {
            loaded.problems.push_back(
                {relative.generic_string(), 0, Severity::Warning, "cannot read file"});
            return std::nullopt;
        }
        return ParseMarkdown(*text);
    };
    const auto report = [&](std::vector<Diagnostic> problems) {
        loaded.problems.insert(loaded.problems.end(), std::make_move_iterator(problems.begin()),
                               std::make_move_iterator(problems.end()));
    };

    const std::filesystem::path manuals = std::filesystem::path("manuals") / std::string(machine);
    if ((loaded.manuals.user = load(manuals / "user.md"))) {
        report(
            CheckUserManual(*loaded.manuals.user, (manuals / "user.md").generic_string(), design));
    }
    if ((loaded.manuals.service = load(manuals / "service.md"))) {
        report(CheckServiceManual(*loaded.manuals.service,
                                  (manuals / "service.md").generic_string(), design));
    }
    for (const LoadedPart& part : design.parts) {
        const std::string name(part.info->name);
        if (loaded.manuals.datasheets.contains(name)) {
            continue;
        }
        const auto file = std::filesystem::path("datasheets") / (name + ".md");
        if (auto doc = load(file)) {
            report(CheckDatasheet(*doc, file.generic_string(), *part.info));
            loaded.manuals.datasheets.emplace(name, std::move(*doc));
        }
    }
    return loaded;
}

}  // namespace bw
