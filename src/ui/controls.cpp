#include "ui/controls.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <imgui_internal.h>
#include <string_view>
#include <utility>

#include "ui/touch.hpp"

namespace bw {

namespace {

constexpr float kFontSize = 13;  // the UI font's, as ImGuiScreen loads it
constexpr float kPadKey = 48;    // a pad key's side
constexpr float kPadGap = 4;
constexpr ImVec4 kRefused{0.55f, 0.1f, 0.1f, 1.0f};

// The pad open now: the field it edits (0: none), its copy of the text.
struct Pad {
    ImGuiID field = 0;
    std::string text;
    bool fresh = true;  // nothing typed yet: the field's value shown
    char unit = 0;      // N, U or M typed on the PC: an S after it makes the unit
    int seen = 0;       // the frame it was last drawn: a pad not drawn is closed
};
Pad pad_;

// The PC's keys that are a pad key.
constexpr std::array<std::pair<ImGuiKey, std::string_view>, 29> kKeys{{
    {ImGuiKey_0, "0"},
    {ImGuiKey_1, "1"},
    {ImGuiKey_2, "2"},
    {ImGuiKey_3, "3"},
    {ImGuiKey_4, "4"},
    {ImGuiKey_5, "5"},
    {ImGuiKey_6, "6"},
    {ImGuiKey_7, "7"},
    {ImGuiKey_8, "8"},
    {ImGuiKey_9, "9"},
    {ImGuiKey_Keypad0, "0"},
    {ImGuiKey_Keypad1, "1"},
    {ImGuiKey_Keypad2, "2"},
    {ImGuiKey_Keypad3, "3"},
    {ImGuiKey_Keypad4, "4"},
    {ImGuiKey_Keypad5, "5"},
    {ImGuiKey_Keypad6, "6"},
    {ImGuiKey_Keypad7, "7"},
    {ImGuiKey_Keypad8, "8"},
    {ImGuiKey_Keypad9, "9"},
    {ImGuiKey_A, "A"},
    {ImGuiKey_B, "B"},
    {ImGuiKey_C, "C"},
    {ImGuiKey_D, "D"},
    {ImGuiKey_E, "E"},
    {ImGuiKey_F, "F"},
    {ImGuiKey_X, "x"},
    {ImGuiKey_Period, "."},
    {ImGuiKey_KeypadDecimal, "."},
}};

// A button drawn lit (the choice made, the value shown).
bool LitButton(const char* label, ImVec2 size, bool lit) {
    if (lit) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
    }
    const bool clicked = ImGui::Button(label, size);
    if (lit) {
        ImGui::PopStyleColor();
    }
    return clicked;
}

}  // namespace

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding = {8, (kControl - kFontSize) / 2};
    style.ScrollbarSize = 20;
    style.GrabMinSize = 20;
}

void BigText(const char* text, ImU32 colour) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::CalcTextSize(text);
    ImGui::Dummy({size.x, kBig});
    ImGui::GetWindowDrawList()->AddText({at.x, at.y + (kBig - size.y) / 2},
                                        colour != 0 ? colour : ImGui::GetColorU32(ImGuiCol_Text),
                                        text);
}

