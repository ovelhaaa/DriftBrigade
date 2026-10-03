#include "ClockedBBDCore.h"
#include <algorithm>
#include <cmath>

namespace drift {
void ClockedBBDCore::prepare(double hostSampleRate, std::size_t stages) {
    sampleRate = std::max(1.0, hostSampleRate);
    storage.assign(std::max<std::size_t>(1, stages), 0.0);
    trace.stageCount = storage.size();
    reset();
    setDelaySeconds(static_cast<double>(trace.stageCount) / (2.0 * sampleRate));
}
void ClockedBBDCore::reset() noexcept {
    std::fill(storage.begin(), storage.end(), 0.0);
    head = 0; eventPhase = 0.0; inputFilter.reset(); outputHold.reset();
    trace.eventsThisHostSample = 0; trace.totalEventCount = 0;
    trace.accumulatedClockPhase = 0.0;
}
void ClockedBBDCore::setDelaySeconds(double seconds) noexcept {
    // The upper clock bound is an explicit real-time architecture limit. The
    // lower 1 Hz bound prevents a stopped/denormal clock, not a musical limit.
    const double minimum = static_cast<double>(trace.stageCount)
                         / (sampleRate * maximumEventsPerHostSample);
    const double maximum = static_cast<double>(trace.stageCount) / 2.0;
    trace.requestedDelaySeconds = std::clamp(std::isfinite(seconds) ? seconds : maximum,
                                             minimum, maximum);
    trace.effectiveClockHz = static_cast<double>(trace.stageCount)
                           / (2.0 * trace.requestedDelaySeconds);
    trace.effectiveTransportDelaySeconds = static_cast<double>(trace.stageCount)
                                         / (2.0 * trace.effectiveClockHz);
}
double ClockedBBDCore::transfer(double input) noexcept {
    // head is the final/oldest logical stage. Retire it, replace it with the
    // new stage-0 sample, then rotate: exactly O(1), equivalent to a shift.
    const double retired = storage[head];
    storage[head] = input;
    if (++head == storage.size()) head = 0;
    return retired;
}
double ClockedBBDCore::process(double input) noexcept {
    input = std::clamp(std::isfinite(input) ? input : 0.0,
                       -static_cast<double>(std::numeric_limits<float>::max()),
                       static_cast<double>(std::numeric_limits<float>::max()));
    const double increment = (2.0 * trace.effectiveClockHz) / sampleRate;
    eventPhase += increment;
    const auto events = static_cast<std::uint32_t>(eventPhase);
    eventPhase -= static_cast<double>(events);
    double eventOutput = outputHold.value();
    const double conditioned = inputFilter.process(input);
    for (std::uint32_t i = 0; i < events; ++i) eventOutput = transfer(conditioned);
    trace.eventsThisHostSample = events;
    trace.totalEventCount += events;
    trace.accumulatedClockPhase = eventPhase;
    return outputHold.process(eventOutput, events != 0);
}
double ClockedBBDCore::stageValue(std::size_t logicalStage) const noexcept {
    if (logicalStage >= storage.size()) return 0.0;
    return storage[(head + storage.size() - 1 - logicalStage) % storage.size()];
}
bool ClockedBBDCore::finiteState() const noexcept {
    if (!std::isfinite(eventPhase) || !std::isfinite(outputHold.value())) return false;
    for (double value : storage) if (!std::isfinite(value)) return false;
    return true;
}
}
