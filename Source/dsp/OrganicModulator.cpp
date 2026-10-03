#include "OrganicModulator.h"
namespace drift {
void OrganicModulator::prepare(double sr) noexcept {
    sampleRate = sr; rate.prepare(sr); chaos.prepare(sr); reset(1);
}
void OrganicModulator::reset(std::uint32_t seed) noexcept {
    random.reset(seed); periodicPhase = random.next(); randomPhase = 0;
    start = 2*random.next()-1; end = 2*random.next()-1;
    lastRandom = start; diagnostic = {}; rate.reset(0.7); chaos.reset(0.5);
}
double OrganicModulator::process(double hz, double c) noexcept {
    hz = bounded(hz, 0.025, 20.0); c = unit(c);
    lastRandom = start + (end-start)*raisedCosine(randomPhase);
    const double sine = std::sin(2*pi*periodicPhase);
    double result = sine;
    double randomRate = 2*hz;

    diagnostic.randomEnvelope = lastRandom;
    diagnostic.carrier = sine; // cosine with a fixed -pi/2 phase offset
    diagnostic.instantaneousRate = hz;
    diagnostic.phaseDerivative = hz;

    switch (variant) {
        case OrganicVariant::Wander: {
            // Original M1 formula, unchanged.
            const double norm = std::sqrt(0.5 / (0.5*(1-c)*(1-c) + 0.25*c*c));
            result = ((1-c)*sine + c*lastRandom)*norm;
            break;
        }
        case OrganicVariant::PaperNarrowband: {
            // Spectrally shift smooth baseband random control around Motion.
            const double shifted = 2*lastRandom*sine;
            const double norm = std::sqrt(0.5 / (0.5*((1-c)*(1-c)+c*c)));
            result = ((1-c)*sine + c*shifted)*norm;
            // Envelope changes at half the carrier rate (one target/cycle).
            randomRate = hz;
            break;
        }
        case OrganicVariant::PhaseDrift: {
            // Smooth bounded timing drift; average rate remains Motion in expectation.
            const double instantaneous = hz*(1+0.75*c*lastRandom);
            diagnostic.instantaneousRate = instantaneous;
            diagnostic.phaseDerivative = instantaneous;
            periodicPhase += instantaneous/sampleRate;
            randomRate = 0.5*hz;
            result = sine;
            break;
        }
    }
    if (variant != OrganicVariant::PhaseDrift) periodicPhase += hz/sampleRate;
    if (periodicPhase >= 1) periodicPhase -= 1;
    randomPhase += randomRate/sampleRate;
    if (randomPhase >= 1) { randomPhase -= 1; start = end; end = 2*random.next()-1; }
    return result;
}
}
