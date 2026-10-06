#pragma once
#include <cmath>
namespace drift {
// ENGINEERING NORMALIZATION: no voltage/device calibration is implied.
// x=1 is the nominal maximum; 2 is an overload reporting boundary, not a
// limiter.
struct BBDOperatingLevel {
  static constexpr double nominalLevel = 1, headroomLevel = 2,
                          nonlinearReferenceLevel = 1;
};
enum class BBDHeadroomProfile { LegacyM25, Conservative, Nominal, HighDrive };
struct BBDGainStagingConfig {
  double preCompressorGain = 1, compressorToBBDGain = 1;
  double bbdToExpanderGain = 1, postExpanderGain = 1,
         nonlinearReferenceLevel = 1;
  static BBDGainStagingConfig profile(BBDHeadroomProfile p) noexcept {
    BBDGainStagingConfig c;
    // Exact binary engineering attenuations: -18.06/-12.04/-6.02 dB.
    // Inverse terminal gain restores detector units; no arbitrary makeup gain.
    c.compressorToBBDGain = p == BBDHeadroomProfile::Conservative ? .125
                            : p == BBDHeadroomProfile::Nominal    ? .25
                            : p == BBDHeadroomProfile::HighDrive  ? .5
                                                                  : 1;
    c.bbdToExpanderGain = 1 / c.compressorToBBDGain;
    return c;
  }
  static BBDGainStagingConfig validated(BBDGainStagingConfig c) noexcept {
    auto v = [](double x) {
      return std::isfinite(x) && x >= 0x1p-20 && x <= 0x1p20 ? x : 1.;
    };
    c.preCompressorGain = v(c.preCompressorGain);
    c.compressorToBBDGain = v(c.compressorToBBDGain);
    c.bbdToExpanderGain = v(c.bbdToExpanderGain);
    c.postExpanderGain = v(c.postExpanderGain);
    c.nonlinearReferenceLevel = v(c.nonlinearReferenceLevel);
    return c;
  }
};
} // namespace drift
