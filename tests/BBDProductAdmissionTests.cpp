#include "AllocationTracker.h"
#include "dsp/DriftEngine.h"
#include <iostream>
#include <stdexcept>

using namespace drift;
namespace {
using Status = BBDAdmissionStatus;
using Outcome = BBDPreparationOutcome;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

BBDAdmissionRequest fixed(EngineParameters parameters) {
    BBDAdmissionRequest request;
    request.initial = request.automation.minimum = request.automation.maximum = parameters;
    return request;
}

BBDAdmissionRequest nominalScene() {
    EngineParameters parameters;
    parameters[Center] = 12;
    return fixed(parameters);
}

void rateMatrix() {
    DriftEngine engine;
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000., 32000., 50000.}) {
        const auto result = engine.prepareProduct(rate, nominalScene());
        const bool qualified = rate == 44100 || rate == 48000 || rate == 88200;
        check(result.requestedBackend == DelayBackend::ExperimentalBBD &&
              result.sampleRate == rate && engine.isPrepared(), "rate preparation");
        if (qualified) {
            check(result.status == Status::Admitted && result.outcome == Outcome::BBD,
                  "qualified rate");
            check(result.maximumClockHz == std::min(384000., 8 * rate) &&
                  result.minimumDelaySeconds == 1024 / (2 * result.maximumClockHz) &&
                  engine.minimumDelaySeconds() == result.minimumDelaySeconds, "rate limits");
        } else {
            check(result.status == Status::UnsupportedSampleRate &&
                  result.outcome == Outcome::FallbackToDigital &&
                  result.preparedBackend == DelayBackend::DigitalFractional,
                  "unsupported rate fallback");
        }
        check(engine.delayBackend() == result.preparedBackend, "active backend");
    }
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    for (double rate : {0., -48000., nan, inf}) {
        const auto result = engine.prepareProduct(rate, {});
        check(result.status == Status::InvalidConfiguration && result.outcome == Outcome::Unavailable &&
              !engine.isPrepared() && engine.delayBackend() == DelayBackend::Unavailable, "invalid rate");
    }
}

void physicalLimits() {
    DriftEngine engine;
    for (double rate : {44100., 48000., 88200.}) {
        EngineParameters p;
        const double floor = 1024 / (2 * std::min(384000., 8 * rate));
        p[Depth] = 0;
        p[Center] = floor * 1000;
        const auto result = engine.prepareProduct(rate, fixed(p));
        check(result.status == Status::Admitted, "center exactly at floor");
        for (int n = 0; n < 300; ++n)
            engine.processSample(n == 0 ? 1 : 0, 0);
        check(engine.telemetry().maximumBBDClock == result.maximumClockHz &&
              engine.telemetry().maximumBBDEventsPerSample <= 16 &&
              engine.telemetry().physicalLimitSamples == 0, "clock at the floor");
        for (std::size_t ch = 0; ch < 2; ++ch)
            for (std::size_t band = 0; band < 4; ++band) {
                const auto& voice = engine.bbdVoice(ch, band);
                const auto& core = voice.signalPath().core;
                check(core.telemetry().stageCount == 1024 && voice.hiddenClamps == 0 &&
                      core.minimumDelaySeconds() == floor, "all eight voices configured");
            }
        p[Center] -= .000001;
        check(engine.prepareProduct(rate, fixed(p)).status == Status::CenterBelowFloor,
              "below floor");
        p[Center] += .100001;
        check(engine.prepareProduct(rate, fixed(p)).status == Status::Admitted, "above floor");
        p[Motion] = 1;
        const double maxExcursion = engine.bbdAdmission().maximumExcursionSeconds;
        p[Depth] = .999 * maxExcursion / depthSeconds(p[Motion], .25) * .25;
        check(evaluateBBDAdmission(rate, fixed(p)).status == Status::Admitted, "near depth limit");
        p[Depth] *= 1.01;
        check(evaluateBBDAdmission(rate, fixed(p)).status == Status::ExcursionOutOfRange,
              "excessive excursion");
    }
    auto request = nominalScene();
    request.clockCeilingHz = 384001;
    check(evaluateBBDAdmission(48000, request).status == Status::ClockLimitExceeded, "excessive clock");
    request.clockCeilingHz = 300000;
    const auto result = engine.prepareProduct(48000, request);
    check(result.status == Status::Admitted && result.maximumClockHz == 300000 &&
          engine.minimumDelaySeconds() == 1024. / 600000., "tightened clock");

    ClockedBBDCore core;
    core.prepare(48000, 1024, 384000);
    core.setDelaySeconds(std::nextafter(1024. / 48000., 1.));
    core.process(0);
    check(core.telemetry().accumulatedClockPhase > .999999999999999, "near-integer scheduler phase");
    core.setDelaySeconds(0);
    core.process(0);
    check(core.telemetry().wasClamped && core.telemetry().effectiveClockHz == 384000 &&
          core.telemetry().eventsThisHostSample == 16 &&
          core.telemetry().accumulatedClockPhase < 1, "scheduler rounding at ceiling");
}

