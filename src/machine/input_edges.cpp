#include "machine/input_edges.hpp"

#include <algorithm>

#include "parts/key_matrix.hpp"
#include "parts/keyboard.hpp"
#include "parts/paddle.hpp"
#include "parts/switch.hpp"

namespace bw {

using namespace literals;

std::expected<std::vector<InputEdge>, std::string> InputEdges(LoadedCircuit& loaded,
                                                              std::span<const ButtonPress> presses,
                                                              std::span<const KnobSetting> knobs,
                                                              std::span<const Typing> typing) {
    std::vector<InputEdge> edges;
    for (const ButtonPress& press : presses) {
        Switch* sw = SwitchOf(loaded, press.reference);
        if (sw == nullptr) {
            return std::unexpected("'" + press.reference + "' is not a switch");
        }
        edges.push_back({press.down, sw->id(), [sw] { sw->setPressed(true); }});
        edges.push_back({press.up, sw->id(), [sw] { sw->setPressed(false); }});
    }
    for (const Typing& typed : typing) {
        if (KeyMatrix* matrix = KeyMatrixOf(loaded, typed.reference)) {
            // The matrix paces its own presses (held, then released): all queued at the start.
            edges.push_back({typed.start, matrix->component().id(),
                             [matrix, text = typed.text] { (void)matrix->type(text); }});
            continue;
        }
        Keyboard* keyboard = KeyboardOf(loaded, typed.reference);
        if (keyboard == nullptr) {
            return std::unexpected("'" + typed.reference + "' is not a keyboard");
        }
        if (typed.untypable) {
            return std::unexpected("cannot type the text on '" + typed.reference + "', a keyboard");
        }
        for (std::size_t i = 0; i < typed.keys.size(); ++i) {
            // 100 ms apart, a key waits only behind a strobe= near 100 ms: the refusal of a
            // full queue is ignored.
            edges.push_back({typed.at(i), keyboard->id(),
                             [keyboard, code = typed.keys[i]] { (void)keyboard->type(code); }});
        }
    }
    for (const KnobSetting& knob : knobs) {
        Paddle* paddle = PaddleOf(loaded, knob.reference);
        if (paddle == nullptr) {
            return std::unexpected("'" + knob.reference + "' is not a paddle controller");
        }
        if (knob.at == 0) {
            paddle->setKnob(knob.turn);
        } else {  // read when a pulse starts: the wake does no harm
            edges.push_back(
                {knob.at, paddle->id(), [paddle, turn = knob.turn] { paddle->setKnob(turn); }});
        }
    }
    std::ranges::stable_sort(edges, {}, &InputEdge::at);
    return edges;
}

bool RunTo(Simulator& sim, std::span<const InputEdge> edges, std::size_t& next, Time end,
           const std::function<bool()>& slice) {
    while (sim.now() < end) {
        Time stop = std::min(end, sim.now() + 1_ms);
        if (next < edges.size()) {
            stop = std::clamp(edges[next].at, sim.now(), stop);
        }
        sim.runUntil(stop);
        for (; next < edges.size() && edges[next].at <= sim.now(); ++next) {
            edges[next].apply();
            sim.wake(edges[next].wake);
        }
        if (!slice()) {
            return false;
        }
    }
    return true;
}

}  // namespace bw
