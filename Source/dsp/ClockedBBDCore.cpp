#include "ClockedBBDCore.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace drift {
void ClockedBBDCore::prepare(double hostSampleRate,std::size_t stages) {
    if(stages<2 || stages>65536 || stages%2) throw std::invalid_argument("BBD physical stages must be even, in [2,65536]");
    trace.hostRateWasNormalized=!std::isfinite(hostSampleRate) || hostSampleRate<minimumHostSampleRate || hostSampleRate>maximumHostSampleRate;
    sampleRate=trace.hostRateWasNormalized?48000.0:hostSampleRate;
    storage.assign(stages/2,0.0);
#ifdef DRIFT_BBD_INSTRUMENT
    captureTimes.assign(storage.size(),-1.0);
#endif
    trace.stageCount=stages; trace.logicalSignalBuckets=storage.size();
    character.prepare(stages);
    cachedClock=-1; rebuildTransitions();
    reset(); setDelaySeconds(static_cast<double>(stages)/(2.0*sampleRate));
}
void ClockedBBDCore::setQualificationMode(BBDMode selected,BBDFilterProfile profile) noexcept {
    mode=selected;
    if(profile==BBDFilterProfile::HoltersParkerTable1) {
        inputFilter.paperReference(false); outputFilter.paperReference(true);
    } else { inputFilter.prototype(2000.0); outputFilter.prototype(2000.0); }
    rebuildTransitions();
}
void ClockedBBDCore::rebuildTransitions() noexcept {
    inputHost=inputFilter.makeTransition(1.0/sampleRate);
    outputHost=outputFilter.makeTransition(1.0/sampleRate);
    if(cachedClock>0 && mode==BBDMode::AsyncLinearReference) {
        inputPeriod=inputFilter.makeTransition(1.0/cachedClock);
        outputPeriod=outputFilter.makeTransition(1.0/cachedClock);
    }
}
void ClockedBBDCore::reset() noexcept {
    std::fill(storage.begin(),storage.end(),0.0);
    head=0; eventPhase=0.0; held=0.0; hostSamples=0; capturePhase=true;
    inputFilter.reset(); outputFilter.reset(); character.reset();
#ifdef DRIFT_BBD_INSTRUMENT
    operatingStats={};
    std::fill(captureTimes.begin(),captureTimes.end(),-1.0);
#endif
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
    if(cachedClock!=trace.effectiveClockHz) {
        cachedClock=trace.effectiveClockHz;
        if(mode==BBDMode::AsyncLinearReference) {
            inputPeriod=inputFilter.makeTransition(1.0/cachedClock);
            outputPeriod=outputFilter.makeTransition(1.0/cachedClock);
        }
        character.update(cachedClock);
    }
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
    double inputElapsed=0.0,outputElapsed=0.0;
    bool captured=false,outputUpdated=false;
    for(std::uint32_t i=0;i<events;++i) {
        const double instant=std::clamp((1.0-oldPhase+i)/increment,0.0,1.0)*dt;

        const double time=static_cast<double>(hostSamples)*dt+instant;
        if(capturePhase) {
            if(mode==BBDMode::AsyncLinearReference) {
                if(captured) { inputFilter.advanceWithTransition(inputPeriod); DRIFT_ASYNC_COUNT(periodApplications,1); }
                else inputFilter.advance(instant);
                captured=true; inputElapsed=instant;
            }
            double captureInput=mode==BBDMode::TransportOnly?input:inputFilter.value();
            if(domainInputGain!=1) captureInput*=domainInputGain;
            storage[head]=character.capture(captureInput);
#ifdef DRIFT_BBD_INSTRUMENT
            if(collectOperatingStats) {
                captureTimes[head]=time;
                const double x=storage[head],m=std::abs(x);
                ++operatingStats.count;
                operatingStats.nominalCount+=m<=nonlinearReference;
                const double normalized=m/nonlinearReference;
                ++operatingStats.bins[normalized<.01?0:normalized<.1?1:normalized<=1?2:normalized<=2?3:4];
                if(normalized>1) operatingStats.aboveNominalSeconds+=1/trace.effectiveClockHz;
                operatingStats.usefulCount+=m>=.1*nonlinearReference && m<=nonlinearReference;
                operatingStats.peak=std::max(operatingStats.peak,m);
                operatingStats.sumSquares+=x*x; operatingStats.sumMagnitude+=m;
                operatingStats.lastInput=x;
            }
#endif
            if(++head==storage.size()) head=0;
            ++trace.totalCaptureCount; ++trace.capturesThisHostSample;
            trace.lastCaptureTimeSeconds=time;
        } else {
            // Eq.1: sample captured at t_n exits at t_(n+N-1).
            // The combined output holds this value for the next two edges.
            if(mode==BBDMode::AsyncLinearReference) {
                if(outputUpdated) { outputFilter.advanceWithTransition(outputPeriod,held); DRIFT_ASYNC_COUNT(periodApplications,1); }
                else outputFilter.advance(instant,held);
                outputUpdated=true; outputElapsed=instant;
            }
            held=nonlinearReference==1 || !character.nonlinear.enabled()
                ?character.transfer(storage[head])
                :character.transferAfterNonlinear(character.nonlinear.process(storage[head]/nonlinearReference)*nonlinearReference);
#ifdef DRIFT_BBD_INSTRUMENT
            if(collectOperatingStats) {
                operatingStats.lastNonlinearInput=storage[head];
                if(captureTimes[head]>=0) {
                    const double residence=time-captureTimes[head];
                    ++operatingStats.transportedCount;
                    operatingStats.minimumBucketResidence=std::min(operatingStats.minimumBucketResidence,residence);
                    operatingStats.maximumBucketResidence=std::max(operatingStats.maximumBucketResidence,residence);
                }
                operatingStats.lastNonlinearOutput=nonlinearReference==1?character.nonlinear.process(storage[head])
                    :character.nonlinear.process(storage[head]/nonlinearReference)*nonlinearReference;
                operatingStats.nonlinearInputPeak=std::max(operatingStats.nonlinearInputPeak,std::abs(storage[head]));
                operatingStats.nonlinearOutputPeak=std::max(operatingStats.nonlinearOutputPeak,std::abs(operatingStats.lastNonlinearOutput));
            }
#endif
            ++trace.totalOutputCount; ++trace.outputsThisHostSample;
            trace.lastOutputTimeSeconds=time;
        }
        capturePhase=!capturePhase;
    }
    if(mode==BBDMode::AsyncLinearReference) {
        if(captured) inputFilter.advance(dt-inputElapsed);
        else inputFilter.advanceWithTransition(inputHost);
        if(outputUpdated) outputFilter.advance(dt-outputElapsed,held);
        else outputFilter.advanceWithTransition(outputHost,held);
    }
    ++hostSamples; trace.totalEventCount+=events;
    trace.accumulatedClockPhase=eventPhase; trace.nextEdgeCaptures=capturePhase;
    return mode==BBDMode::TransportOnly?(domainOutputGain==1?held:held*domainOutputGain)
        :(domainOutputGain==1?outputFilter.value():outputFilter.value()*domainOutputGain);
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
