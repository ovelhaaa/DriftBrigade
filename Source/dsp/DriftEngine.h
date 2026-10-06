#pragma once
#include "BBDModulatedDelayVoice.h"
#include "DepthMapping.h"
#include "EnvelopeFollower.h"
#include "FractionalDelay.h"
#include "MultiscaleBank.h"
#include "OrganicModulator.h"
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
  bool isPrepared() const noexcept { return prepared; }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  // Reach the model boundary below the product Center range, before prepare.
  bool qualificationSetCenter(double seconds) noexcept {
    if (prepared || !std::isfinite(seconds) || seconds <= 0)
      return false;
    qualificationCenter = seconds;
    return true;
  }
  void qualificationInjectTail(double value) noexcept {
    for (auto &bank : banks)
      bank.qualificationInjectTail(value);
    envelope.qualificationInjectTail(value);
    envelopeControl.qualificationInjectTail(value);
    for (auto &ch : bbdDelays)
      for (auto &voice : ch)
        voice.qualificationInjectTail(value);
  }
  auto qualificationBankState(std::size_t ch) const noexcept {
    return banks[ch].qualificationState();
  }
  double qualificationEnvelopeState() const noexcept {
    return envelope.qualificationState();
  }
  double qualificationEnvelopeControlState() const noexcept {
    return envelopeControl.value();
  }
  void qualificationBeginCallback() noexcept {
#ifdef DRIFT_BBD_INSTRUMENT
    trace.maximumBBDEventsPerSample = 0;
    trace.maximumBBDClock = 0;
#endif
  }
  void qualificationInject(std::size_t ch, std::size_t band,
                           BBDQualificationFault fault, double value) noexcept {
    if (ch < 2 && band < 4)
      bbdDelays[ch][band].qualificationInject(fault, value);
  }
  void qualificationResetVoice(std::size_t ch, std::size_t band,
                               std::uint32_t seed) noexcept {
    if (ch < 2 && band < 4)
      bbdDelays[ch][band].reset(bbdBandSeed(seed ^ bbdConfig.seed, band));
  }
#ifdef DRIFT_BBD_INSTRUMENT
  void qualificationClearMeasurements() noexcept {
    for (auto &ch : bbdDelays)
      for (auto &v : ch)
        v.qualificationClearMeasurements();
  }
#endif
#endif
  DelayBackend delayBackend() const noexcept { return backend; }
    double minimumDelaySeconds() const noexcept { return backend == DelayBackend::DigitalFractional ? 4.0/sampleRate : double(bbdConfig.physicalStages)/(sampleRate*ClockedBBDCore::maximumEventsPerHostSample); }
    double maximumDelaySeconds() const noexcept { return backend == DelayBackend::DigitalFractional ? .055 : double(bbdConfig.physicalStages)/2; }
    const BBDModulatedDelayVoice& bbdVoice(std::size_t ch, std::size_t band) const noexcept { return bbdDelays[ch][band]; }
#ifdef DRIFT_BBD_INSTRUMENT
    void enableBBDOperatingInstrumentation(bool enabled) noexcept {
        for(auto& ch:bbdDelays) for(auto& voice:ch) voice.enableOperatingInstrumentation(enabled);
    }
#endif
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
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  double qualificationCenter = 0;
#endif
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
} // namespace drift
