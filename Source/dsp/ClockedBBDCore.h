#pragma once
#include "DspMath.h"
#include <cstdint>
#include <limits>
#include <vector>

namespace drift {

// Telemetry is a snapshot only; reading it never changes scheduler state.
struct BBDTelemetry {
    double requestedDelaySeconds = 0.0;
    double effectiveClockHz = 0.0;
    std::size_t stageCount = 0;
    double accumulatedClockPhase = 0.0; // phase of the next transfer event [0, 1)
    std::uint32_t eventsThisHostSample = 0;
    std::uint64_t totalEventCount = 0;
    double effectiveTransportDelaySeconds = 0.0;
};

// Temporary M2.0 boundaries. They deliberately do no spectral shaping.
struct BBDInputFilter {
    double process(double value) noexcept { return value; }
    void reset() noexcept {}
};
struct BBDOutputHold {
    double process(double eventOutput, bool hadEvent) noexcept {
        if (hadEvent) held = eventOutput;
        return held;
    }
    void reset() noexcept { held = 0.0; }
    double value() const noexcept { return held; }
private:
    double held = 0.0;
};

// A fixed-stage transport driven by transfer events. One physical clock cycle
// has two non-overlapping phases, so its abstract transfer-event rate is 2*fclock.
class ClockedBBDCore {
public:
    static constexpr std::uint32_t maximumEventsPerHostSample = 128;
    void prepare(double hostSampleRate, std::size_t stages);
    void reset() noexcept;
    void setDelaySeconds(double seconds) noexcept;
    double process(double input) noexcept;
    const BBDTelemetry& telemetry() const noexcept { return trace; }
    double stageValue(std::size_t logicalStage) const noexcept;
    bool finiteState() const noexcept;
private:
    double transfer(double input) noexcept;
    std::vector<double> storage;
    std::size_t head = 0;
    double sampleRate = 48000.0;
    double eventPhase = 0.0;
    BBDInputFilter inputFilter;
    BBDOutputHold outputHold;
    BBDTelemetry trace;
};
}
