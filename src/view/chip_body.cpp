#include "view/chip_body.hpp"

#include <algorithm>

namespace bw {

std::optional<BodyBox> PadCentres(const BoardComponent& component) {
    if (component.pads.empty()) {
        return std::nullopt;
    }
    BodyBox box{component.pads[0].position, component.pads[0].position};
    for (const Pad& pad : component.pads) {
        box.min = {std::min(box.min.x, pad.position.x), std::min(box.min.y, pad.position.y)};
        box.max = {std::max(box.max.x, pad.position.x), std::max(box.max.y, pad.position.y)};
    }
    return box;
}

std::optional<ChipBody> BodyOf(const BoardComponent& component) {
    const auto pads = PadCentres(component);
    if (!pads) {
        return std::nullopt;
    }
    const bool horizontal = pads->width() >= pads->height();
    const std::int64_t inset = 650'000;       // body edge from a row's pad centres
    const std::int64_t overhang = 1'270'000;  // body past the end pins
    if (horizontal) {
        return ChipBody{{{pads->min.x - overhang, pads->min.y + inset},
                         {pads->max.x + overhang, pads->max.y - inset}},
                        true};
    }
    return ChipBody{{{pads->min.x + inset, pads->min.y - overhang},
                     {pads->max.x - inset, pads->max.y + overhang}},
                    false};
}

MemoryLook MemoryLookOf(std::string_view part) {
    if (part == "2716") {
        return MemoryLook::Eprom;
    }
    if (part == "2332" || part == "2316B" || part == "2513" || part == "6530") {
        return MemoryLook::Mask;
    }
    return MemoryLook::None;
}

std::optional<MemoryWindow> MemoryWindowOf(const BoardComponent& component, std::string_view part) {
    const auto body = BodyOf(component);
    if (MemoryLookOf(part) == MemoryLook::None || !body) {
        return std::nullopt;
    }
    const std::int64_t across = body->horizontal ? body->box.height() : body->box.width();
    const std::int64_t quartz = across * 3 / 10;
    return MemoryWindow{body->box.centre(), quartz, quartz * 23 / 20, body->horizontal};
}

std::optional<std::size_t> MemoryWindowAt(const BoardScene& scene, Point at) {
    const auto& components = scene.board().components;
    for (std::size_t k = 0; k < components.size(); ++k) {
        const auto window = MemoryWindowOf(components[k], scene.partName(k));
        if (!window) {
            continue;
        }
        const auto dx = static_cast<double>(at.x - window->centre.x);
        const auto dy = static_cast<double>(at.y - window->centre.y);
        const auto seal = static_cast<double>(window->seal);
        if (dx * dx + dy * dy <= seal * seal) {
            return k;
        }
    }
    return std::nullopt;
}

}  // namespace bw
