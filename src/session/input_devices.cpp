#include "session/input_devices.hpp"

#include <algorithm>
#include <chrono>

#include "parts/keyboard.hpp"
#include "parts/paddle.hpp"
#include "parts/switch.hpp"

namespace bw {

namespace {

// A hand's timing is not tied to the 1 ms slices: without this, every hold lasts whole
// milliseconds and a 1 MHz counter cycling every 6 µs keeps its parity (the dice only rolls even).
Time PressJitter() {
    const auto ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return static_cast<Time>(ns % 1'000'000) * 1000;  // ps, within 1 ms
}

}  // namespace

void InputDevices::bind(const LoadedCircuit& design, LoadedCircuit& live, Runner& runner) {
    design_ = &design;
    live_ = &live;
    runner_ = &runner;
    for (const auto& [reference, turn] : knobs_) {
        if (Paddle* paddle = PaddleOf(live, reference)) {
            paddle->setKnob(turn);
        }
    }
}

void InputDevices::poweredOn() {
    powered_ = true;
    typed_.clear();
}

void InputDevices::poweredOff() {
    for (const std::string& reference : pressed_) {
        if (Switch* sw = SwitchOf(*live_, reference)) {
            runner_->post([sw] { sw->setPressed(false); });
        }
    }
    pressed_.clear();
    for (const auto& [reference, key] : matrixHeld_) {
        if (KeyMatrix* matrix = KeyMatrixOf(*live_, reference)) {
            runner_->post([matrix, key] { matrix->setHeld(key, false); });
        }
    }
    matrixHeld_.clear();
    for (const std::string& reference : keyboards()) {
        if (Keyboard* keyboard = KeyboardOf(*live_, reference)) {
            keyboard->dropWaiting();
        }
    }
    for (const std::string& reference : matrixKeyboards()) {
        if (KeyMatrix* matrix = KeyMatrixOf(*live_, reference)) {
            matrix->dropWaiting();
        }
    }
    powered_ = false;
}

template <class Of>
std::vector<std::string> InputDevices::referencesOf(Of of) const {
    std::vector<std::string> references;
    for (const LoadedPart& part : design_->parts) {
        if (of(*design_, part.reference) != nullptr) {
            references.push_back(part.reference);
        }
    }
    return references;
}

std::vector<std::string> InputDevices::switches() const {
    return referencesOf([](const LoadedCircuit& c, std::string_view r) { return SwitchOf(c, r); });
}

std::vector<SwitchKey> InputDevices::switchKeys() const {
    std::vector<SwitchKey> keys;
    for (const std::string& reference : switches()) {
        if (const Switch* sw = SwitchOf(*design_, reference); sw && !sw->pcKey().empty()) {
            keys.push_back({reference, std::string(sw->pcKey())});
        }
    }
    return keys;
}

void InputDevices::setPressed(std::string_view reference, bool pressed) {
    if (!powered_ || pressed == pressed_.contains(reference)) {
        return;
    }
    if (pressed) {
        pressed_.emplace(reference);
    } else {
        pressed_.erase(pressed_.find(reference));
    }
    if (Switch* sw = SwitchOf(*live_, reference)) {
        runner_->post([sw, pressed] { sw->setPressed(pressed); }, sw->id(), PressJitter());
    }
}

std::vector<std::string> InputDevices::knobs() const {
    return referencesOf([](const LoadedCircuit& c, std::string_view r) { return PaddleOf(c, r); });
}

bool InputDevices::setKnob(std::string_view reference, double turn) {
    if (PaddleOf(*design_, reference) == nullptr) {
        return false;
    }
    turn = std::clamp(turn, 0.0, 1.0);
    knobs_.insert_or_assign(std::string(reference), turn);
    if (Paddle* paddle = PaddleOf(*live_, reference)) {
        runner_->post([paddle, turn] { paddle->setKnob(turn); });
    }
    return true;
}

double InputDevices::knob(std::string_view reference) const {
    const auto it = knobs_.find(reference);
    return it == knobs_.end() ? 0.0 : it->second;
}

std::vector<std::string> InputDevices::keyboards() const {
    return referencesOf(
        [](const LoadedCircuit& c, std::string_view r) { return KeyboardOf(c, r); });
}

bool InputDevices::type(std::string_view reference, char32_t key) {
    if (KeyMatrixOf(*live_, reference) != nullptr) {
        return typeText(reference, std::u32string_view{&key, 1});
    }
    const auto code = KeyboardCode(key);
    return code && typeCode(reference, *code);
}

bool InputDevices::typeText(std::string_view reference, std::u32string_view text) {
    if (KeyMatrix* matrix = KeyMatrixOf(*live_, reference)) {
        if (!powered_ || !matrix->type(text)) {
            return false;
        }
        runner_->wake(matrix->component().id(), PressJitter());
        return true;
    }
    bool any = false;
    for (const char32_t c : text) {
        const auto code = KeyboardCode(c);
        any = (code && typeCode(reference, *code)) || any;
    }
    return any;
}

bool InputDevices::press(std::string_view reference, MatrixPress press) {
    KeyMatrix* matrix = KeyMatrixOf(*live_, reference);
    if (matrix == nullptr || !powered_ || !matrix->press(press)) {
        return false;
    }
    runner_->wake(matrix->component().id(), PressJitter());
    return true;
}

std::vector<std::string> InputDevices::matrixKeyboards() const {
    return referencesOf(
        [](const LoadedCircuit& c, std::string_view r) { return KeyMatrixOf(c, r); });
}

std::optional<KeyMatrixKind> InputDevices::matrixKind(std::string_view reference) const {
    const KeyMatrix* matrix = KeyMatrixOf(*live_, reference);
    if (matrix == nullptr) {
        return std::nullopt;
    }
    return matrix->kind();
}

void InputDevices::setMatrixKeyHeld(std::string_view reference, std::size_t key, bool held) {
    KeyMatrix* matrix = KeyMatrixOf(*live_, reference);
    if (!powered_ || matrix == nullptr || key >= matrix->keyCount() ||
        held == matrixKeyHeld(reference, key)) {
        return;
    }
    if (held) {
        matrixHeld_.emplace(std::string(reference), key);
    } else {
        matrixHeld_.erase({std::string(reference), key});
    }
    runner_->post([matrix, key, held] { matrix->setHeld(key, held); }, matrix->component().id(),
                  PressJitter());
}

bool InputDevices::matrixKeyHeld(std::string_view reference, std::size_t key) const {
    return matrixHeld_.contains({std::string(reference), key});
}

bool InputDevices::matrixKeyDown(std::string_view reference, std::size_t key) const {
    if (matrixKeyHeld(reference, key)) {
        return true;
    }
    const KeyMatrix* matrix = KeyMatrixOf(*live_, reference);
    return powered_ && matrix != nullptr && matrix->scripted(key);
}

bool InputDevices::typeCode(std::string_view reference, std::uint8_t code) {
    if (!powered_ || code > kKeyboardLastCode || KeyboardOf(*design_, reference) == nullptr) {
        return false;
    }
    Keyboard* keyboard = KeyboardOf(*live_, reference);
    if (keyboard == nullptr || !keyboard->type(code)) {
        return false;
    }
    runner_->wake(keyboard->id(), PressJitter());
    std::string& sent = typed_[std::string(reference)];
    sent.push_back(static_cast<char>(code));
    if (sent.size() > kTypedShown) {
        sent.erase(0, sent.size() - kTypedShown);
    }
    return true;
}

std::string InputDevices::typed(std::string_view reference) const {
    const auto it = typed_.find(reference);
    return it == typed_.end() ? std::string() : it->second;
}

}  // namespace bw
