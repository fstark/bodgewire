#include "ui/scope_window.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>

#include "netlist/units.hpp"

#include <imgui.h>

#include "instruments/volts.hpp"
#include "ui/board_view.hpp"
#include "view/time_text.hpp"
#include "ui/toolbar.hpp"

namespace bw {

namespace {

constexpr float kBandHeight = 70.0f;
constexpr double kFullScale = 5.0;  // V per band
constexpr ImU32 kScreen = IM_COL32(8, 14, 10, 255);
constexpr ImU32 kGrid = IM_COL32(60, 80, 65, 255);
constexpr ImU32 kGridMinor = IM_COL32(35, 50, 40, 255);
constexpr ImU32 kTriggerMark = IM_COL32(255, 140, 0, 220);
constexpr ImU32 kCursorLine = IM_COL32(255, 255, 255, 90);
constexpr ImU32 kMarkerLine = IM_COL32(255, 200, 0, 160);
constexpr ImU32 kScale = IM_COL32(150, 170, 150, 255);
constexpr ImVec4 kGrey{0.5f, 0.5f, 0.5f, 1.0f};

const char* StateText(const Session& session) {
    if (!session.powered()) {
        return "power off";
    }
    if (std::ranges::none_of(session.hooks(), [](const auto& h) { return h.has_value(); })) {
        return "no hooks";
    }
    switch (session.scopeStatus()) {
    case SweepKind::None:
        return "waiting for trigger";
    case SweepKind::Triggered:
        return "triggered";
    case SweepKind::Auto:
        return "auto";
    }
    return "";
}

std::string TimebaseText(Time timebase) { return FormatTime(timebase, false) + "/div"; }

}  // namespace

void ScopeWindow::draw(bool& open, Session& session, BoardView& board) {
    if (session.sweeps().update()) {
        shown_ = session.sweeps().front();
    }
    if (!open) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize({640, 440}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + viewport->WorkSize.x * 2 / 3 - 660, viewport->WorkPos.y + 230},
        ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Oscilloscope", &open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    // Header.
    if (ToolButton("Hook (O)", board.tool() == Tool::Hook)) {
        board.setTool(Tool::Hook);
    }
    ScopeSettings settings = session.scopeSettings();
    bool changed = false;
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(110);
    if (ImGui::BeginCombo("##timebase", TimebaseText(settings.timebase).c_str())) {
        for (const Time timebase : kTimebases) {
            if (ImGui::Selectable(TimebaseText(timebase).c_str(), timebase == settings.timebase)) {
                settings.timebase = timebase;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const auto triggerText = [](std::size_t t) {
        return std::format("ch{} {}", t / 2 + 1, t % 2 == 0 ? "rise" : "fall");
    };
    const std::size_t trigger = settings.source * 2 + (settings.edge == ScopeEdge::Fall ? 1 : 0);
    if (ImGui::BeginCombo("##trigger", triggerText(trigger).c_str())) {
        for (std::size_t t = 0; t < 2 * kScopeChannels; ++t) {
            if (ImGui::Selectable(triggerText(t).c_str(), t == trigger)) {
                settings.source = t / 2;
                settings.edge = t % 2 == 0 ? ScopeEdge::Rise : ScopeEdge::Fall;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("auto", settings.mode == ScopeMode::Auto)) {
        settings.mode = ScopeMode::Auto;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("normal", settings.mode == ScopeMode::Normal)) {
        settings.mode = ScopeMode::Normal;
        changed = true;
    }
    ImGui::SameLine(0, 16);
    ImGui::TextUnformatted(StateText(session));

    // Delay: typed as a time, or stepped by a division.
    ImGui::TextUnformatted("delay");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (!delayEditing_) {
        const std::string text = settings.delay == 0 ? "0" : FormatTime(settings.delay, false);
        std::ranges::fill(delayText_, '\0');
        std::ranges::copy(text.substr(0, delayText_.size() - 1), delayText_.begin());
    }
    if (delayRefused_) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{0.55f, 0.1f, 0.1f, 1.0f});
    }
    ImGui::InputText("##delay", delayText_.data(), delayText_.size());
    if (delayRefused_) {
        ImGui::PopStyleColor();
    }
    ImGui::SetItemTooltip("time from the Trigger to its mark: 12.5us, 2ms, 0");
    delayEditing_ = ImGui::IsItemActive();
    if (ImGui::IsItemEdited()) {
        delayRefused_ = false;
    }
    const Time maxDelay = kMaxDelayDivisions * settings.timebase;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::string text(delayText_.data());
        std::erase(text, ' ');
        const auto parsed = text == "0" ? std::expected<Time, std::string>(0) : ParseDuration(text);
        delayRefused_ = !parsed || *parsed > maxDelay;
        if (!delayRefused_) {
            settings.delay = *parsed;
            changed = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("-") && settings.delay > 0) {
        settings.delay = std::max<Time>(0, settings.delay - settings.timebase);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+") && settings.delay < maxDelay) {
        settings.delay = std::min(maxDelay, settings.delay + settings.timebase);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("0")) {
        settings.delay = 0;
        changed = true;
    }

    // Screen.
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const float width = std::max(10.0f, ImGui::GetContentRegionAvail().x);
    const float height = kBandHeight * kScopeChannels;
    ImGui::InvisibleButton("##screen", {width, height});
    const bool hovered = ImGui::IsItemHovered();
    const float mouseX = ImGui::GetIO().MousePos.x;
    const Time timebase = shown_ ? shown_->timebase : settings.timebase;
    const double span = static_cast<double>(kDivisions * timebase);
    const Time delay = shown_ ? shown_->delay : settings.delay;
    const auto timeAt = [&](float x) {  // ps since the Trigger
        return static_cast<Time>(std::llround((x - top.x) / width * span)) + delay - timebase;
    };
    if (ImGui::IsItemClicked()) {
        marker_ = timeAt(mouseX);
    }

    list.AddRectFilled(top, {top.x + width, top.y + height}, kScreen);
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        const float bandTop = top.y + kBandHeight * static_cast<float>(c);
        for (int v = 1; v < 5; ++v) {
            const float y = bandTop + kBandHeight * static_cast<float>(v) / 5;
            list.AddLine({top.x, y}, {top.x + width, y}, kGridMinor);
        }
        list.AddLine({top.x, bandTop}, {top.x + width, bandTop}, kGrid);
    }
    for (Time d = 0; d <= kDivisions; ++d) {
        const float x = top.x + width * static_cast<float>(d) / static_cast<float>(kDivisions);
        list.AddLine({x, top.y}, {x, top.y + height}, kGrid);
    }
    const float triggerX = top.x + width / static_cast<float>(kDivisions) -
                           static_cast<float>(static_cast<double>(delay) / span * width);
    if (triggerX >= top.x) {
        list.AddLine({triggerX, top.y}, {triggerX, top.y + height}, kTriggerMark, 1.5f);
        list.AddTriangleFilled({triggerX - 5, top.y}, {triggerX + 5, top.y}, {triggerX, top.y + 7},
                               kTriggerMark);
    } else {
        // Off screen to the left: an arrow and how far back it is.
        const float y = top.y + height - 8;
        list.AddTriangleFilled({top.x + 2, y}, {top.x + 10, y - 5}, {top.x + 10, y + 5},
                               kTriggerMark);
        const std::string back = "T " + FormatTime(-(delay - timebase), true);
        list.AddText({top.x + 14, y - 8}, kTriggerMark, back.c_str());
    }
    const bool anyHook =
        std::ranges::any_of(session.hooks(), [](const auto& h) { return h.has_value(); });
    if (!anyHook) {
        list.AddText({top.x + 8, top.y + kBandHeight / 2}, ImGui::GetColorU32(kGrey),
                     "Hook (O), then click a chip pin.");
    }

    const auto bandY = [&](std::size_t c, double volts) {
        const float bandBottom = top.y + kBandHeight * static_cast<float>(c + 1) - 4;
        return bandBottom - static_cast<float>(volts / kFullScale) * (kBandHeight - 8);
    };
    std::array<std::optional<double>, kScopeChannels> rails;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        rails[c] = session.hooks()[c] ? session.hookRail(c) : std::nullopt;
    }
    if (shown_ &&
        (shown_->serial != columnsSerial_ || width != columnsWidth_ || rails != columnsRail_)) {
        columnsSerial_ = shown_->serial;
        columnsWidth_ = width;
        columnsRail_ = rails;
        const auto columns = static_cast<std::size_t>(width);
        const double end = static_cast<double>(shown_->start) + span;
        const auto x = [&](double t) {
            return std::clamp((t - static_cast<double>(shown_->start)) / span * width, 0.0,
                              static_cast<double>(columns - 1));
        };
        for (std::size_t c = 0; c < kScopeChannels; ++c) {
            auto& out = columns_[c];
            out.assign(columns,
                       {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()});
            const auto& samples = shown_->channels[c];
            for (std::size_t i = 0; i < samples.size(); ++i) {
                const double t0 = static_cast<double>(samples[i].time);
                const double t1 =
                    i + 1 < samples.size() ? static_cast<double>(samples[i + 1].time) : end;
                const auto volts =
                    static_cast<float>(rails[c] ? *rails[c] : LogicVolts(samples[i]));
                const auto px1 = static_cast<std::size_t>(x(t1));
                for (auto px = static_cast<std::size_t>(x(t0)); px <= px1; ++px) {
                    out[px].first = std::min(out[px].first, volts);
                    out[px].second = std::max(out[px].second, volts);
                }
            }
        }
    }

    list.PushClipRect(top, {top.x + width, top.y + height}, true);
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        const auto& hook = session.hooks()[c];
        if (!hook) {
            continue;
        }
        const ImU32 colour = kHookColours[c];
        const float bandTop = top.y + kBandHeight * static_cast<float>(c);
        const std::string label = std::to_string(c + 1) + " " + hook->label;
        list.AddText({top.x + 4, bandTop + 2}, colour, label.c_str());
        if (!session.powered()) {
            const float y = bandY(c, 0.0);
            list.AddLine({top.x, y}, {top.x + width, y}, colour, 1.5f);
            continue;
        }
        if (!shown_ || shown_->channels[c].empty()) {
            continue;
        }
        const auto& columns = columns_[c];
        for (std::size_t px = 0; px < columns.size();) {
            const auto [lo, hi] = columns[px];
            std::size_t next = px + 1;
            while (next < columns.size() && columns[next] == columns[px]) {
                ++next;  // one rect per run of equal columns
            }
            if (lo <= hi) {
                const float x = top.x + static_cast<float>(px);
                list.AddRectFilled({x, bandY(c, hi) - 0.75f},
                                   {top.x + static_cast<float>(next), bandY(c, lo) + 0.75f},
                                   colour);
            }
            px = next;
        }
    }
    list.AddText({top.x + width - 30, top.y + 2}, kScale, "5 V");
    if (marker_) {
        const float x =
            top.x +
            static_cast<float>(static_cast<double>(*marker_ + timebase - delay) / span * width);
        list.AddLine({x, top.y}, {x, top.y + height}, kMarkerLine, 1.0f);
    }
    if (hovered) {
        list.AddLine({mouseX, top.y}, {mouseX, top.y + height}, kCursorLine, 1.0f);
    }
    list.PopClipRect();

    // Hook rows.
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        ImGui::PushID(static_cast<int>(c));
        const auto& hook = session.hooks()[c];
        ImGui::ColorButton("##colour", ImColor(kHookColours[c]),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {12, 12});
        ImGui::SameLine();
        if (hook) {
            ImGui::Text("%zu  %s", c + 1, hook->label.c_str());
        } else {
            ImGui::TextColored(kGrey, "%zu  free", c + 1);
        }
        ImGui::SameLine(220);
        ImGui::BeginDisabled(!hook);
        if (ImGui::SmallButton("\u00d7")) {
            session.unhook(c);
        }
        ImGui::EndDisabled();
        if (c == 0) {
            ImGui::SameLine(0, 40);
            if (hovered) {
                std::string readout = "t = " + FormatTime(timeAt(mouseX), true);
                if (marker_) {
                    readout += "   dt = " + FormatTime(timeAt(mouseX) - *marker_, true);
                }
                ImGui::TextUnformatted(readout.c_str());
            } else {
                ImGui::TextColored(kGrey, "hover: time   click: marker");
            }
        }
        ImGui::PopID();
    }
    if (changed) {
        session.setScope(settings);
    }
    ImGui::End();
}

}  // namespace bw
