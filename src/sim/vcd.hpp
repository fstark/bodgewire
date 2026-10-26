#pragma once

#include <ostream>

#include "sim/circuit.hpp"
#include "sim/recorder.hpp"
#include "sim/time.hpp"

namespace bw {

// Timescale 1 ps, one 1-bit wire per recorded Net, values 0/1/x.
void WriteVcd(std::ostream& out, const Circuit& circuit, const Recorder& recorder, Time end);

}  // namespace bw
