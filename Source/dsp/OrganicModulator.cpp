#include "OrganicModulator.h"
namespace drift {
void OrganicModulator::prepare(double sr) noexcept {
    sampleRate = sr; rate.prepare(sr); chaos.prepare(sr); reset(1);
}
void OrganicModulator::reset(std::uint32_t seed) noexcept {
    random.reset(seed); periodicPhase = random.next(); randomPhase = 0;
    start = 2*random.next()-1; end = 2*random.next()-1;
    lastRandom = start; rate.reset(0.7); chaos.reset(0.5);
}
double OrganicModulator::process(double hz, double c) noexcept {
    hz = bounded(hz, 0.025, 20.0); c = unit(c);
    lastRandom = start + (end-start)*raisedCosine(randomPhase);
    const double sine = std::sin(2*pi*periodicPhase);
    // Random variance is 1/4; sine variance is 1/2. Normalize expected RMS.
    const double norm = std::sqrt(0.5 / (0.5*(1-c)*(1-c) + 0.25*c*c));
    const double result = ((1-c)*sine + c*lastRandom)*norm;
    periodicPhase += hz/sampleRate;
    if (periodicPhase >= 1) periodicPhase -= 1;
    randomPhase += 2*hz/sampleRate;
    if (randomPhase >= 1) { randomPhase -= 1; start = end; end = 2*random.next()-1; }
    return result;
}
}