void automationAndReset() {
    BBDAdmissionRequest request;
    auto& lo = request.automation.minimum;
    auto& hi = request.automation.maximum;
    lo[Motion] = .1; hi[Motion] = 10;
    lo[Depth] = 0; hi[Depth] = .85;
    lo[Center] = 22; hi[Center] = 30;
    for (auto control : {Chaos, Coherence, Dynamics, Width, Feedback}) {
        lo[control] = parameterSpecs[control].minimum;
        hi[control] = parameterSpecs[control].maximum;
    }
    request.initial = lo;
    auto incompatible = request;
    incompatible.automation.minimum[Center] = 2;
    check(evaluateBBDAdmission(48000, incompatible).status == Status::ExcursionOutOfRange,
          "unsafe combination inside box with valid initial parameters");

    DriftEngine engine, reference;
    check(engine.prepareProduct(48000, request, 77).status == Status::Admitted &&
          reference.prepareProduct(48000, request, 77).status == Status::Admitted, "automation box");
    engine.setOrganicVariant(OrganicVariant::PhaseDrift);
    engine.setBankMode(BankMode::Selective);
    engine.usePerceptualDepth(false);
    const auto before = allocations.load();
    bool valid = true;
    for (int n = 0; n < 48000; ++n) {
        if (n % 211 == 0) {
            auto p = (n / 211) % 2 ? hi : lo;
            engine.setParameters(p);
            reference.setParameters(p);
            valid &= engine.bbdAutomationStatus() == BBDAutomationStatus::Accepted;
            p[Center] = 20;
            engine.setParameters(p);
            valid &= engine.bbdAutomationStatus() == BBDAutomationStatus::OutsideEnvelope;
            p[Depth] = n % 2 ? std::numeric_limits<double>::infinity()
                             : std::numeric_limits<double>::quiet_NaN();
            engine.setParameters(p);
            valid &= engine.bbdAutomationStatus() == BBDAutomationStatus::InvalidParameters;
        }
        const double x = n < 9000 && n % 157 == 0 ? 2.0 : 0.0;
        const auto output = engine.processSample(x, -x, n % 211 < 51);
        valid &= std::isfinite(output[0]) && std::isfinite(output[1]) &&
                 output == reference.processSample(x, -x, n % 211 < 51);
        const auto& trace = engine.telemetry();
        valid &= trace.actualExcursionSeconds == trace.requestedExcursionSeconds &&
                 trace.effectiveFeedback <= .75;
    }
    check(allocations.load() == before, "allocation in automation or processing");
    check(valid, "rejected updates changed output or admitted output is invalid");
    check(engine.telemetry().physicalLimitSamples == 0 &&
          engine.telemetry().maximumBBDEventsPerSample <= 16 &&
          engine.telemetry().maximumBBDClock <= engine.bbdAdmission().maximumClockHz,
          "physical limits during automation");
    for (std::size_t ch = 0; ch < 2; ++ch)
        for (std::size_t band = 0; band < 4; ++band) {
            const auto& voice = engine.bbdVoice(ch, band);
            check(voice.hiddenClamps == 0 && voice.numericalGuards == 0 && voice.finiteState(),
                  "voice guards during automation");
        }
    engine.setParameters(hi);
    engine.reset(77);
    auto fresh = request;
    fresh.initial = hi;
    reference.prepareProduct(48000, fresh, 77);
    for (int n = 0; n < 2048; ++n) {
        const double x = n == 0 ? 1.0 : 0.0;
        check(engine.processSample(x, x) == reference.processSample(x, x), "deterministic reset");
    }

    std::array<float, 257> mono {};
    mono[0] = 1;
    auto* channel = mono.data();
    const auto beforeBlocks = allocations.load();
    engine.process(&channel, 1, 1);
    ++channel;
    engine.process(&channel, 1, 256);
    check(allocations.load() == beforeBlocks, "allocation in block callback");
    for (std::size_t n = 0; n < mono.size(); ++n) {
        const double x = n == 0 ? 1 : 0;
        check(mono[n] == static_cast<float>(reference.processSample(x, x, true)[0]),
              "block segmentation or mono routing");
    }
}

