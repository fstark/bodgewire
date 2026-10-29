#pragma once

#include <span>

#include "sim/ids.hpp"
#include "sim/recorder.hpp"
#include "sim/time.hpp"

namespace bw {

// What the devices thread feeds: an output Device now, instruments later. Devices thread only.
class Consumer {
public:
    virtual ~Consumer() = default;
    virtual std::span<const NetId> nets() const = 0;    // watched in this order (ties)
    virtual void powerOn() = 0;                         // time restarts at 0
    virtual void sample(NetId net, Sample sample) = 0;  // in time order across all its Nets
    // Current value of a Net watched again; may be older than Samples already fed.
    virtual void seed(NetId /*net*/, Sample /*sample*/) {}
    virtual void advanceTo(Time time) = 0;  // end of a slice
    virtual void powerOff() = 0;
};

}  // namespace bw
