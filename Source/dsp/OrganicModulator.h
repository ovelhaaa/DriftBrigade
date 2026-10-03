#pragma once
#include "ParameterSmoother.h"
namespace drift {
class OrganicModulator {
public:
    void prepare(double sr) noexcept;
    void reset(std::uint32_t seed) noexcept;
    void setRate(double hz) noexcept { rate.setTarget(bounded(hz, 0.025, 20.0)); }
    void setChaos(double c) noexcept { chaos.setTarget(unit(c)); }
    // Used by DriftEngine, which already smooths shared macro controls.
    double process(double hz, double c) noexcept;
    double process() noexcept { return process(rate.next(), chaos.next()); }
    double randomValue() const noexcept { return lastRandom; }
    double segmentPhase() const noexcept { return randomPhase; }
    static double raisedCosine(double t) noexcept { return 0.5 - 0.5*std::cos(pi*t); }
    static constexpr double maximumMagnitude = 1.7320508075688772;
private:
    Random random; ParameterSmoother rate, chaos;
    double sampleRate = 48000, periodicPhase = 0, randomPhase = 0;
    double start = 0, end = 0, lastRandom = 0;
};
}
