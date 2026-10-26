#pragma once

#include <cstdint>
#include <utility>

namespace bw {

enum class NetId : std::uint32_t {};
enum class PinId : std::uint32_t {};  // global, across all Components
enum class ComponentId : std::uint32_t {};
using PinIndex = std::uint32_t;  // Pin number local to its Component

constexpr std::uint32_t ToIndex(NetId id) { return std::to_underlying(id); }
constexpr std::uint32_t ToIndex(PinId id) { return std::to_underlying(id); }
constexpr std::uint32_t ToIndex(ComponentId id) { return std::to_underlying(id); }

}  // namespace bw
