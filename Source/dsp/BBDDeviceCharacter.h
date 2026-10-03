#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
namespace drift {
enum class BBDCharacterMode { Ideal, LossOnly, NoiseOnly, FullLinearCharacter };
struct BBDCharacterConfig {
  BBDCharacterMode mode = BBDCharacterMode::Ideal;
  double insertionDb = 0, lossPerStage = 0, leakagePerStageSecond = 0;
  double residualPolePer1024 = 0, inputNoiseRms = 0, outputNoiseRms = 0,
         mismatchFraction = 0;
  std::uint32_t seed = 1;
};
class BBDNoiseModel {
public:
  void prepare(std::uint32_t seed) noexcept {
    initial = seed ? seed : 1;
    reset();
  }
  void reset() noexcept { state = initial; }
  double sample(double rms) noexcept {
    if (rms == 0)
      return 0; // Also preserves PRNG state in exact bypass.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (static_cast<double>(state) / 4294967296.0 * 2 - 1) *
           1.7320508075688772 * rms;
  }

private:
  std::uint32_t initial = 1, state = 1;
};
class BBDDeviceLoss {
public:
  void update(const BBDCharacterConfig &c, std::size_t stages,
              double clock) noexcept {
    gain = std::pow(10., c.insertionDb / 20) *
           std::exp(-static_cast<double>(stages) *
                    (c.lossPerStage + c.leakagePerStageSecond / (2 * clock)));
    pole = c.residualPolePer1024 == 0
               ? 0
               : std::exp(-1024.0 / (c.residualPolePer1024 * stages));
  }
  void reset() noexcept { memory = 0; }
  double process(double x) noexcept {
    if (pole == 0)
      return gain == 1 ? x : x * gain;
    memory = (1 - pole) * x + pole * memory;
    if (std::abs(memory) < 1e-280)
      memory = 0;
    return memory * gain;
  }
  std::complex<double> response(double ratio) const noexcept {
    return gain * (1 - pole) /
           (1.0 - pole * std::exp(std::complex<double>(
                             0, -6.2831853071795864769 * ratio)));
  }
  bool finiteState() const noexcept {
    return std::isfinite(memory) && std::isfinite(gain) && std::isfinite(pole);
  }
  double gain = 1, pole = 0;

private:
  double memory = 0;
};
class BBDTransferImperfection {
public:
  void prepare(double fraction, std::uint32_t seed) noexcept {
    BBDNoiseModel draw;
    draw.prepare(seed);
    gain = 1 + draw.sample(fraction / 1.7320508075688772);
  }
  double process(double x) const noexcept { return gain == 1 ? x : x * gain; }
  double gain = 1;
};
class BBDDeviceCharacter {
public:
  void configure(BBDCharacterConfig value) noexcept {
    config = value;
    auto bound = [](double x, double lo, double hi) {
      return std::isfinite(x) ? std::clamp(x, lo, hi) : lo;
    };
    config.insertionDb = bound(value.insertionDb, -120, 12);
    config.lossPerStage = bound(value.lossPerStage, 0, 1);
    config.leakagePerStageSecond = bound(value.leakagePerStageSecond, 0, 1);
    config.residualPolePer1024 = bound(value.residualPolePer1024, 0, 100);
    config.inputNoiseRms = bound(value.inputNoiseRms, 0, 1);
    config.outputNoiseRms = bound(value.outputNoiseRms, 0, 1);
    config.mismatchFraction = bound(value.mismatchFraction, 0, .1);
  }
  void prepare(std::size_t n) noexcept {
    stages = n;
    noiseScale = std::sqrt(n / 1024.0);
    inputNoise.prepare(config.seed);
    outputNoise.prepare(config.seed ^ 0x9e3779b9u);
    mismatch.prepare(config.mismatchFraction, config.seed ^ 0x85ebca6bu);
    reset();
  }
  void reset() noexcept {
    loss.reset();
    inputNoise.reset();
    outputNoise.reset();
  }
  void update(double clock) noexcept {
    if (lossEnabled())
      loss.update(config, stages, clock);
  }
  double capture(double x) noexcept {
    return noiseEnabled() && config.inputNoiseRms != 0
               ? x + inputNoise.sample(config.inputNoiseRms * noiseScale)
               : x;
  }
  double transfer(double x) noexcept {
    if (lossEnabled())
      x = loss.process(x);
    if (config.mode == BBDCharacterMode::FullLinearCharacter)
      x = mismatch.process(x);
    if (noiseEnabled() && config.outputNoiseRms != 0)
      x += outputNoise.sample(config.outputNoiseRms * noiseScale);
    return x;
  }
  bool lossEnabled() const noexcept {
    return config.mode == BBDCharacterMode::LossOnly ||
           config.mode == BBDCharacterMode::FullLinearCharacter;
  }
  bool noiseEnabled() const noexcept {
    return config.mode == BBDCharacterMode::NoiseOnly ||
           config.mode == BBDCharacterMode::FullLinearCharacter;
  }
  bool finiteState() const noexcept {
    return loss.finiteState() && std::isfinite(mismatch.gain);
  }
  BBDDeviceLoss loss;
  BBDTransferImperfection mismatch;

private:
  BBDCharacterConfig config;
  BBDNoiseModel inputNoise, outputNoise;
  std::size_t stages = 2;
  double noiseScale = 1;
};
} // namespace drift
