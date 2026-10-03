#pragma once
#include "MultiscaleBank.h"
#include "FractionalDelay.h"
#include "OrganicModulator.h"
#include "EnvelopeFollower.h"
#include "DepthMapping.h"
#include "params/ParameterSpecs.h"
namespace drift {
struct Telemetry {
    double organic = 0, randomControl = 0, envelope = 0, effectiveChaos = 0, effectiveFeedback = 0, wetProminence = 1;
    std::array<std::array<double, 4>, 2> modulation {}, delaySeconds {};
};
class DriftEngine {
public:
    void prepare(double sr, std::uint32_t seed = 0x44524946u);
    void reset(std::uint32_t seed = 0x44524946u) noexcept;
    void setParameters(const EngineParameters& p) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t samples) noexcept;
    std::array<double, 2> processSample(double left, double right, bool mono = false) noexcept;
    const Telemetry& telemetry() const noexcept { return trace; }
    void usePerceptualDepth(bool enabled) noexcept { perceptualDepth = enabled; }
    std::size_t delayCapacity() const noexcept { return delays[0][0].capacity(); }
    static constexpr double modulationBound = 3.4641016151377544;
private:
    double sampleRate = 48000; bool perceptualDepth = true;
    EngineParameters targets;
    std::array<ParameterSmoother, Count> controls;
    std::array<MultiscaleBank, 2> banks;
    std::array<std::array<DelayPath, 4>, 2> delays;
    OrganicModulator common;
    std::array<OrganicModulator, 4> independent, stereo;
    EnvelopeFollower envelope; ParameterSmoother envelopeControl;
    std::array<std::array<double, 4>, 2> currentDelays {};
    Telemetry trace;
};
}
