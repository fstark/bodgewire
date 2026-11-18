#include "view/machine_keys.hpp"

namespace bw {

void MachineKeys::setTakers(bool takers) {
    takers_ = takers;
    if (!takers) {
        to_ = KeysTo::Bench;
    }
}

void MachineKeys::front() {
    if (takers_) {
        to_ = KeysTo::Machine;
    }
}

void MachineKeys::bench() { to_ = KeysTo::Bench; }

void MachineKeys::toggle() {
    if (machine()) {
        bench();
    } else {
        front();
    }
}

}  // namespace bw
