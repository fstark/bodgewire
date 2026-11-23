#include "netlist/diagnostic.hpp"

#include <cstdio>
#include <print>

namespace bw {

std::string Format(const Diagnostic& d) {
    std::string text = d.file;
    if (d.line != 0) {
        text += ":" + std::to_string(d.line);
    }
    text += d.severity == Severity::Error ? ": error: " : ": warning: ";
    return text + d.message;
}

void PrintDiagnostics(std::span<const Diagnostic> diagnostics) {
    for (const Diagnostic& d : diagnostics) {
        std::println(stderr, "{}", Format(d));
    }
}

}  // namespace bw
