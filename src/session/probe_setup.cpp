#include "session/probe_setup.hpp"

namespace bw {

void ProbeSetup::bind(ProbeConsumer& consumer) {
    consumer_ = &consumer;
    follow();
}

void ProbeSetup::touch(std::optional<ProbeTip> tip) {
    if (build_->machine == nullptr) {
        return;
    }
    tip_ = tip;
    follow();
}

ProbeLamps ProbeSetup::lamps() const { return build_->powered ? consumer_->lamps() : ProbeLamps{}; }

void ProbeSetup::follow() {
    if (build_->machine == nullptr) {
        return;
    }
    consumer_->touch(*build_->runner,
                     tip_ ? build_->machine->probeNet(*build_->circuit, tip_->layer, tip_->at)
                          : std::nullopt);
}

}  // namespace bw
