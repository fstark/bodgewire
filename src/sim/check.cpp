#include "sim/check.hpp"

#include <stdexcept>
#include <string>

namespace bw {

void Check(bool condition, std::string_view message, std::source_location where) {
    if (condition) {
        return;
    }
    std::string text = where.file_name();
    text += ':';
    text += std::to_string(where.line());
    text += ": ";
    text += message;
    throw std::logic_error(text);
}

}  // namespace bw
