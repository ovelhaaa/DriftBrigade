#pragma once
#include "DspMath.h"
#include "AsyncAnalogFilter.h"
#include "BBDDeviceCharacter.h"
#include <cstdint>
#include <limits>
#include <vector>
namespace drift_m24 {
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
    void setDelaySeconds(double seconds) noexcept;
    // Qualification only: configure before prepare/reset. Clock transition/cache
    // rebuilds preserve bucket memory, scheduler phase, filter state, held output,
    // character state and RNG state.
    void setQualificationMode(BBDMode mode,BBDFilterProfile profile=BBDFilterProfile::ValidationPrototype) noexcept;
    // Internal qualification configuration: set before prepare.
    void setCharacterConfig(BBDCharacterConfig config) noexcept { character.configure(config); }
    const BBDDeviceCharacter& deviceCharacter() const noexcept { return character; }
    double process(double input) noexcept;
    const BBDTelemetry& telemetry() const noexcept { return trace; }
    double stageValue(std::size_t logicalStage) const noexcept;
    double heldOutput() const noexcept { return held; }
    double inputFilterValue() const noexcept { return inputFilter.value(); }
    double outputFilterValue() const noexcept { return outputFilter.value(); }
    bool finiteState() const noexcept;
#ifdef DRIFT_BBD_INSTRUMENT
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
    void rebuildTransitions() noexcept;
    BBDDeviceCharacter character;
    BBDTelemetry trace;
};
}
