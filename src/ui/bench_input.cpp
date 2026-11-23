#include "ui/bench_input.hpp"

#include <algorithm>

#include "parts/switch.hpp"
#include "ui/board_tags.hpp"

namespace bw {

void SwitchHands::update(Session& session, BoardView& view, std::span<const ButtonPress> presses,
                         const BoardKeyCapture& keys, Time simTime) {
    const BoardScene* scene = view.scene();
    if (!session.powered()) {
        latched_.clear();
    }
    const auto toggled = view.takeLatchToggle();
    for (const std::string& reference : session.inputs().switches()) {
        const auto component =
            scene ? scene->componentIndex(reference) : std::optional<std::size_t>();
        const Switch* sw = SwitchOf(session.circuit(), reference);
        if (sw != nullptr && sw->kind() == SwitchKind::Slide) {
            if (component && toggled == component && !slid_.erase(reference)) {
                slid_.insert(reference);
            }
        } else if (component && toggled == component && session.powered() &&
                   !latched_.erase(reference)) {
            latched_.insert(reference);
        }
        const bool scripted = std::ranges::any_of(presses, [&](const ButtonPress& p) {
            return p.reference == reference && p.down <= simTime && simTime < p.up;
        });
        session.inputs().setPressed(
            reference, latched_.contains(reference) || slid_.contains(reference) || scripted ||
                           (component && view.held() == component) || keys.holds(reference));
    }
    if (scene) {
        view.setSwitches(SwitchTagsOf(*scene, session));
    }
}

void ScriptedTyping::update(Session& session, Time simTime, std::string_view matrix,
                            std::string_view keyboard, KeyboardWindow& window) {
    // Not in the first frame powered: its simTime is the previous run's (the power-on is only
    // queued).
    if (!session.powered()) {
        std::ranges::fill(sent_, 0);
    } else if (wasPowered_) {
        for (std::size_t t = 0; t < typing_.size(); ++t) {
            const Typing& typing = typing_[t];
            if (typing.reference == matrix) {
                if (sent_[t] == 0 && typing.start <= simTime) {
                    session.inputs().typeText(typing.reference, typing.text);
                    sent_[t] = 1;
                }
                continue;
            }
            while (sent_[t] < typing.keys.size() && typing.at(sent_[t]) <= simTime) {
                const std::uint8_t code = typing.keys[sent_[t]++];
                if (session.inputs().typeCode(typing.reference, code) &&
                    typing.reference == keyboard) {
                    window.flash(code);
                }
            }
        }
    }
    wasPowered_ = session.powered();
}

}  // namespace bw
