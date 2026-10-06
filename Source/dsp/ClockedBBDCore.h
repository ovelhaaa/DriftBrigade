#pragma once
#include "AsyncAnalogFilter.h"
#include "BBDDeviceCharacter.h"
#include "BBDGainStaging.h"
#include "DspMath.h"
#include <cstdint>
#include <limits>
#include <vector>
namespace drift {
enum class BBDMode { TransportOnly, AsyncLinearReference };
enum class BBDFilterProfile { ValidationPrototype, HoltersParkerTable1 };
struct BBDTelemetry {
    double requestedDelaySeconds=0.0,effectiveDelaySeconds=0.0,effectiveClockHz=0.0;
    double signalSamplingRateHz=0.0,signalNyquistHz=0.0;
    std::size_t stageCount=0,logicalSignalBuckets=0;
    double accumulatedClockPhase=0.0;
    std::uint32_t eventsThisHostSample=0,capturesThisHostSample=0,outputsThisHostSample=0;
    std::uint64_t totalEventCount=0,totalCaptureCount=0,totalOutputCount=0;
    double lastCaptureTimeSeconds=0.0,lastOutputTimeSeconds=0.0;
    bool nextEdgeCaptures=true,wasClamped=false,hostRateWasNormalized=false;
};
class ClockedBBDCore {
public:
    static constexpr std::uint32_t maximumEventsPerHostSample=128;
    static constexpr double minimumHostSampleRate=8000.0,maximumHostSampleRate=384000.0;
    void prepare(double hostSampleRate,std::size_t stages);
    void reset() noexcept;
    void reseedCharacter(std::uint32_t seed) noexcept { character.reseed(seed); }
    void setDelaySeconds(double seconds) noexcept;
    // Qualification only: configure before prepare/reset. Clock transition/cache
  // rebuilds preserve bucket memory, scheduler phase, filter state, held
  // output, character state and RNG state.
  void setQualificationMode(BBDMode mode,BBDFilterProfile profile=BBDFilterProfile::ValidationPrototype) noexcept;
    // Internal qualification configuration: set before prepare.
    void setCharacterConfig(BBDCharacterConfig config) noexcept { character.configure(config); }
    void setOperatingDomain(double inputGain=1, double outputGain=1, double reference=1) noexcept {
        BBDGainStagingConfig c; c.compressorToBBDGain=inputGain; c.bbdToExpanderGain=outputGain; c.nonlinearReferenceLevel=reference;
        c=BBDGainStagingConfig::validated(c); domainInputGain=c.compressorToBBDGain; domainOutputGain=c.bbdToExpanderGain; nonlinearReference=c.nonlinearReferenceLevel;
    }
    const BBDDeviceCharacter& deviceCharacter() const noexcept { return character; }
    double process(double input) noexcept;
    const BBDTelemetry& telemetry() const noexcept { return trace; }
    double stageValue(std::size_t logicalStage) const noexcept;
    double heldOutput() const noexcept { return held; }
    double inputFilterValue() const noexcept { return inputFilter.value(); }
    double outputFilterValue() const noexcept { return outputFilter.value(); }
    bool finiteState() const noexcept;
  void clearSubnormalFilterState() noexcept {
    inputFilter.clearSubnormalState();
    outputFilter.clearSubnormalState();
  }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  // Fault injection exists only in the M2.8 qualification executables.
  void qualificationInjectTail(double value) noexcept {
    inputFilter.qualificationInjectTail(value);
    outputFilter.qualificationInjectTail(value);
  }
  void qualificationInject(bool bucket, double value) noexcept {
    if (bucket) {
      if (!storage.empty())
        storage[(head + 1) % storage.size()] = value;
    } else
      held = value;
  }
#endif
#ifdef DRIFT_BBD_INSTRUMENT
    struct InputOperatingStats {
        std::uint64_t count=0, nominalCount=0, usefulCount=0;
        std::uint64_t bins[5]={};
        double aboveNominalSeconds=0, nonlinearInputPeak=0, nonlinearOutputPeak=0;
        std::uint64_t transportedCount=0;
        double minimumBucketResidence=std::numeric_limits<double>::max(), maximumBucketResidence=0;
        double peak=0, sumSquares=0, sumMagnitude=0, lastInput=0, lastNonlinearInput=0, lastNonlinearOutput=0;
    };
    InputOperatingStats operatingStats;
    bool collectOperatingStats=false;
    std::vector<double> captureTimes; // Measurement only; fixed in prepare.
    auto inputFilterState() const noexcept { return inputFilter.stateSnapshot(); }
    auto outputFilterState() const noexcept { return outputFilter.stateSnapshot(); }
#endif
private:
    std::vector<double> storage;
    std::size_t head=0;
    double sampleRate=48000.0,eventPhase=0.0,held=0.0;
    std::uint64_t hostSamples=0;
    bool capturePhase=true;
    BBDMode mode=BBDMode::TransportOnly;
    AsyncAnalogFilter inputFilter,outputFilter;
    AsyncAnalogTransition inputHost,outputHost,inputPeriod,outputPeriod;
    double cachedClock=-1;
    double domainInputGain=1,domainOutputGain=1,nonlinearReference=1;
    void rebuildTransitions() noexcept;
    BBDDeviceCharacter character;
    BBDTelemetry trace;
};
} // namespace drift