void fallbackAndRecovery() {
    DriftEngine engine, digital;
    BBDAdmissionRequest request;
    request.profile = static_cast<BBDProductProfile>(123);
    check(engine.prepareProduct(48000, request).status == Status::UnsupportedProfile, "profile");
    request = {};
    request.physicalStages = 512;
    check(engine.prepareProduct(48000, request).status == Status::UnsupportedProfile, "stage profile");
    request.physicalStages = 1023;
    check(engine.prepareProduct(48000, request).status == Status::InvalidConfiguration, "invalid stages");
    request = {};
    request.initial[Depth] = std::numeric_limits<double>::quiet_NaN();
    const auto rejected = engine.prepareProduct(48000, request);
    check(rejected.status == Status::InvalidConfiguration &&
          rejected.outcome == Outcome::FallbackToDigital, "invalid parameter fallback");

    engine.qualificationFailPrepare(true, false);
    const auto fallback = engine.prepareProduct(48000, nominalScene(), 42);
    check(fallback.status == Status::PreparationFailed && fallback.outcome == Outcome::FallbackToDigital,
          "partial BBD prepare fallback");
    digital.setParameters(fallback.request.initial);
    digital.prepare(48000, 42);
    for (int pass = 0; pass < 2; ++pass) {
        for (int n = 0; n < 4096; ++n) {
            const double x = n == 0 ? 1.0 : 0.0;
            check(engine.processSample(x, -x) == digital.processSample(x, -x), "fallback Digital identity");
        }
        engine.reset(42);
        digital.reset(42);
    }
    engine.qualificationFailPrepare(true, true);
    const auto failed = engine.prepareProduct(48000, nominalScene());
    check(failed.status == Status::PreparationFailed && failed.outcome == Outcome::DigitalPreparationFailed &&
          !engine.isPrepared() && engine.delayBackend() == DelayBackend::Unavailable, "both prepares failed");
    engine.reset();
    check(engine.processSample(1, 1) == std::array<double, 2>{}, "unavailable engine output");
    engine.qualificationFailPrepare(false, false);
    check(engine.prepareProduct(88200, engine.bbdAdmission().request).outcome == Outcome::BBD,
          "reprepare after failed fallback");
    engine.prepare(48000);
    check(engine.delayBackend() == DelayBackend::DigitalFractional &&
          engine.bbdAdmission().status == Status::NotRequested, "legacy prepare after product");
}
} // namespace

int main() {
    try {
        rateMatrix();
        physicalLimits();
        automationAndReset();
        fallbackAndRecovery();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
