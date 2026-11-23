#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace bw {

enum class Severity : std::uint8_t { Error, Warning };

struct Diagnostic {
    std::string file;
    std::uint32_t line;  // 1-based; 0 = whole file
    Severity severity;
    std::string message;
};

std::string Format(const Diagnostic& d);  // "file:12: error: message"

void PrintDiagnostics(std::span<const Diagnostic> diagnostics);  // each Format, on stderr

}  // namespace bw