bool Stepper(const char* id, std::span<const std::string> values, std::size_t& index,
             float valueWidth) {
    if (values.empty()) {
        return false;
    }
    bool changed = false;
    ImGui::PushID(id);
    ImGui::BeginGroup();
    const NoFingerPadding edgeToEdge;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, ImGui::GetStyle().ItemSpacing.y});
    ImGui::BeginDisabled(index == 0);
    if (ImGui::Button("-", {kBig, kBig})) {
        --index;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button((values[index] + "##value").c_str(), {valueWidth, kBig})) {
        ImGui::OpenPopup("##values");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= values.size());
    if (ImGui::Button("+", {kBig, kBig})) {
        ++index;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    if (ImGui::BeginPopup("##values")) {
        // Every value in two columns, down the first then the second.
        const std::size_t rows = (values.size() + 1) / 2;
        for (std::size_t row = 0; row < rows; ++row) {
            for (std::size_t column = 0; column < 2; ++column) {
                const std::size_t i = column * rows + row;
                if (i >= values.size()) {
                    continue;
                }
                if (column > 0) {
                    ImGui::SameLine();
                }
                ImGui::PushID(static_cast<int>(i));
                if (LitButton(values[i].c_str(), {valueWidth + 16, kBig}, i == index)) {
                    changed = changed || i != index;
                    index = i;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

bool Choice(const char* id, std::span<const char* const> labels, std::size_t& index, float height) {
    bool changed = false;
    ImGui::PushID(id);
    ImGui::BeginGroup();  // one item for what follows it on the line (CentreOnBig, SameLine)
    const NoFingerPadding edgeToEdge;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, ImGui::GetStyle().ItemSpacing.y});
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine();
        }
        ImGui::PushID(static_cast<int>(i));
        if (LitButton(labels[i], {0, height}, i == index) && i != index) {
            index = i;
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

bool ChoicePopup(const char* id, std::span<const char* const> labels, std::size_t& index,
                 float width, float height) {
    bool changed = false;
    ImGui::PushID(id);
    if (ImGui::Button(labels[index], {width, height})) {
        ImGui::OpenPopup("##choices");
    }
    if (ImGui::BeginPopup("##choices")) {
        const NoFingerPadding edgeToEdge;
        for (std::size_t i = 0; i < labels.size(); ++i) {
            if (i > 0) {
                ImGui::SameLine(0, 1);
            }
            ImGui::PushID(static_cast<int>(i));
            if (LitButton(labels[i], {std::max(kBig, width), kBig}, i == index)) {
                changed = changed || i != index;
                index = i;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

void DragScroll::update(bool taken) {
    constexpr float kGlideTau = 0.25f;  // s: the glide's speed falls by e in that time
    constexpr float kGlideEnd = 20;     // px a second: under it the glide is over
    const ImGuiIO& io = ImGui::GetIO();
    scrolled_ = false;
    if (taken) {
        pressed_ = dragging_ = false;
        velocity_ = 0;
        return;
    }
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed_ = hovered && ImGui::GetScrollMaxY() > 0 &&
                   ImGui::GetActiveID() !=
                       ImGui::GetWindowScrollbarID(ImGui::GetCurrentWindow(), ImGuiAxis_Y);
        dragging_ = false;
        velocity_ = 0;  // a press stops the glide
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        pressed_ = dragging_ = false;
    }
    if (hovered && io.MouseWheel != 0) {
        velocity_ = 0;  // the wheel stops the glide too
    }
    if (pressed_ && !dragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        dragging_ = true;
        if (ImGui::GetActiveID() != ImGui::GetCurrentWindow()->MoveId) {
            ImGui::ClearActiveID();  // the item the press began on lets go
        }
    }
    const float before = ImGui::GetScrollY();
    if (dragging_) {
        ImGui::SetScrollY(before - io.MouseDelta.y);
        if (io.DeltaTime > 0) {
            velocity_ = 0.6f * velocity_ + 0.4f * (-io.MouseDelta.y / io.DeltaTime);
        }
        scrolled_ = io.MouseDelta.y != 0;
    } else if (velocity_ != 0) {
        const float to =
            std::clamp(before + velocity_ * io.DeltaTime, 0.0f, ImGui::GetScrollMaxY());
        ImGui::SetScrollY(to);
        velocity_ *= std::exp(-io.DeltaTime / kGlideTau);
        if (std::abs(velocity_) < kGlideEnd || to <= 0 || to >= ImGui::GetScrollMaxY()) {
            velocity_ = 0;
        }
    }
    if (const FingerFrame& touch = Touch(); hovered && touch.two && touch.panY != 0) {
        ImGui::SetScrollY(ImGui::GetScrollY() - touch.panY);
        velocity_ = 0;
        scrolled_ = true;
    }
}

bool NumberField(const char* id, std::string& text, PadKind kind, float width, bool refused,
                 std::size_t limit) {
    ImGui::PushID(id);
    const ImGuiID field = ImGui::GetID("##field");
    const bool editing = pad_.field == field;
    // The field: a frame showing the value, or the pad's copy while it is open.
    ImGui::PushStyleColor(ImGuiCol_Button, refused && !editing
                                               ? kRefused
                                               : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.0f, 0.5f});
    const std::string shown = (editing ? pad_.text + "_" : text) + "##field";
    const bool clicked = ImGui::Button(shown.c_str(), {width, 0});
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    const ImVec2 fieldMin = ImGui::GetItemRectMin(), fieldMax = ImGui::GetItemRectMax();
    if (clicked && !editing) {
        pad_ = Pad{field, text, true, 0, ImGui::GetFrameCount()};
        ImGui::OpenPopup("##pad");
    }
    bool confirmed = false;
    if (pad_.field == field) {
        // Under the field, or above it near the window's bottom: beside it, never on it.
        const std::size_t rows = PadKeys(kind).size() / kPadColumns;
        const ImVec2 padding = ImGui::GetStyle().WindowPadding;
        const float padHeight =
            static_cast<float>(rows) * (kPadKey + kPadGap) - kPadGap + 2 * padding.y;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const bool below = fieldMax.y + 4 + padHeight <= viewport->WorkPos.y + viewport->WorkSize.y;
        ImGui::SetNextWindowPos({fieldMin.x, below ? fieldMax.y + 4 : fieldMin.y - 4 - padHeight});
        if (ImGui::BeginPopup("##pad", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_AlwaysAutoResize)) {
            pad_.seen = ImGui::GetFrameCount();
            const NoFingerPadding edgeToEdge;
            bool done = false, cancel = false;
            const auto press = [&](std::string_view key) {
                if (key == kPadConfirm) {
                    done = true;
                    return;
                }
                const std::string typed = PadTyped(pad_.text, key, kind, limit, pad_.fresh);
                if (IsPadUnit(key)) {
                    if (typed != pad_.text) {  // a number to put it after
                        pad_.text = typed;
                        pad_.fresh = false;
                        done = true;
                    }
                } else if (typed != pad_.text || key == kPadBackspace) {
                    pad_.text = typed;
                    pad_.fresh = false;
                }
            };
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {kPadGap, kPadGap});
            const auto keys = PadKeys(kind);
            for (std::size_t k = 0; k < keys.size(); ++k) {
                // The same key twice in a row (OK OK): one button two cells wide.
                if (k % kPadColumns != 0 && !keys[k].empty() && keys[k] == keys[k - 1]) {
                    continue;
                }
                if (k % kPadColumns != 0) {
                    ImGui::SameLine();
                }
                const bool wide =
                    k + 1 < keys.size() && (k + 1) % kPadColumns != 0 && keys[k + 1] == keys[k];
                const ImVec2 size{wide ? 2 * kPadKey + kPadGap : kPadKey, kPadKey};
                if (keys[k].empty()) {
                    ImGui::Dummy(size);
                    continue;
                }
                ImGui::PushID(static_cast<int>(k));
                const std::string label(keys[k] == "us" ? "\xc2\xb5s" : keys[k]);
                if (ImGui::Button(label.c_str(), size)) {
                    press(keys[k]);
                }
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            // The PC's keys.
            for (const auto& [key, name] : kKeys) {
                if (ImGui::IsKeyPressed(key, true)) {
                    pad_.unit = 0;
                    press(name);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) {
                press(kPadBackspace);
            }
            if (kind == PadKind::Duration) {
                for (const auto& [key, letter] :
                     {std::pair{ImGuiKey_N, 'n'}, std::pair{ImGuiKey_U, 'u'},
                      std::pair{ImGuiKey_M, 'm'}}) {
                    if (ImGui::IsKeyPressed(key, false)) {
                        pad_.unit = letter;
                    }
                }
                if (pad_.unit != 0 && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
                    press(std::string{pad_.unit, 's'});
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
                done = true;
            }
            cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
            if (done || cancel) {
                if (done && !pad_.fresh) {  // something was typed: also the same value again
                    text = pad_.text;
                    confirmed = true;
                }
                pad_ = Pad{};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else {
            pad_ = Pad{};  // closed by a click elsewhere
        }
    }
    ImGui::PopID();
    return confirmed;
}

bool NumberPadOpen() {
    // A pad whose field was not drawn any more (its window closed) is gone.
    if (pad_.field != 0 && ImGui::GetFrameCount() - pad_.seen > 1) {
        pad_ = Pad{};
    }
    return pad_.field != 0;
}

}  // namespace bw
