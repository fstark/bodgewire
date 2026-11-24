#include "ui/tool_click.hpp"

#include <format>
#include <string_view>

#include "instruments/logic_analyzer.hpp"
#include "instruments/oscilloscope.hpp"
#include "view/board_scene.hpp"

namespace bw {

std::optional<RepairPlan> ToolClick(Owner owner, const BoardClick& click, Session& session,
                                    const BoardView& boardView, PencilMarks& marks, WireDraft& wire,
                                    const std::function<void(std::string)>& refuse) {
    const auto& pick = click.pick;
    std::optional<RepairPlan> plan;  // a tool that needs the power off (P6o)
    switch (owner) {
    case Owner::Probe:
        if (!pick) {
            session.probe().touch(std::nullopt);  // bare Board: the tip lifted (P6o)
        } else if (pick->hit.kind != HitKind::Component) {
            session.probe().touch(ProbeTip{pick->contact, pick->layer});
        }
        break;
    case Owner::Puller:
        if (click.component) {
            plan = RepairPlan{
                {SwapRepair{session.drawnBoard()->components[*click.component].reference}},
                click.component,
                ""};
        }
        break;
    case Owner::Iron: {
        // A chip pin is resoldered, else a crack mended (a mend is always Done: Session);
        // on the Board only, so a click beside it never power-cycles.
        if (!InsideOutline(boardView.scene()->outline(), click.at)) {
            break;
        }
        plan = RepairPlan{};
        if (pick && pick->hit.kind == HitKind::Pad) {
            const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
            plan->tries.push_back(
                ResolderRepair{{part.reference, part.pads[pick->hit.index].number}});
        }
        plan->tries.push_back(pick && pick->hit.kind == HitKind::Trace
                                  ? MendRepair{pick->layer, pick->contact}
                                  : MendRepair{click.active, click.at});
        break;
    }
    case Owner::Cutters: {
        constexpr std::string_view kCutRefusal = "Cut: click a trace or a wire";
        if (const auto bodge = boardView.wireAt(click.at)) {
            const WireEnds& w = session.machine()->wires()[*bodge];
            plan = RepairPlan{{UnwireRepair{w.a, w.b}}, std::nullopt, std::string(kCutRefusal)};
        } else if (const auto bridge = boardView.bridgeAt(click.at)) {
            const WireEnds& b = session.machine()->bridges()[*bridge];
            plan = RepairPlan{{UnbridgeRepair{b.a, b.b}}, std::nullopt, std::string(kCutRefusal)};
        } else if (!pick || pick->hit.kind != HitKind::Trace) {
            refuse(std::string(kCutRefusal));
        } else if (boardView.scene()->board().traces[pick->hit.index].mid) {
            refuse("Cut: straight traces only");
        } else {
            plan = RepairPlan{
                {CutRepair{pick->layer, pick->contact}}, std::nullopt, std::string(kCutRefusal)};
        }
        break;
    }
    case Owner::Wire: {
        if (!pick || pick->hit.kind != HitKind::Pad) {
            refuse("Wire: solder to pins only");
            break;
        }
        const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
        const Pad& pad = part.pads[pick->hit.index];
        const PinRef end{part.reference, pad.number};
        if (!wire.start) {  // the first pin, with the power on too
            wire.start = end;
            wire.at = pad.position;
            break;
        }
        if (wire.start->reference != end.reference || wire.start->pin != end.pin) {
            plan = RepairPlan{{WireRepair{*wire.start, end}}, std::nullopt, ""};
        }
        wire.start.reset();  // the same pin again: cancelled
        break;
    }
    case Owner::Analyzer:
    case Owner::Scope: {
        const bool isClip = owner == Owner::Analyzer;
        if (!pick || pick->hit.kind == HitKind::Component) {
            break;
        }
        if (!AcceptsLead(LeadKind::Attached, pick->hit.kind)) {
            refuse(std::format("{}: attaches to pins only", isClip ? "Clip" : "Hook"));
            break;
        }
        const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
        const std::string& pin = part.pads[pick->hit.index].number;
        if (!session.padLead(part.reference, pin)) {  // any pin: a chip's, a resistor's…
            refuse(std::format("{}: attaches to pins only", isClip ? "Clip" : "Hook"));
            break;
        }
        if (isClip) {
            const auto channel = session.analyzer().clipAt(part.reference, pin);
            const auto group = channel ? session.analyzer().groupOf(*channel) : std::nullopt;
            if (channel && click.shift && group) {
                session.analyzer().unclipGroup(*group);
            } else if (channel) {
                session.analyzer().unclip(*channel);
            } else if (click.shift) {
                if (const auto clipped = session.analyzer().clipGroup(part.reference, pin);
                    !clipped) {
                    refuse("Clip: " + clipped.error());
                }
            } else if (!session.analyzer().clip(part.reference, pin)) {
                refuse(std::format("Clip: all {} channels used", kChannels));
            }
        } else {
            if (const auto channel = session.scope().hookAt(part.reference, pin)) {
                session.scope().unhook(*channel);
            } else if (!session.scope().hook(part.reference, pin)) {
                refuse(std::format("Hook: all {} channels used", kScopeChannels));
            }
        }
        break;
    }
    case Owner::Pencil:
        if (!InsideOutline(boardView.scene()->outline(), click.at)) {
            break;  // on the Board only (P6o)
        }
        if (click.shift) {
            marks.clear(click.at, boardView.markReach());
        } else {
            marks.click(click.at, boardView.scene()->bodyAt(click.at), boardView.markReach());
        }
        break;
    case Owner::Meter:
        if (pick && AcceptsLead(LeadKind::Held, pick->hit.kind) &&
            !session.meter().placeLead({pick->contact, pick->layer,
                                        LeadLabel(*boardView.scene(), pick->hit, pick->layer)})) {
            refuse("Meter: power off for continuity");
        }
        break;
    }
    return plan;
}

}  // namespace bw
