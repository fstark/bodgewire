#pragma once

#include <cstdint>

namespace bw {

// The shelf's run switch, its three positions (P6o): `‖`, `1×`, `Max`.
enum class RunSwitch : std::uint8_t { Pause, RealTime, Max };

}  // namespace bw
