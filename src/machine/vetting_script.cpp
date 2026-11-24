#include "machine/vetting_script.hpp"

#include <cstddef>
#include <optional>
#include <utility>

#include "machine/puzzle.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

class Parser {
public:
    explicit Parser(std::string file) { script_.file = std::move(file); }

    void line(std::uint32_t number, std::string_view text) {
        // A task's and a typed text may hold a #: no comment is cut off them.
        if (const std::vector<std::string_view> raw = SplitWords(text);
            !raw.empty() && (raw[0] == "task" || raw[0] == "type")) {
            const std::size_t start = static_cast<std::size_t>(raw[0].data() - text.data());
            const std::string_view rest = text.substr(start + raw[0].size());
            if (raw[0] == "task") {
                task(number, rest);
            } else {
                type(number, rest);
            }
            return;
        }
        if (const std::size_t hash = text.find('#'); hash != std::string_view::npos) {
            text = text.substr(0, hash);
        }
        const std::vector<std::string_view> tokens = SplitWords(text);
        if (tokens.empty()) {
            return;
        }
        if (tokens[0] == "machine") {
            machine(number, tokens);
        } else if (tokens[0] == "press") {
            if (tokens.size() != 2) {
                error(number, "expected: press REF:DOWN:UP");
            } else if (auto press = ParsePress(tokens[1])) {
                script_.presses.push_back(std::move(*press));
            } else {
                error(number, press.error());
            }
        } else if (tokens[0] == "knob") {
            if (tokens.size() != 2) {
                error(number, "expected: knob REF:N%[:TIME]");
            } else if (auto knob = ParseKnob(tokens[1])) {
                script_.knobs.push_back(std::move(*knob));
            } else {
                error(number, knob.error());
            }
        } else if (tokens[0] == "capture") {
            capture(number, tokens);
        } else {
            error(number, "unknown directive '" + std::string(tokens[0]) + "'");
        }
    }

    std::expected<VettingScript, std::vector<Diagnostic>> finish() {
        if (script_.captures.empty() && !badCapture_) {
            errors_.insert(errors_.begin(), {script_.file, 0, Severity::Error, "no capture line"});
        }
        if (script_.machineLine == 0) {
            errors_.insert(errors_.begin(), {script_.file, 0, Severity::Error, "no machine line"});
        }
        if (!errors_.empty()) {
            return std::unexpected(std::move(errors_));
        }
        return std::move(script_);
    }

private:
    void error(std::uint32_t line, std::string message) {
        errors_.push_back({script_.file, line, Severity::Error, std::move(message)});
    }

    void machine(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() != 2) {
            error(line, "expected: machine NAME");
            return;
        }
        if (!IsMachineName(tokens[1])) {
            error(line, "bad machine name '" + std::string(tokens[1]) + "'");
            return;
        }
        if (script_.machineLine != 0) {
            error(line, "machine already given at line " + std::to_string(script_.machineLine));
            return;
        }
        script_.machineLine = line;
        script_.machine = tokens[1];
    }

    void task(std::uint32_t line, std::string_view rest) {
        const auto value = QuotedValue(rest);
        if (!value) {
            error(line, "expected: task \"TEXT\"");
            return;
        }
        if (script_.taskLine != 0) {
            error(line, "task already given at line " + std::to_string(script_.taskLine));
            return;
        }
        script_.taskLine = line;
        script_.task = *value;
    }

    // The rest of the line: the text keeps its spaces.
    void type(std::uint32_t line, std::string_view rest) {
        const std::string_view text = Trim(rest);
        if (text.empty()) {
            error(line, "expected: type REF:START:TEXT");
        } else if (auto typing = ParseTyping(text)) {
            script_.typing.push_back(std::move(*typing));
        } else {
            error(line, typing.error());
        }
    }

    void capture(std::uint32_t line, const std::vector<std::string_view>& tokens) {
        if (tokens.size() != 2) {
            badCapture_ = true;
            error(line, "expected: capture TIME");
            return;
        }
        const auto time = ParseDuration(tokens[1]);
        if (!time) {
            badCapture_ = true;
            error(line, time.error());
            return;
        }
        if (!script_.captures.empty() && *time <= script_.captures.back()) {
            error(line, "captures must come in time order");
            return;
        }
        script_.captures.push_back(*time);
    }

    VettingScript script_;
    std::vector<Diagnostic> errors_;
    bool badCapture_ = false;  // a capture line in error: not also "no capture line"
};

}  // namespace

std::expected<VettingScript, std::vector<Diagnostic>> ParseVettingScript(std::string_view text,
                                                                         std::string file) {
    Parser parser(std::move(file));
    std::uint32_t number = 1;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        parser.line(number++, text.substr(0, newline));
        if (newline == std::string_view::npos) {
            break;
        }
        text.remove_prefix(newline + 1);
    }
    return parser.finish();
}

std::expected<VettingScript, std::vector<Diagnostic>>
ReadVettingScriptFile(const std::filesystem::path& path) {
    std::string file = path.filename().string();
    const auto text = ReadTextFile(path);
    if (!text) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    return ParseVettingScript(*text, std::move(file));
}

}  // namespace bw
