#pragma once

#include <cstdint>
#include <string>

namespace bw {

// The code printed on a ceramic capacitor (P6g): picofarads under 100 as they are ("47", "10"),
// else two significant digits and a power of ten ("101" 100 pF, "473" 47 nF, "105" 1 µF).
// Whole picofarads, as ParseFarads gives them.
std::string CapacitorCode(std::uint64_t picofarads);

}  // namespace bw
