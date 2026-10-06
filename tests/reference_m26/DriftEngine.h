#pragma once
#include "dsp/MultiscaleBank.h"
#include "dsp/FractionalDelay.h"
#include "dsp/OrganicModulator.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/DepthMapping.h"
#include "params/ParameterSpecs.h"
namespace drift {
enum class PreM27DynamicsMode { Current, MotionOnly };
struct PreM27Telemetry {
    double organic = 0, randomControl = 0, envelope = 0, effectiveChaos = 0, effectiveFeedback = 0, wetProminence = 1;
    double requestedExcursionSeconds = 0, actualExcursionSeconds = 0;
    std::array<std::array<double, 4>, 2> modulation {}, delaySeconds {};
    OrganicDiagnostics organicDiagnostics;
};
class PreM27DriftEngine {
public:
    void prepare(double sr, std::uint32_t seed = 0x44524946u);
    void reset(std::uint32_t seed = 0x44524946u) noexcept;
    void setParameters(const EngineParameters& p) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t samples) noexcept;
    std::array<double, 2> processSample(double left, double right, bool mono = false) noexcept;
    const PreM27Telemetry& telemetry() const noexcept { return trace; }
    void usePerceptualDepth(bool enabled) noexcept { perceptualDepth = enabled; }
    void setOrganicVariant(OrganicVariant value) noexcept;
    void setBankMode(BankMode value) noexcept;
    void setPreM27DynamicsMode(PreM27DynamicsMode value) noexcept { dynamicsMode = value; }
    std::size_t delayCapacity() const noexcept { return delays[0][0].capacity(); }
    // Two successive variance-preserving coherence/stereo sums each have a
    // conservative sqrt(2) magnitude factor: combined bound = 2*source bound.
    static constexpr double combinedModulationBound(OrganicVariant value) noexcept {
        return 2*OrganicModulator::maximumMagnitude(value);
    }
private:
    double sampleRate = 48000; bool perceptualDepth = true;
    OrganicVariant organicVariant = OrganicVariant::Wander;
    BankMode bankMode = BankMode::Gentle;
    PreM27DynamicsMode dynamicsMode = PreM27DynamicsMode::Current;
    EngineParameters targets;
    std::array<ParameterSmoother, Count> controls;
    std::array<MultiscaleBank, 2> banks;
    std::array<std::array<DelayPath, 4>, 2> delays;
    OrganicModulator common;
    std::array<OrganicModulator, 4> independent, stereo;
    EnvelopeFollower envelope; ParameterSmoother envelopeControl;
    std::array<std::array<double, 4>, 2> currentDelays {};
    PreM27Telemetry trace;
};
}


