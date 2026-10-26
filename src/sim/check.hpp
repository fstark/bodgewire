#pragma once

#include <source_location>
#include <string_view>

namespace bw {

// Throws std::logic_error "<file>:<line>: <message>" when condition is false.
void Check(bool condition, std::string_view message,
           std::source_location where = std::source_location::current());

}  // namespace bw
