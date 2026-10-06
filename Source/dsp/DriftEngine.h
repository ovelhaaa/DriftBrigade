#pragma once
#include "MultiscaleBank.h"
#include "FractionalDelay.h"
#include "BBDModulatedDelayVoice.h"
#include "OrganicModulator.h"
#include "EnvelopeFollower.h"
#include "DepthMapping.h"
#include "params/ParameterSpecs.h"
#include <limits>
namespace drift {
enum class DynamicsMode { Current, MotionOnly };
struct Telemetry {
    double organic = 0, randomControl = 0, envelope = 0, effectiveChaos = 0, effectiveFeedback = 0, wetProminence = 1;
    double requestedExcursionSeconds = 0, actualExcursionSeconds = 0;
    std::array<std::array<double, 4>, 2> modulation {}, delaySeconds {};
    OrganicDiagnostics organicDiagnostics;
#ifdef DRIFT_BBD_INSTRUMENT
    std::uint64_t physicalLimitSamples = 0;
    std::uint64_t totalBBDEvents = 0;
    std::uint32_t maximumBBDEventsPerSample = 0;
    double maximumBBDClock = 0, maximumBBDInternalPeak = 0, minimumBBDNominalOccupancy = 1;
    double minimumBBDClock = std::numeric_limits<double>::max(), accumulatedBBDClock = 0;
    std::array<std::array<double,4>,2> bandInput {}, bandWet {};
#endif
};
class DriftEngine {
public:
    // Configuration is immutable after prepare: no callback reconstruction.
    bool setDelayBackend(DelayBackend value, BBDVoiceConfig config = BBDVoiceConfig::fullResearchFixture()) noexcept {
        if (prepared) return false;
        backend = value; bbdConfig = config; return true;
    }
    DelayBackend delayBackend() const noexcept { return backend; }
    double minimumDelaySeconds() const noexcept { return backend == DelayBackend::DigitalFractional ? 4.0/sampleRate : double(bbdConfig.physicalStages)/(sampleRate*ClockedBBDCore::maximumEventsPerHostSample); }
    double maximumDelaySeconds() const noexcept { return backend == DelayBackend::DigitalFractional ? .055 : double(bbdConfig.physicalStages)/2; }
    const BBDModulatedDelayVoice& bbdVoice(std::size_t ch, std::size_t band) const noexcept { return bbdDelays[ch][band]; }
    void prepare(double sr, std::uint32_t seed = 0x44524946u);
    void reset(std::uint32_t seed = 0x44524946u) noexcept;
    void setParameters(const EngineParameters& p) noexcept;
    void process(float* const* channels, std::size_t channelCount, std::size_t samples) noexcept;
    std::array<double, 2> processSample(double left, double right, bool mono = false) noexcept;
    const Telemetry& telemetry() const noexcept { return trace; }
    void usePerceptualDepth(bool enabled) noexcept { perceptualDepth = enabled; }
    void setOrganicVariant(OrganicVariant value) noexcept;
    void setBankMode(BankMode value) noexcept;
    void setDynamicsMode(DynamicsMode value) noexcept { dynamicsMode = value; }
    std::size_t delayCapacity() const noexcept { return delays[0][0].capacity(); }
    // Two successive variance-preserving coherence/stereo sums each have a
    // conservative sqrt(2) magnitude factor: combined bound = 2*source bound.
    static constexpr double combinedModulationBound(OrganicVariant value) noexcept {
        return 2*OrganicModulator::maximumMagnitude(value);
    }
private:
    DelayBackend backend = DelayBackend::DigitalFractional;
    BBDVoiceConfig bbdConfig;
    bool prepared = false;
    std::array<std::array<BBDModulatedDelayVoice,4>,2> bbdDelays;
    double sampleRate = 48000; bool perceptualDepth = true;
    OrganicVariant organicVariant = OrganicVariant::Wander;
    BankMode bankMode = BankMode::Gentle;
    DynamicsMode dynamicsMode = DynamicsMode::Current;
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
