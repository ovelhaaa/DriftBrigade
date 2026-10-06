#include "DriftEngine.h"
#include <limits>
namespace drift {
namespace {
constexpr std::array<double, 4> independentRates {0.91, 1.037, 1.083, 0.967};
constexpr std::array<double, 4> stereoRates {1.113, 0.943, 1.057, 0.887};
constexpr double dynamicsChaos = 0.20, dynamicsFeedback = 0.08, quietWetReduction = 0.55;
} // namespace
void DriftEngine::setOrganicVariant(OrganicVariant value) noexcept {
    organicVariant=value; common.setVariant(value);
    for(auto& m: independent) m.setVariant(value);
    for(auto& m: stereo) m.setVariant(value);
}
void DriftEngine::setBankMode(BankMode value) noexcept {
    bankMode=value; for(auto& b:banks) b.setMode(value);
}
void DriftEngine::prepare(double sr, std::uint32_t seed) {
  prepared = false; // Failed prepare cannot leave a partially initialized
                    // engine active.
  sampleRate = bounded(sr, 44100, 192000);
    for (auto& c : controls) c.prepare(sampleRate);
    for (auto& b : banks) b.prepare(sampleRate);
    if (backend == DelayBackend::DigitalFractional) {
        for (auto& ch : delays) for (auto& d : ch) d.prepare(sampleRate);
    } else {
        for (auto& ch : bbdDelays) for (std::size_t b=0;b<4;++b) {
            auto c=bbdConfig; c.seed=bbdBandSeed(seed ^ bbdConfig.seed,b);
            ch[b].prepare(sampleRate,c);
        }
    }
    common.prepare(sampleRate);
    for (auto& m : independent) m.prepare(sampleRate);
    for (auto& m : stereo) m.prepare(sampleRate);
    envelope.prepare(sampleRate); envelopeControl.prepare(sampleRate);
    reset(seed);
    prepared = true;
}
void DriftEngine::reset(std::uint32_t seed) noexcept {
    for (std::size_t i=0; i<Count; ++i) controls[i].reset(targets.values[i]);
    for (auto& b : banks) b.reset();
    if (backend == DelayBackend::DigitalFractional) {
        for (auto& ch : delays) for (auto& d : ch) d.reset();
    } else {
        for (auto& ch : bbdDelays) for (std::size_t b=0;b<4;++b) ch[b].reset(bbdBandSeed(seed ^ bbdConfig.seed,b));
    }
    common.reset(seed);
    for (std::size_t b=0; b<4; ++b) { independent[b].reset(seed ^ (0x9e3779b9u*static_cast<std::uint32_t>(b+1))); stereo[b].reset(seed ^ (0x85ebca6bu*static_cast<std::uint32_t>(b+5))); }
  double initialCenter = targets[Center] * 0.001;
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  if (backend == DelayBackend::ExperimentalBBD && qualificationCenter > 0)
    initialCenter = qualificationCenter;
#endif
    for (auto &ch : currentDelays) {
        ch.fill(initialCenter);
    }
    if (backend == DelayBackend::ExperimentalBBD) for (auto& ch : currentDelays)
        for (auto& d:ch) d=bounded(d,minimumDelaySeconds(),maximumDelaySeconds());
    envelope.reset(); envelopeControl.reset(0); trace = {};
}
void DriftEngine::setParameters(const EngineParameters& p) noexcept {
    for (std::size_t i=0; i<Count; ++i) {
        const auto& spec = parameterSpecs[i];
        targets.values[i] = bounded(p.values[i], spec.minimum, spec.maximum);
        controls[i].setTarget(targets.values[i]);
    }
}
std::array<double, 2> DriftEngine::processSample(double left, double right, bool mono) noexcept {
  if (!prepared)
    return {0.0, 0.0};
  // Float-range finite inputs are kept in double for filter/control headroom.
  constexpr double maximumFloat = std::numeric_limits<float>::max();
  left = std::isfinite(left) ? bounded(left, -maximumFloat, maximumFloat) : 0.0;
  right =
      mono ? left
           : (std::isfinite(right) ? bounded(right, -maximumFloat, maximumFloat)
                                   : 0.0);
    std::array<double, Count> p {};
    for (std::size_t i=0; i<Count; ++i) p[i] = controls[i].next();
    trace.envelope = envelope.process(std::max(std::abs(left), std::abs(right)));
    envelopeControl.setTarget(trace.envelope/(trace.envelope+0.1));
    const double intensity = envelopeControl.next();
    trace.effectiveChaos = unit(p[Chaos]+dynamicsChaos*p[Dynamics]*intensity);
    trace.effectiveFeedback = std::min(0.75, p[Feedback]+dynamicsFeedback*p[Dynamics]*intensity);
    trace.wetProminence = 1-quietWetReduction*p[Dynamics]*(1-intensity);
    if (dynamicsMode == DynamicsMode::MotionOnly) trace.wetProminence = 1;
    trace.organic = common.process(p[Motion], trace.effectiveChaos);
    trace.randomControl = common.randomValue();
    trace.organicDiagnostics = common.diagnostics();
    const double independentWeight = std::sqrt(std::max(0.0, 1-p[Coherence]*p[Coherence]));
    const double stereoShared = 1-0.35*p[Width];
    const double stereoWeight = std::sqrt(std::max(0.0, 1-stereoShared*stereoShared));
    const double minimumDelay = minimumDelaySeconds();
    const double maximumDelay = std::min(.055,maximumDelaySeconds());
  double requestedCenter = p[Center] * 0.001;
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  if (backend == DelayBackend::ExperimentalBBD && qualificationCenter > 0)
    requestedCenter = qualificationCenter;
#endif
  const double center =
      backend == DelayBackend::DigitalFractional
          ? requestedCenter
          : bounded(requestedCenter,minimumDelay,maximumDelay);
    const double bound = combinedModulationBound(organicVariant);
    trace.requestedExcursionSeconds = depthSeconds(p[Motion], p[Depth], perceptualDepth);
    trace.actualExcursionSeconds = std::min(trace.requestedExcursionSeconds, (center-minimumDelay)*0.85/bound);
    if (backend == DelayBackend::ExperimentalBBD) {
        trace.actualExcursionSeconds = std::max(0.0,trace.actualExcursionSeconds);
#ifdef DRIFT_BBD_INSTRUMENT
        trace.physicalLimitSamples += center != p[Center]*.001 || trace.actualExcursionSeconds < trace.requestedExcursionSeconds;
#endif
    }
    const double excursion = trace.actualExcursionSeconds;
    const auto lb = banks[0].process(left), rb = banks[1].process(right);
    std::array<double, 2> wet {};
    for (std::size_t b=0; b<4; ++b) {
        const double separate = independent[b].process(p[Motion]*independentRates[b], trace.effectiveChaos);
        const double side = stereo[b].process(p[Motion]*stereoRates[b], trace.effectiveChaos);
        const double base = p[Coherence]*trace.organic + independentWeight*separate;
        trace.modulation[0][b] = base;
        trace.modulation[1][b] = mono ? base : stereoShared*base+stereoWeight*side;
        for (std::size_t ch=0; ch<2; ++ch) {
            const double target = bounded(center+excursion*trace.modulation[ch][b], minimumDelay, maximumDelay);
            // At most 1/4 sample per sample, even during large center automation.
            currentDelays[ch][b] += std::clamp(target-currentDelays[ch][b], -0.25/sampleRate, 0.25/sampleRate);
            trace.delaySeconds[ch][b] = currentDelays[ch][b];
            const double input = ch == 0 ? lb[b] : rb[b];
            const double output = backend == DelayBackend::DigitalFractional
                ? delays[ch][b].process(input,currentDelays[ch][b],trace.effectiveFeedback)
                : bbdDelays[ch][b].process(input,currentDelays[ch][b],trace.effectiveFeedback);
            wet[ch] += output;
#ifdef DRIFT_BBD_INSTRUMENT
            trace.bandInput[ch][b]=input; trace.bandWet[ch][b]=output;
            if (backend == DelayBackend::ExperimentalBBD) {
                const auto& core=bbdDelays[ch][b].signalPath().core;
                const auto& t=core.telemetry(); const auto& s=core.operatingStats;
                trace.totalBBDEvents+=t.eventsThisHostSample;
                trace.maximumBBDEventsPerSample=std::max(trace.maximumBBDEventsPerSample,t.eventsThisHostSample);
                trace.maximumBBDClock=std::max(trace.maximumBBDClock,t.effectiveClockHz);
                trace.minimumBBDClock=std::min(trace.minimumBBDClock,t.effectiveClockHz);
                trace.accumulatedBBDClock+=t.effectiveClockHz;
                trace.maximumBBDInternalPeak=std::max(trace.maximumBBDInternalPeak,s.peak);
                if(s.count) trace.minimumBBDNominalOccupancy=std::min(trace.minimumBBDNominalOccupancy,double(s.nominalCount)/s.count);
            }
#endif
        }
    }
  // BBD-only numerical tail hygiene; no global FTZ/DAZ or normal-value change.
  if (backend == DelayBackend::ExperimentalBBD) {
    for (auto &bank : banks)
      bank.clearSubnormalState();
    envelope.clearSubnormalState();
    envelopeControl.clearSubnormalState();
    for (auto &control : controls)
      control.clearSubnormalState();
  }
  return {(1-p[Mix])*left+p[Mix]*trace.wetProminence*wet[0], (1-p[Mix])*right+p[Mix]*trace.wetProminence*wet[1]};
}
void DriftEngine::process(float* const* channels, std::size_t channelCount, std::size_t samples) noexcept {
    if (channelCount == 0 || channelCount > 2) return;
    for (std::size_t n=0; n<samples; ++n) {
        const auto out = processSample(channels[0][n], channelCount == 2 ? channels[1][n] : channels[0][n], channelCount == 1);
        channels[0][n] = static_cast<float>(out[0]);
        if (channelCount == 2) channels[1][n] = static_cast<float>(out[1]);
    }
}
} // namespace drift
