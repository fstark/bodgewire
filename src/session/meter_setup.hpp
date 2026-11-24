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
    std::string label;  // "U1.14 QA", "Trace", "via"
};

// The Multimeter as the player set it up (Session::meter()): its mode and Test Leads, kept across
// rebuilds. Volts reads the red Lead's Net (black on GND, implied); Continuity, with the power
// off, whether both Leads are on one island of the current Copper.
class MeterSetup {
public:
    explicit MeterSetup(const SessionBuild& build) : build_(&build) {}

    void setMode(MeterMode mode);
    MeterMode mode() const { return mode_; }
    bool placeLead(TestLead lead);  // false: no Machine, or Continuity with the power on
    void liftLeads();
    bool blackNext() const { return blackNext_; }  // Continuity: the next click places black
    const std::optional<TestLead>& red() const { return red_; }
    const std::optional<TestLead>& black() const { return black_; }
    std::optional<NetId> net() const { return consumer_->touched(); }
    std::optional<double> volts() const;     // Volts mode; nullopt: no red Lead or no reading yet
    std::optional<bool> continuity() const;  // nullopt: a Lead missing or powered

private:
    friend class Session;                       // its builds and its Repairs
    void bind(MeterConsumer& consumer);         // a new build: the red Lead's Net watched
    void copperChanged() { findContinuity(); }  // a Repair
    void follow();                              // the consumer onto the red Lead's Net, in Volts
    void findContinuity();  // connected_ for the Leads as they are, in the current Copper

    const SessionBuild* build_;
    MeterConsumer* consumer_ = nullptr;
    MeterMode mode_ = MeterMode::Volts;
    std::optional<TestLead> red_, black_;
    bool blackNext_ = false;
    std::optional<bool> connected_;  // red_ and black_ on one island of the current Copper
    std::optional<double> rail_;     // RailVolts of the red Lead's Net
};

}  // namespace bw
