#pragma once

#include <expected>
#include <string>

#include "session/session.hpp"
#include "view/bench_options.hpp"

namespace bw {

// A command-line bench's flags applied to its Session before the window opens (the probe, the
// leads, the repairs, the knobs...), each checked against the Machine: the first that does not fit
// ("unknown pin 'U1.99'"), for a usage error.
std::expected<void, std::string> ApplyBenchOptions(Session& session, const BenchOptions& options);

}  // namespace bw
