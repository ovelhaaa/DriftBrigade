#pragma once
#include "ClockedBBDCore.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace drift {
// Internal qualification only. No production routing or topology selection.
enum class BBDCompanderTopology { OutsideFilters };
struct BBDCompanderConfig {
  bool enabled = false;
  double rectifierCapFarads = .47e-6;
  double detectorResistanceOhms = 10000;
  BBDCompanderTopology topology = BBDCompanderTopology::OutsideFilters;
  double numericalFloor = 0x1p-500;
};

class CompanderLevelAverager {
public:
  static constexpr double minimumFloor = 0x1p-500;
  void configure(BBDCompanderConfig c) noexcept {
    auto valid = [](double v, double fallback, double lo, double hi) {
      return std::isfinite(v) && v >= lo && v <= hi ? v : fallback;
    };
    const double capacitance = valid(c.rectifierCapFarads, .47e-6, 1e-12, 1e-3);
    const double resistance = valid(c.detectorResistanceOhms, 10000, 1, 1e6);
    tau = resistance * capacitance;
    floor = valid(c.numericalFloor, minimumFloor, minimumFloor, 1);
  }
  // Unity startup is a deliberate engineering initial condition, not an IC
  // claim.
  void reset(double initial = 1) noexcept {
    level = std::isfinite(initial) ? std::max(floor, std::abs(initial)) : 1;
    level = std::min(level, double(std::numeric_limits<float>::max()));
  }
  double alpha(double dt) const noexcept {
    return std::isfinite(dt) && dt > 0 ? dt / (tau + dt) : 0;
  }
  double beta(double dt) const noexcept {
    return std::isfinite(dt) && dt > 0 ? tau / (tau + dt) : 1;
  }
  static double admittedSignal(double x) noexcept {
    if (!std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL)
      return 0;
    const double bound = double(std::numeric_limits<float>::max());
    return std::clamp(x, -bound, bound);
  }
  // Prevent subnormal arithmetic contributions without changing ordinary audio.
  // Both operands are nonnegative. If each is >=2^-450, their product is
  // normal; only the extreme numerical tail pays for the underflow-boundary
  // division.
  static double normalProduct(double a, double b) noexcept {
    constexpr double normal = std::numeric_limits<double>::min();
    if (a < 0x1p-450 || b < 0x1p-450) {
      if (a < normal || b < normal)
        return 0;
      if (a < 1 && b <= normal / a)
        return 0;
    }
    return a * b;
  }
  double processMagnitude(double magnitude, double dt) noexcept {
    const double m = std::abs(admittedSignal(magnitude));
    setLevel(normalProduct(alpha(dt), m) + normalProduct(beta(dt), level));
    return level;
  }
  void setLevel(double value) noexcept { level = std::max(floor, value); }
  double value() const noexcept { return level; }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  void qualificationInject(double value) noexcept { level = value; }
#endif
  double timeConstantSeconds() const noexcept { return tau; }
  double numericalFloor() const noexcept { return floor; }

private:
  double level = 1, tau = .0047, floor = minimumFloor;
};

class BBDFeedbackCompressor {
public:
  void configure(BBDCompanderConfig c) noexcept { detector.configure(c); }
  void reset(double initial = 1) noexcept {
    detector.reset(initial);
    gain = 1 / detector.value();
  }
  double process(double input, double dt) noexcept {
    const double x = CompanderLevelAverager::admittedSignal(input);
    if (std::isfinite(dt) && dt > 0) {
      const double b = CompanderLevelAverager::normalProduct(detector.beta(dt),
                                                             detector.value());
      // L^2 - b L - alpha |x| = 0. The positive root has no cancellation.
      const double level =
          .5 * (b + std::sqrt(CompanderLevelAverager::normalProduct(b, b) +
                              CompanderLevelAverager::normalProduct(
                                  4 * detector.alpha(dt), std::abs(x))));
      detector.setLevel(level);
    }
    gain = 1 / detector.value();
    if (detector.value() > 1 &&
        std::abs(x) <= std::numeric_limits<double>::min() * detector.value())
      return std::copysign(0., x);
    // Division rather than x*gain avoids an intermediate overflow at the floor.
    return x / detector.value();
  }
  const CompanderLevelAverager &levelAverager() const noexcept {
    return detector;
  }
  double currentGain() const noexcept { return gain; }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  void qualificationInjectDetector(double value) noexcept {
    detector.qualificationInject(value);
  }
#endif

private:
  CompanderLevelAverager detector;
  double gain = 1;
};

class BBDFeedforwardExpander {
public:
  void configure(BBDCompanderConfig c) noexcept { detector.configure(c); }
  void reset(double initial = 1) noexcept { detector.reset(initial); }
  double process(double input, double dt) noexcept {
    const double x = CompanderLevelAverager::admittedSignal(input);
    return std::copysign(
        CompanderLevelAverager::normalProduct(
            detector.processMagnitude(std::abs(x), dt), std::abs(x)),
        x);
  }
  const CompanderLevelAverager &levelAverager() const noexcept {
    return detector;
  }
  double currentGain() const noexcept { return detector.value(); }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  void qualificationInjectDetector(double value) noexcept {
    detector.qualificationInject(value);
  }
#endif

private:
  CompanderLevelAverager detector;
};

// Runs outside both continuous/asynchronous filters at the host observation
// rate. The BBD core and every event-time character operation remain untouched.
class BBDFullPath {
public:
  void configure(BBDCompanderConfig c) noexcept {
    enabled = c.enabled;
    compressor.configure(c);
    expander.configure(c);
  }
  void setGainStaging(BBDGainStagingConfig c) noexcept {
    gains=BBDGainStagingConfig::validated(c);
    core.setOperatingDomain(gains.compressorToBBDGain,gains.bbdToExpanderGain,gains.nonlinearReferenceLevel);
  }
  const BBDGainStagingConfig& gainStaging() const noexcept { return gains; }
  void prepare(double rate, std::size_t stages) {
    core.prepare(rate, stages);
    const double effective =
        core.telemetry().hostRateWasNormalized ? 48000 : rate;
    dt = 1 / effective;
    compressor.reset();
    expander.reset();
  }
  void reset() noexcept {
    core.reset();
    compressor.reset();
    expander.reset();
  }
  void setDelaySeconds(double seconds) noexcept {
    core.setDelaySeconds(seconds);
  }
  double process(double input) noexcept {
    if(gains.preCompressorGain!=1) input*=gains.preCompressorGain;
    double output=enabled?expander.process(core.process(compressor.process(input, dt)), dt):core.process(input);
    return gains.postExpanderGain==1?output:output*gains.postExpanderGain;
  }
  ClockedBBDCore core;
  BBDFeedbackCompressor compressor;
  BBDFeedforwardExpander expander;

private:
  BBDGainStagingConfig gains;
  bool enabled = false;
  double dt = 1.0 / 48000;
};
using BBDCompanderQualificationChain = BBDFullPath; // compatibility for existing qualification tools
} // namespace drift
