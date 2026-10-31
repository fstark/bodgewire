#pragma once

#include <optional>

#include "instruments/logic_analyzer.hpp"
#include "session/session.hpp"

namespace bw {

class BoardView;

// The floating Logic Analyzer: settings, Channel rows, the last Capture's waveforms.
class AnalyzerWindow {
public:
    void draw(bool& open, Session& session, BoardView& board);

private:
    void fit() { from_ = 0, to_ = kDepth; }

    std::optional<Capture> shown_;   // kept across rebuilds and power-off
    double from_ = 0, to_ = kDepth;  // visible points
    std::optional<double> marker_;   // point
    float pressX_ = 0;               // lane press, to tell a click from a drag
    bool moved_ = false;
};

}  // namespace bw
