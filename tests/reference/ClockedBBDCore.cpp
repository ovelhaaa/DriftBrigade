#include "ClockedBBDCore.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace drift_reference {
void ClockedBBDCore::prepare(double hostSampleRate,std::size_t stages) {
    if(stages<2 || stages>65536 || stages%2) throw std::invalid_argument("BBD physical stages must be even, in [2,65536]");
    trace.hostRateWasNormalized=!std::isfinite(hostSampleRate) || hostSampleRate<minimumHostSampleRate || hostSampleRate>maximumHostSampleRate;
    sampleRate=trace.hostRateWasNormalized?48000.0:hostSampleRate;
    storage.assign(stages/2,0.0);
    trace.stageCount=stages; trace.logicalSignalBuckets=storage.size();
    character.prepare(stages);
    reset(); setDelaySeconds(static_cast<double>(stages)/(2.0*sampleRate));
}
void ClockedBBDCore::setQualificationMode(BBDMode selected,BBDFilterProfile profile) noexcept {
    mode=selected;
    if(profile==BBDFilterProfile::HoltersParkerTable1) {
        inputFilter.paperReference(false); outputFilter.paperReference(true);
    } else { inputFilter.prototype(2000.0); outputFilter.prototype(2000.0); }
}
void ClockedBBDCore::reset() noexcept {
    std::fill(storage.begin(),storage.end(),0.0);
    head=0; eventPhase=0.0; held=0.0; hostSamples=0; capturePhase=true;
    inputFilter.reset(); outputFilter.reset(); character.reset();
    trace.eventsThisHostSample=trace.capturesThisHostSample=trace.outputsThisHostSample=0;
    trace.totalEventCount=trace.totalCaptureCount=trace.totalOutputCount=0;
    trace.lastCaptureTimeSeconds=trace.lastOutputTimeSeconds=0.0;
    trace.accumulatedClockPhase=0.0; trace.nextEdgeCaptures=true;
}
void ClockedBBDCore::setDelaySeconds(double seconds) noexcept {
    const double minimum=static_cast<double>(trace.stageCount)/(sampleRate*maximumEventsPerHostSample);
    const double maximum=static_cast<double>(trace.stageCount)/2.0;
    const bool valid=std::isfinite(seconds);
    trace.requestedDelaySeconds=valid?seconds:maximum;
    if(storage.empty()) return;
    trace.effectiveDelaySeconds=std::clamp(trace.requestedDelaySeconds,minimum,maximum);
    trace.wasClamped=!valid || trace.effectiveDelaySeconds!=trace.requestedDelaySeconds;
    trace.effectiveClockHz=static_cast<double>(trace.stageCount)/(2.0*trace.effectiveDelaySeconds);
    trace.signalSamplingRateHz=trace.effectiveClockHz;
    trace.signalNyquistHz=trace.effectiveClockHz/2.0;
    character.update(trace.effectiveClockHz);
}
double ClockedBBDCore::process(double input) noexcept {
    if(storage.empty()) return 0.0;
    input=std::clamp(std::isfinite(input)?input:0.0,-static_cast<double>(std::numeric_limits<float>::max()),static_cast<double>(std::numeric_limits<float>::max()));
    const double dt=1.0/sampleRate,increment=2.0*trace.effectiveClockHz/sampleRate;
    const double oldPhase=eventPhase;
    eventPhase+=increment;
    const auto events=static_cast<std::uint32_t>(eventPhase);
    eventPhase-=events;
    trace.eventsThisHostSample=events; trace.capturesThisHostSample=trace.outputsThisHostSample=0;
    // Input samples are weighted Dirac impulses at interval start (Eq.4).
    // Output is observed at interval end; events on the boundary run first.
    if(mode==BBDMode::AsyncLinearReference) inputFilter.injectImpulse(input*dt);
    double elapsed=0.0;
    for(std::uint32_t i=0;i<events;++i) {
        const double instant=std::clamp((1.0-oldPhase+i)/increment,0.0,1.0)*dt;
        if(mode==BBDMode::AsyncLinearReference) {
            inputFilter.advance(instant-elapsed); outputFilter.advance(instant-elapsed,held);
        }
        elapsed=instant;
        const double time=static_cast<double>(hostSamples)*dt+instant;
        if(capturePhase) {
            storage[head]=character.capture(mode==BBDMode::TransportOnly?input:inputFilter.value());
            if(++head==storage.size()) head=0;
            ++trace.totalCaptureCount; ++trace.capturesThisHostSample;
            trace.lastCaptureTimeSeconds=time;
        } else {
            // Eq.1: sample captured at t_n exits at t_(n+N-1).
            // The combined output holds this value for the next two edges.
            held=character.transfer(storage[head]);
            ++trace.totalOutputCount; ++trace.outputsThisHostSample;
            trace.lastOutputTimeSeconds=time;
        }
        capturePhase=!capturePhase;
    }
    if(mode==BBDMode::AsyncLinearReference) {
        inputFilter.advance(dt-elapsed); outputFilter.advance(dt-elapsed,held);
    }
    ++hostSamples; trace.totalEventCount+=events;
    trace.accumulatedClockPhase=eventPhase; trace.nextEdgeCaptures=capturePhase;
    return mode==BBDMode::TransportOnly?held:outputFilter.value();
}
double ClockedBBDCore::stageValue(std::size_t logicalStage) const noexcept {
    if(logicalStage>=storage.size()) return 0.0;
    return storage[(head+storage.size()-1-logicalStage)%storage.size()];
}
bool ClockedBBDCore::finiteState() const noexcept {
    if(!std::isfinite(eventPhase) || !std::isfinite(held) || !std::isfinite(trace.effectiveClockHz)
       || !std::isfinite(trace.effectiveDelaySeconds) || !inputFilter.finiteState() || !outputFilter.finiteState() || !character.finiteState()) return false;
    for(double value:storage) if(!std::isfinite(value)) return false;
    return true;
}
}

