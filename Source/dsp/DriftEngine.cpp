#include "DriftEngine.h"
#include <limits>
namespace drift {
namespace {
constexpr std::array<double, 4> independentRates {0.91, 1.037, 1.083, 0.967};
constexpr std::array<double, 4> stereoRates {1.113, 0.943, 1.057, 0.887};
constexpr double dynamicsChaos = 0.20, dynamicsFeedback = 0.08, quietWetReduction = 0.55;
}
void DriftEngine::prepare(double sr, std::uint32_t seed) {
    sampleRate = bounded(sr, 44100, 192000);
    for (auto& c : controls) c.prepare(sampleRate);
    for (auto& b : banks) b.prepare(sampleRate);
    for (auto& ch : delays) for (auto& d : ch) d.prepare(sampleRate);
    common.prepare(sampleRate);
    for (auto& m : independent) m.prepare(sampleRate);
    for (auto& m : stereo) m.prepare(sampleRate);
    envelope.prepare(sampleRate); envelopeControl.prepare(sampleRate);
    reset(seed);
}
void DriftEngine::reset(std::uint32_t seed) noexcept {
    for (std::size_t i=0; i<Count; ++i) controls[i].reset(targets.values[i]);
    for (auto& b : banks) b.reset();
    for (auto& ch : delays) for (auto& d : ch) d.reset();
    common.reset(seed);
    for (std::size_t b=0; b<4; ++b) { independent[b].reset(seed ^ (0x9e3779b9u*static_cast<std::uint32_t>(b+1))); stereo[b].reset(seed ^ (0x85ebca6bu*static_cast<std::uint32_t>(b+5))); }
    for (auto& ch : currentDelays) ch.fill(targets[Center]*0.001);
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
    // Float-range finite inputs are kept in double for filter/control headroom.
    constexpr double maximumFloat = std::numeric_limits<float>::max();
    left = bounded(left, -maximumFloat, maximumFloat);
    right = mono ? left : bounded(right, -maximumFloat, maximumFloat);
    std::array<double, Count> p {};
    for (std::size_t i=0; i<Count; ++i) p[i] = controls[i].next();
    trace.envelope = envelope.process(std::max(std::abs(left), std::abs(right)));
    envelopeControl.setTarget(trace.envelope/(trace.envelope+0.1));
    const double intensity = envelopeControl.next();
    trace.effectiveChaos = unit(p[Chaos]+dynamicsChaos*p[Dynamics]*intensity);
    trace.effectiveFeedback = std::min(0.75, p[Feedback]+dynamicsFeedback*p[Dynamics]*intensity);
    trace.wetProminence = 1-quietWetReduction*p[Dynamics]*(1-intensity);
    trace.organic = common.process(p[Motion], trace.effectiveChaos);
    trace.randomControl = common.randomValue();
    const double independentWeight = std::sqrt(std::max(0.0, 1-p[Coherence]*p[Coherence]));
    const double stereoShared = 1-0.35*p[Width];
    const double stereoWeight = std::sqrt(std::max(0.0, 1-stereoShared*stereoShared));
    const double center = p[Center]*0.001;
    const double minimumDelay = 4.0/sampleRate;
    const double excursion = std::min(depthSeconds(p[Motion], p[Depth], perceptualDepth), (center-minimumDelay)*0.85/modulationBound);
    const auto lb = banks[0].process(left), rb = banks[1].process(right);
    std::array<double, 2> wet {};
    for (std::size_t b=0; b<4; ++b) {
        const double separate = independent[b].process(p[Motion]*independentRates[b], trace.effectiveChaos);
        const double side = stereo[b].process(p[Motion]*stereoRates[b], trace.effectiveChaos);
        const double base = p[Coherence]*trace.organic + independentWeight*separate;
        trace.modulation[0][b] = base;
        trace.modulation[1][b] = mono ? base : stereoShared*base+stereoWeight*side;
        for (std::size_t ch=0; ch<2; ++ch) {
            const double target = bounded(center+excursion*trace.modulation[ch][b], minimumDelay, 0.055);
            // At most 1/4 sample per sample, even during large center automation.
            currentDelays[ch][b] += std::clamp(target-currentDelays[ch][b], -0.25/sampleRate, 0.25/sampleRate);
            trace.delaySeconds[ch][b] = currentDelays[ch][b];
            wet[ch] += delays[ch][b].process(ch == 0 ? lb[b] : rb[b], currentDelays[ch][b], trace.effectiveFeedback);
        }
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
}
