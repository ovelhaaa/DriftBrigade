#pragma once
#include "ParameterSmoother.h"

namespace drift {

// Internal experiment switch only; it is deliberately not a plug-in parameter.
enum class OrganicVariant { Wander, PaperNarrowband, PhaseDrift };

struct OrganicDiagnostics {
    double randomEnvelope = 0;
    double carrier = 0;
    double instantaneousRate = 0;
    double phaseDerivative = 0;
};

class OrganicModulator {
public:
    void prepare(double sr) noexcept;
    void reset(std::uint32_t seed) noexcept;
    void setRate(double hz) noexcept { rate.setTarget(bounded(hz, 0.025, 20.0)); }
    void setChaos(double c) noexcept { chaos.setTarget(unit(c)); }
    void setVariant(OrganicVariant value) noexcept { variant = value; }
    OrganicVariant getVariant() const noexcept { return variant; }
    // Used by DriftEngine, which already smooths shared macro controls.
    double process(double hz, double c) noexcept;
    double process() noexcept { return process(rate.next(), chaos.next()); }
    double randomValue() const noexcept { return lastRandom; }
    double segmentPhase() const noexcept { return randomPhase; }
    const OrganicDiagnostics& diagnostics() const noexcept { return diagnostic; }
    static double raisedCosine(double t) noexcept { return 0.5 - 0.5*std::cos(pi*t); }
    // B's normalized blend is the largest analytical bound (2 sqrt(2)).
    static constexpr double maximumMagnitude = 2.8284271247461903;
private:
    Random random; ParameterSmoother rate, chaos;
    OrganicVariant variant = OrganicVariant::Wander;
    double sampleRate = 48000, periodicPhase = 0, randomPhase = 0;
    double start = 0, end = 0, lastRandom = 0;
    OrganicDiagnostics diagnostic;
};
}
