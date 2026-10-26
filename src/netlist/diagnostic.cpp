#include "netlist/diagnostic.hpp"

namespace bw {

std::string Format(const Diagnostic& d) {
    std::string text = d.file;
    if (d.line != 0) {
        text += ":" + std::to_string(d.line);
    }
    text += d.severity == Severity::Error ? ": error: " : ": warning: ";
    return text + d.message;
}

}  // namespace bw
