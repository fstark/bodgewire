#include "ui/analyzer_window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <string>

#include <imgui.h>

#include "ui/board_view.hpp"

namespace bw {

namespace {

constexpr float kLaneHeight = 20.0f;
constexpr float kAxisHeight = 18.0f;
constexpr double kMinPoints = 16;
constexpr float kZoomStep = 0.8f;  // span factor per wheel notch
constexpr ImU32 kUnknown = IM_COL32(140, 140, 140, 110);
constexpr ImU32 kTriggerLine = IM_COL32(255, 255, 255, 140);
constexpr ImU32 kCursorLine = IM_COL32(255, 255, 255, 90);
constexpr ImU32 kMarkerLine = IM_COL32(255, 200, 0, 160);
constexpr ImU32 kAxis = IM_COL32(180, 180, 180, 255);
constexpr ImU32 kFaint = IM_COL32(200, 200, 200, 90);
constexpr ImVec4 kGrey{0.5f, 0.5f, 0.5f, 1.0f};
constexpr std::array<const char*, 5> kConditions{"any", "0", "1", "rise", "fall"};
constexpr std::array<const char*, 3> kPositions{"10 %", "50 %", "90 %"};

// Three significant digits, trailing zeros dropped; `sign`: "+" before positive values.
std::string FormatTime(Time ps, bool sign) {
    if (ps == 0) {
        return "0";
    }
    const double magnitude = std::abs(static_cast<double>(ps));
    static constexpr std::array<std::pair<double, const char*>, 5> kUnits{
        {{1e12, "s"}, {1e9, "ms"}, {1e6, "\u00b5s"}, {1e3, "ns"}, {1, "ps"}}};
    auto unit = kUnits.back();
    for (const auto& u : kUnits) {
        if (magnitude >= u.first) {
            unit = u;
            break;
        }
    }
    const double value = magnitude / unit.first;
    std::string digits = std::format("{:.{}f}", value, value >= 100 ? 0 : value >= 10 ? 1 : 2);
    if (digits.find('.') != std::string::npos) {
        digits.erase(digits.find_last_not_of('0') + 1);
        if (digits.back() == '.') {
            digits.pop_back();
        }
    }
    return std::format("{}{} {}", ps < 0 ? "-" : (sign ? "+" : ""), digits, unit.second);
}

std::string FormatFrequency(Time period) {
    const Time hz = kPicosPerSecond / period;
    return hz >= 1'000'000 ? std::format("{} MHz", hz / 1'000'000)
                           : std::format("{} kHz", hz / 1'000);
}

const char* PhaseText(AnalyzerPhase phase, bool powered) {
    switch (phase) {
    case AnalyzerPhase::Idle:
        return "idle";
    case AnalyzerPhase::Armed:
        return powered ? "armed: waiting for trigger" : "armed: waiting for power";
    case AnalyzerPhase::Filling:
        return "triggered: filling";
    case AnalyzerPhase::Captured:
        return "captured";
    }
    return "";
}

// 0 low, 1 high, 2 X for Channel `c` at `point`.
int Level(const Capture& capture, std::size_t point, std::size_t c) {
    const ChannelBits bits = capture.points[point];
    if ((bits.unknown >> c & 1) != 0) {
        return 2;
    }
    return bits.high >> c & 1;
}

// Time step in 1-2-5 steps giving at most `ticks` ticks over `span` ps.
Time TickStep(double span, int ticks) {
    Time step = 1;
    while (true) {
        for (const Time m : {1, 2, 5}) {
            if (span / static_cast<double>(step * m) <= ticks) {
                return step * m;
            }
        }
        step *= 10;
    }
}

}  // namespace

void AnalyzerWindow::draw(bool& open, Session& session) {
    if (session.captures().update()) {
        shown_ = session.captures().front();
        marker_.reset();
        fit();
    }
    if (!open) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize({760, 330}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + 20, viewport->WorkPos.y + viewport->WorkSize.y - 350},
        ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Logic Analyzer", &open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    // Header.
    const AnalyzerPhase phase = session.analyzerPhase();
    const bool running = phase == AnalyzerPhase::Armed || phase == AnalyzerPhase::Filling;
    if (ImGui::Button(running ? "Stop" : "Arm", {60, 0})) {
        if (running) {
            session.disarm();
        } else {
            session.arm();
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(PhaseText(phase, session.powered()));
    AnalyzerSettings settings = session.analyzerSettings();
    bool changed = false;
    ImGui::SameLine(300);
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("Rate", FormatFrequency(settings.period).c_str())) {
        for (const Time period : kAnalyzerPeriods) {
            if (ImGui::Selectable(FormatFrequency(period).c_str(), period == settings.period)) {
                settings.period = period;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 24);
    ImGui::SetNextItemWidth(70);
    if (ImGui::BeginCombo("Trigger at", kPositions[static_cast<std::size_t>(settings.position)])) {
        for (std::size_t p = 0; p < kPositions.size(); ++p) {
            if (ImGui::Selectable(kPositions[p],
                                  p == static_cast<std::size_t>(settings.position))) {
                settings.position = static_cast<TriggerPosition>(p);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    // Channel rows.
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const double span = to_ - from_;
    float laneX0 = 0, laneX1 = 0, lanesTop = 0, lanesBottom = 0;
    std::optional<float> hoverX;
    if (ImGui::BeginTable("channels", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("trigger", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("unclip", ImGuiTableColumnFlags_WidthFixed, 20);
        ImGui::TableSetupColumn("lane", ImGuiTableColumnFlags_WidthStretch);
        for (std::size_t c = 0; c < kChannels; ++c) {
            ImGui::PushID(static_cast<int>(c));
            const auto& clip = session.clips()[c];
            ImGui::TableNextRow(ImGuiTableRowFlags_None, kLaneHeight);
            ImGui::TableNextColumn();
            ImGui::ColorButton("##colour", ImColor(kChannelColours[c]),
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                               {12, 12});
            ImGui::SameLine();
            if (clip) {
                ImGui::TextUnformatted(clip->label.c_str());
            } else {
                ImGui::TextColored(kGrey, "free");
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            const auto condition = static_cast<std::size_t>(settings.trigger[c]);
            if (ImGui::BeginCombo("##trigger", kConditions[condition])) {
                for (std::size_t t = 0; t < kConditions.size(); ++t) {
                    if (ImGui::Selectable(kConditions[t], t == condition)) {
                        settings.trigger[c] = static_cast<TriggerCondition>(t);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!clip);
            if (ImGui::SmallButton("\u00d7")) {
                session.unclip(c);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            const ImVec2 top = ImGui::GetCursorScreenPos();
            const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            ImGui::InvisibleButton("##lane", {width, kLaneHeight});
            laneX0 = top.x;
            laneX1 = top.x + width;
            if (c == 0) {
                lanesTop = top.y;
            }
            lanesBottom = top.y + kLaneHeight;

            // Lane input: wheel zoom, drag pan, click marker, double-click fit.
            const double perPixel = span / static_cast<double>(width);
            const float mouseX = ImGui::GetIO().MousePos.x;
            if (ImGui::IsItemHovered()) {
                hoverX = mouseX;
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0 && shown_) {
                    const double at = from_ + (mouseX - top.x) * perPixel;
                    const double next =
                        std::clamp(span * std::pow(kZoomStep, wheel), kMinPoints, double{kDepth});
                    from_ = at - (mouseX - top.x) / width * next;
                    to_ = from_ + next;
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    fit();
                }
            }
            if (ImGui::IsItemActivated()) {
                pressX_ = mouseX;
                moved_ = false;
            }
            if (ImGui::IsItemActive()) {
                moved_ = moved_ || std::abs(mouseX - pressX_) > 3;
                const double delta = ImGui::GetIO().MouseDelta.x * perPixel;
                from_ -= delta;
                to_ -= delta;
            }
            if (ImGui::IsItemDeactivated() && !moved_ && shown_) {
                marker_ = from_ + (mouseX - top.x) * perPixel;
            }
            const double shift = std::clamp(from_, 0.0, double{kDepth} - (to_ - from_)) - from_;
            from_ += shift;
            to_ += shift;

            // Waveform.
            if (shown_ && !shown_->labels[c].empty()) {
                const auto x = [&](double point) {
                    return static_cast<float>(top.x + (point - from_) / (to_ - from_) * width);
                };
                const float high = top.y + 3, low = top.y + kLaneHeight - 3;
                const ImU32 colour = kChannelColours[c];
                const double points = to_ - from_;
                if (points <= width) {
                    const auto first = static_cast<std::size_t>(
                        std::max<double>(static_cast<double>(shown_->first), std::floor(from_)));
                    const auto last =
                        std::min<std::size_t>(kDepth, static_cast<std::size_t>(std::ceil(to_)));
                    int previous = -1;
                    for (std::size_t p = first; p < last; ++p) {
                        const int level = Level(*shown_, p, c);
                        const float x0 = std::max(top.x, x(static_cast<double>(p)));
                        const float x1 = std::min(top.x + width, x(static_cast<double>(p + 1)));
                        if (level == 2) {
                            list.AddRectFilled({x0, high}, {x1, low}, kUnknown);
                        } else {
                            const float y = level == 1 ? high : low;
                            list.AddLine({x0, y}, {x1, y}, colour, 1.5f);
                            if (previous == 1 - level) {
                                list.AddLine({x0, high}, {x0, low}, colour, 1.5f);
                            }
                        }
                        previous = level;
                    }
                } else {
                    for (float px = 0; px < width; ++px) {
                        const double p0 = from_ + px * perPixel;
                        const auto begin = static_cast<std::size_t>(
                            std::max<double>(static_cast<double>(shown_->first), std::floor(p0)));
                        const auto end = std::min<std::size_t>(
                            kDepth, static_cast<std::size_t>(std::ceil(p0 + perPixel)));
                        int seen = 0;  // bit per level
                        for (std::size_t p = begin; p < end; ++p) {
                            seen |= 1 << Level(*shown_, p, c);
                        }
                        const float column = top.x + px;
                        if (seen == 0) {
                            continue;
                        }
                        if (seen == 4) {
                            list.AddRectFilled({column, high}, {column + 1, low}, kUnknown);
                        } else if (seen == 1 || seen == 2) {
                            const float y = seen == 2 ? high : low;
                            list.AddLine({column, y}, {column + 1, y}, colour, 1.5f);
                        } else {
                            list.AddRectFilled({column, high}, {column + 1, low}, colour);
                        }
                    }
                }
                if (!clip || clip->label != shown_->labels[c]) {
                    list.AddText({top.x + 4, top.y + 3}, kFaint, shown_->labels[c].c_str());
                }
            } else if (!shown_ && c == 0) {
                list.AddText({top.x + 4, top.y + 3}, ImGui::GetColorU32(kGrey),
                             "arm, then power on");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Trigger, marker, cursor, axis.
    if (shown_) {
        const float width = laneX1 - laneX0;
        const auto x = [&](double point) {
            return static_cast<float>(laneX0 + (point - from_) / (to_ - from_) * width);
        };
        const auto timeAt = [&](double point) {
            return static_cast<Time>(std::llround((point - static_cast<double>(shown_->trigger)) *
                                                  static_cast<double>(shown_->period)));
        };
        list.PushClipRect({laneX0, lanesTop}, {laneX1, lanesBottom + kAxisHeight}, true);
        const float trigger = x(static_cast<double>(shown_->trigger));
        list.AddLine({trigger, lanesTop}, {trigger, lanesBottom}, kTriggerLine, 1.0f);
        if (marker_) {
            list.AddLine({x(*marker_), lanesTop}, {x(*marker_), lanesBottom}, kMarkerLine, 1.0f);
        }
        if (hoverX) {
            list.AddLine({*hoverX, lanesTop}, {*hoverX, lanesBottom}, kCursorLine, 1.0f);
        }
        const double spanTime = (to_ - from_) * static_cast<double>(shown_->period);
        const Time step = TickStep(spanTime, std::max(2, static_cast<int>(width / 90)));
        const Time start = timeAt(from_);
        for (Time t = (start / step - 1) * step; t <= timeAt(to_); t += step) {
            const double point = static_cast<double>(shown_->trigger) +
                                 static_cast<double>(t) / static_cast<double>(shown_->period);
            const float tx = x(point);
            if (tx < laneX0 || tx > laneX1) {
                continue;
            }
            list.AddLine({tx, lanesBottom}, {tx, lanesBottom + 4}, kAxis);
            list.AddText({tx + 2, lanesBottom + 2}, kAxis, FormatTime(t, true).c_str());
        }
        list.PopClipRect();
        ImGui::Dummy({0, kAxisHeight});
        if (hoverX) {
            const double cursor = from_ + (*hoverX - laneX0) / width * (to_ - from_);
            std::string readout = "t = " + FormatTime(timeAt(cursor), true);
            if (marker_) {
                readout += "   dt = " + FormatTime(timeAt(cursor) - timeAt(*marker_), true);
            }
            ImGui::TextUnformatted(readout.c_str());
        } else {
            ImGui::TextColored(kGrey,
                               "wheel: zoom   drag: pan   click: marker   double-click: fit");
        }
    }
    if (changed) {
        session.setAnalyzer(settings);
    }
    ImGui::End();
}

}  // namespace bw
