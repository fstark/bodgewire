#pragma once

// A circuit for the lamp tests (led_consumer_test.cpp, led_input_test.cpp).

#include <vector>

#include "parts/catalogue.hpp"
#include "sim/circuit.hpp"
#include "sim/logic.hpp"

namespace bw::test {

// X1 1 kHz on K; A pulled up; D1 between them. `second`: D2 from K (anode) to GND (cathode).
struct Blinker {
    Circuit circuit;
    NetId gnd, vcc, a, k;

    explicit Blinker(bool second = false) {
        gnd = circuit.addNet("GND");
        vcc = circuit.addNet("VCC");
        circuit.tie(gnd, LogicValue::Zero, DriveStrength::Supply);
        circuit.tie(vcc, LogicValue::One, DriveStrength::Supply);
        const std::vector<PartParam> freq{{"freq", "1kHz"}};
        Component& x1 = *FindPart("OSC")->create(circuit, "X1", freq).value();
        Component& d1 = *FindPart("LED")->create(circuit, "D1", {}).value();
        a = circuit.addNet("A");
        k = circuit.addNet("K");
        circuit.tie(a, LogicValue::One, DriveStrength::Weak);
        circuit.connect(x1, "GND", gnd);
        circuit.connect(x1, "VCC", vcc);
        circuit.connect(x1, "OUT", k);
        circuit.connect(d1, "A", a);
        circuit.connect(d1, "K", k);
        if (second) {
            Component& d2 = *FindPart("LED")->create(circuit, "D2", {}).value();
            circuit.connect(d2, "A", k);
            circuit.connect(d2, "K", gnd);
        }
        circuit.finalize();
    }
};

}  // namespace bw::test
