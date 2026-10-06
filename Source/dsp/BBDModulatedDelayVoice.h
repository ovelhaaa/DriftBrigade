#pragma once
#include "BBDCompander.h"
namespace drift {
enum class DelayBackend { DigitalFractional, ExperimentalBBD };
enum class BBDFeedbackTopology { ExternalWetReturn };
#ifdef DRIFT_BBD_REALTIME_QUALIFY
enum class BBDQualificationFault {
  Feedback,
  Held,
  Bucket,
  CompressorDetector,
  ExpanderDetector
};
#endif
// QUALIFICATION FIXTURE / ENGINEERING NORMALIZATION / NOT PRODUCT DEFAULT.
struct BBDVoiceConfig {
  std::size_t physicalStages = 1024;
  BBDFilterProfile filterProfile = BBDFilterProfile::HoltersParkerTable1;
  BBDCharacterConfig character;
  BBDCompanderConfig compander;
  BBDGainStagingConfig gainStaging;
  BBDFeedbackTopology feedbackTopology = BBDFeedbackTopology::ExternalWetReturn;
  std::uint32_t seed = 570;
  static BBDVoiceConfig fullResearchFixture() noexcept {
    BBDVoiceConfig c;
    c.compander.enabled = true;
    c.character = BBDCharacterConfig::syntheticQualificationFixture();
    c.character.nonlinear.strength = 1;
    c.character.nonlinear.mode = BBDNonlinearityMode::EngineeringPolynomial;
    c.gainStaging = BBDGainStagingConfig::profile(BBDHeadroomProfile::Nominal);
    return c;
  }
  static BBDVoiceConfig linearReference() noexcept { return {}; }
};
inline std::uint32_t bbdBandSeed(std::uint32_t base,
                                 std::size_t band) noexcept {
  std::uint32_t x =
      base ^ (0x424244u + 0x9e3779b9u * static_cast<std::uint32_t>(band + 1));
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x ? x : 1;
}
class BBDModulatedDelayVoice {
public:
#ifdef DRIFT_BBD_INSTRUMENT
  // The core owns collection configuration; reset only clears measurements.
  BBDModulatedDelayVoice() noexcept { path.core.collectOperatingStats = true; }
#endif
  void prepare(double sr, const BBDVoiceConfig &c) {
    config = c;
    rate = sr;
    path.configure(c.compander);
    path.setGainStaging(c.gainStaging);
    path.core.setQualificationMode(BBDMode::AsyncLinearReference,
                                   c.filterProfile);
    auto character = c.character;
    character.seed = c.seed;
    path.core.setCharacterConfig(character);
    path.prepare(sr, c.physicalStages);
    rate = path.core.telemetry().hostRateWasNormalized ? 48000 : sr;
    pole = std::exp(-2 * pi * 5 / rate);
    reset(c.seed);
  }
  void reset(std::uint32_t seed) noexcept {
    path.core.reseedCharacter(seed);
    path.reset();
    previousWet = previousInput = dcState = 0;
#ifdef DRIFT_BBD_INSTRUMENT
    hiddenClamps = numericalGuards = 0;
#endif
  }
  double process(double input, double seconds, double feedback) noexcept {
    path.setDelaySeconds(
        seconds); // Engine already limits delay; no extra slew.
#ifdef DRIFT_BBD_INSTRUMENT
    hiddenClamps += path.core.telemetry().wasClamped;
#endif
    double wet = path.process(input + bounded(feedback, 0, .75) * previousWet);
    if (!std::isfinite(wet)) {
      wet = 0;
#ifdef DRIFT_BBD_INSTRUMENT
      ++numericalGuards;
#endif
    }
    path.core.clearSubnormalFilterState();
    previousWet =
        wet; // Expanded/reconstructed return BEFORE output DC blocker.
    dcState = flushSubnormal(wet - previousInput + pole * dcState);
    previousInput = wet;
    return dcState;
  }
  double minimumDelaySeconds() const noexcept {
    return double(config.physicalStages) /
           (rate * ClockedBBDCore::maximumEventsPerHostSample);
  }
  double maximumDelaySeconds() const noexcept {
    return double(config.physicalStages) / 2;
  }
  const BBDFullPath &signalPath() const noexcept { return path; }
  double feedbackWet() const noexcept { return previousWet; }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  void qualificationInject(BBDQualificationFault fault, double value) noexcept {
    switch (fault) {
    case BBDQualificationFault::Feedback:
      previousWet = value;
      break;
    case BBDQualificationFault::Held:
      path.core.qualificationInject(false, value);
      break;
    case BBDQualificationFault::Bucket:
      path.core.qualificationInject(true, value);
      break;
    case BBDQualificationFault::CompressorDetector:
      path.compressor.qualificationInjectDetector(value);
      break;
    case BBDQualificationFault::ExpanderDetector:
      path.expander.qualificationInjectDetector(value);
      break;
    }
  }
  void qualificationInjectTail(double value) noexcept {
    path.core.qualificationInjectTail(value);
    dcState = value;
  }
  double qualificationDCState() const noexcept { return dcState; }
#ifdef DRIFT_BBD_INSTRUMENT
  void qualificationClearMeasurements() noexcept {
    hiddenClamps = numericalGuards = 0;
    path.core.operatingStats = {};
    std::fill(path.core.captureTimes.begin(), path.core.captureTimes.end(),
              -1.0);
  }
#endif
#endif
  bool finiteState() const noexcept {
    return path.core.finiteState() && std::isfinite(previousWet) &&
           std::isfinite(dcState) &&
           std::isfinite(path.compressor.currentGain()) &&
           std::isfinite(path.expander.currentGain());
  }
#ifdef DRIFT_BBD_INSTRUMENT
  void enableOperatingInstrumentation(bool enabled) noexcept { path.core.collectOperatingStats=enabled; }
  std::uint64_t hiddenClamps = 0, numericalGuards = 0;
#endif
private:
  BBDFullPath path;
  BBDVoiceConfig config;
  double rate = 48000, pole = 0, previousWet = 0, previousInput = 0,
         dcState = 0;
};
} // namespace drift
