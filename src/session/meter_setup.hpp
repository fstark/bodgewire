#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "board/board.hpp"
#include "runner/meter_consumer.hpp"
#include "session/leads.hpp"

namespace bw {

enum class MeterMode : std::uint8_t { Volts, Continuity };

struct TestLead {
    Point at;           // nm
    Layer layer;        // as ProbeTip
    std::string label;  // "U1.14 QA leg", "Trace", "via"
};

// The Multimeter as the player holds it (Session::meter(), P8g). The power is its mode: Volts
// while the Machine is powered (black on GND, implied), Continuity while it is off. The red Test
// Lead is on the Board while a press holds it; in Continuity the black one is clipped and stays,
// across rebuilds and Repairs, until unclipped or the power goes on.
class MeterSetup {
public:
    explicit MeterSetup(const SessionBuild& build) : build_(&build) {}

    MeterMode mode() const { return build_->powered ? MeterMode::Volts : MeterMode::Continuity; }
    void hold(std::optional<TestLead> red);  // nullopt: lifted; the same place: no-op
    bool clip(TestLead black);               // false: no Machine, or powered
    void unclip();
    const std::optional<TestLead>& red() const { return red_; }
    const std::optional<TestLead>& black() const { return black_; }
    std::optional<NetId> net() const { return consumer_->touched(); }
    // Volts: the red Lead's average (0 on bare Copper, a rail's volts); nullopt in Continuity,
    // with no red Lead, or before the first refresh. Run state aside: the LCD shows dashes while
    // paused (bw_ui).
    std::optional<double> volts() const;
    // Continuity, both Leads on: whether they are on one island of the current Copper.
    std::optional<bool> continuity() const;

private:
    friend class Session;                       // its builds, power and Repairs
    void bind(MeterConsumer& consumer);         // a new build: the red Lead's Net watched
    void copperChanged() { findContinuity(); }  // a Repair
    void powerChanged();                        // on: the clip lifted; either way, followed
    void follow();          // the consumer onto the red Lead's Net, in Volts; else lifted
    void findContinuity();  // connected_ for the Leads as they are, in the current Copper

    const SessionBuild* build_;
    MeterConsumer* consumer_ = nullptr;
    std::optional<TestLead> red_, black_;
    std::optional<bool> connected_;  // red_ and black_ on one island of the current Copper
    std::optional<double> rail_;     // RailVolts of the red Lead's Net
};

}  // namespace bw
