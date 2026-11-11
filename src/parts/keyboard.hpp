#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr Time kKeyboardDelay = 100'000;          // ps: encoder output delay
inline constexpr Time kKeyboardSetup = 1'000'000;        // ps: D0-D6 steady before STROBE rises
inline constexpr Time kKeyboardGap = 1'000'000;          // ps: STROBE low before the next key
inline constexpr Time kKeyboardStrobe = 10'000'000;      // ps: default strobe=
inline constexpr std::size_t kKeyboardWaiting = 32;      // keys waiting at most
inline constexpr std::uint8_t kKeyboardLastCode = 0x5F;  // the highest code a key sends

// The 7-bit code the keyboard sends for a typed character: 0x20-0x5F as is, a-z as A-Z,
// '\r' or '\n' 0x0D (Return), '\b' 0x08 (Backspace); nullopt for anything else.
std::optional<std::uint8_t> KeyboardCode(char32_t key);

// ASCII keyboard behind a KK-10 Connector, logic only (an encoder chip). Pins 1-7 D0-D6,
// 8 STROBE, 9 VCC, 10 GND (pin index = KiCad pad - 1). A key's code goes out on D0-D6, then
// STROBE pulses high for `strobe`; the code stays until the next key. Keys typed meanwhile wait
// in turn.
class Keyboard final : public Component {
public:
    Keyboard(std::string reference, Time strobe);  // Check(strobe >= 1 us)
    static constexpr PinIndex kD0 = 0, kStrobe = 7;

    // One producer thread (the UI; bw run's own). False when kKeyboardWaiting keys wait.
    // Follow with Simulator or Runner wake(): an idle keyboard takes it at its next evaluation.
    bool type(std::uint8_t code);
    // Producer side, any time: the keys waiting now are skipped (power-off).
    void dropWaiting();

    void powerOn(PowerOnContext& ctx) override;  // Idle, STROBE low; waiting keys kept
    void evaluate(EvalContext& ctx) override;

private:
    enum class Phase : std::uint8_t { Idle, Setup, High, Gap };
    void next(EvalContext& ctx, Phase phase, Time after);

    Time strobe_;
    Phase phase_ = Phase::Idle;
    Time nextAt_ = 0;  // ps; outside Idle: when the phase's own wake is due
    std::array<std::uint8_t, kKeyboardWaiting> waiting_{};
    std::atomic<std::uint32_t> head_{0}, tail_{0};  // SPSC: type() writes head_, evaluate tail_
    std::atomic<std::uint32_t> dropBefore_{0};      // dropWaiting(): keys before it are skipped
};

}  // namespace bw
