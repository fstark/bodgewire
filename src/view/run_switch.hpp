#pragma once

#include <cstdint>

namespace bw {

// The shelf's run switch, its four positions (P6o; ¼×, slow motion, 2026-11-25): `‖`, `¼×`, `1×`,
// `Max`.
enum class RunSwitch : std::uint8_t { Pause, Slow, RealTime, Max };

}  // namespace bw
