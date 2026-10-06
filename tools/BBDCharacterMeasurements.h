#pragma once
#include "dsp/ClockedBBDCore.h"
namespace characterQualification {
inline drift::BBDCharacterConfig
example(drift::BBDCharacterMode mode =
            drift::BBDCharacterMode::FullLinearCharacter) {
  auto c = drift::BBDCharacterConfig::syntheticQualificationFixture();
  c.mode = mode;
  // Deliberately synthetic sensitivity fixture, never a device calibration.
  return c;
}
inline void setup(drift::ClockedBBDCore &core, std::size_t stages, double clock,
                  drift::BBDCharacterConfig config, bool filters = true,
                  double rate = 48000) {
  core.setCharacterConfig(config);
  core.setQualificationMode(filters ? drift::BBDMode::AsyncLinearReference
                                    : drift::BBDMode::TransportOnly,
                            drift::BBDFilterProfile::HoltersParkerTable1);
  core.prepare(rate, stages);
  core.setDelaySeconds(stages / (2 * clock));
}
} // namespace characterQualification
