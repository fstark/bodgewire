#pragma once

#include <ostream>

#include "devices/tv.hpp"

namespace bw {

void WriteGreyscaleBmp(std::ostream& out, const TvFrame& frame);  // kTvColumns × kTvRows, 8 bit

}  // namespace bw
