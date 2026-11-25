#pragma once

#include <imgui.h>

#include "view/meter_reading.hpp"

namespace bw {

inline constexpr ImVec2 kMeterLcdSize{200, 64};  // logical px

// The Multimeter's LCD risen from its Shelf drawing while it measures (P8g): the yellow case's
// top, a grey-green LCD (lit green on `beep`) with kMeterCells seven-segment cells, their unlit
// segments faint, the decimal points in the gaps between them, `V` or, on a beep, the •))) sign.
// Its bottom centre at `base`, moved sideways to stay inside the main viewport.
void DrawMeterLcd(ImDrawList& list, ImVec2 base, const MeterReading& reading);

}  // namespace bw
