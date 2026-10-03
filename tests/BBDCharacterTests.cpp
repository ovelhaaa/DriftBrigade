#include "../tools/BBDCharacterMeasurements.h"
#include "AllocationTracker.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace characterQualification;
void require(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
int main() {
  try {
    for (bool filters : {false, true}) {
      drift::ClockedBBDCore a, b;
      auto neutral = example();
      neutral.lossPerStage = neutral.leakagePerStageSecond =
          neutral.residualPolePer1024 = neutral.outputNoiseRms =
              neutral.mismatchFraction = 0;
      setup(a, 1024, 16000, {}, filters);
      setup(b, 1024, 16000, neutral, filters);
      for (int i = 0; i < 20000; ++i) {
        double clock = 8000 + i;
        a.setDelaySeconds(512 / clock);
        b.setDelaySeconds(512 / clock);
        const double x = a.process(std::sin(i * .1));
        const double y = b.process(std::sin(i * .1));
        require(std::memcmp(&x, &y, sizeof(x)) == 0,
                "neutral character bit identity");
      }
    }
    drift::ClockedBBDCore a, b, c;
    drift::BBDDeviceCharacter transparent;
    auto zeroNoise = drift::BBDCharacterConfig{};
    zeroNoise.mode = drift::BBDCharacterMode::FullLinearCharacter;
    transparent.configure(zeroNoise);
    transparent.prepare(1024);
    transparent.update(16000);
    require(std::signbit(transparent.capture(-0.0)) &&
                std::signbit(transparent.transfer(-0.0)),
            "neutral signed zero identity");
    auto cfg = example();
    setup(a, 4096, 16000, cfg);
    setup(b, 4096, 16000, cfg);
    cfg.seed = 42;
    setup(c, 4096, 16000, cfg);
    double energy = 0, otherEnergy = 0;
    bool differs = false;
    const auto before = allocations.load();
    for (int i = 0; i < 200000; ++i) {
      double x = a.process(0), y = b.process(0), z = c.process(0);
      require(x == y, "seed reproducibility");
      differs |= x != z;
      energy += x * x;
      otherEnergy += z * z;
      require(std::isfinite(x), "finite noise");
    }
    require(allocations.load() == before, "process allocations");
    require(differs && energy > 0, "independent seeds");
    require(std::abs(otherEnergy / energy - 1) < .1,
            "seed statistical agreement");
    a.reset();
    b.reset();
    for (int i = 0; i < 20000; ++i)
      require(a.process(0) == b.process(0), "reset reproducibility");
    setup(b, 4096, 16000, example());
    a.reset();
    for (int i = 0; i < 20000; ++i)
      require(a.process(0) == b.process(0), "reprepare reproducibility");
    // Input-referred noise travels through retained bucket history.
    auto inputOnly = example(drift::BBDCharacterMode::NoiseOnly);
    inputOnly.outputNoiseRms = 0;
    inputOnly.inputNoiseRms = 1e-4;
    setup(a, 1024, 16000, inputOnly, false);
    setup(b, 1024, 16000, inputOnly, false);
    for (int i = 0; i < 1500; ++i) {
      const double x = a.process(0);
      require(x == b.process(0), "input noise reproducibility");
      if (i < 1500 && a.telemetry().totalCaptureCount < 512)
        require(x == 0, "input noise traverses buckets");
    }
    // Restore the long-stage fixture for the continuity/safety probe.
    setup(a, 4096, 16000, example());
    drift::BBDDeviceLoss loss;
    cfg = example(drift::BBDCharacterMode::LossOnly);
    loss.update(cfg, 1024, 16000);
    double re = 0, im = 0;
    for (int i = 0; i < 18000; ++i) {
      double y = loss.process(std::sin(2 * drift::pi * .25 * i));
      if (i >= 2000) {
        re += y * std::cos(2 * drift::pi * .25 * i);
        im += y * std::sin(2 * drift::pi * .25 * i);
      }
    }
    require(std::abs(2 * std::hypot(re, im) / 16000 -
                     std::abs(loss.response(.25))) < 1e-10,
            "device response");
    double previous = 0;
    std::uint64_t events = 0;
    const auto noalloc = allocations.load();
    for (int i = 0; i < 100000; ++i) {
      double clock = 8000 + 24000. * i / 100000;
      a.setDelaySeconds(4096 / (2 * clock));
      double gain = a.deviceCharacter().loss.gain;
      require(i == 0 || std::abs(gain - previous) < 1e-6,
              "continuous coefficients");
      previous = gain;
      a.process(i % 2 ? std::numeric_limits<float>::max()
                      : -std::numeric_limits<float>::max());
      require(a.finiteState(), "maximum float safety");
      require(a.telemetry().totalEventCount >= events,
              "history counters retained");
      events = a.telemetry().totalEventCount;
    }
    require(allocations.load() == noalloc, "sweep allocations");
    for (double feedback : {0., .5, .9}) {
      setup(a, 1024, 16000, example());
      double y = 0, peak = 0;
      for (int i = 0; i < 200000; ++i) {
        y = a.process((i == 0 ? .1 : 0) + feedback * y);
        peak = std::max(peak, std::abs(y));
      }
      require(peak < 1 && std::isfinite(y), "bounded feedback");
    }
    // Same clock and seed separates physical-stage noise scaling from clock.
    setup(a, 256, 16000, example(drift::BBDCharacterMode::NoiseOnly), false);
    setup(b, 4096, 16000, example(drift::BBDCharacterMode::NoiseOnly), false);
    for (int i = 0; i < 5000; ++i)
      require(b.process(0) == 4 * a.process(0),
              "sqrt stage noise scaling at fixed clock");
    auto reference = drift::BBDCharacterConfig{};
    reference.mode = drift::BBDCharacterMode::LossOnly;
    reference.insertionDb = 2.3;
    setup(a, 1024, 16000, reference, false);
    require(std::abs(a.deviceCharacter().loss.gain - std::pow(10., 2.3 / 20)) <
                1e-15,
            "optional measured Juno gain");
    auto extreme = example();
    extreme.lossPerStage = std::numeric_limits<double>::infinity();
    extreme.inputNoiseRms = std::numeric_limits<double>::quiet_NaN();
    extreme.residualPolePer1024 = 100;
    extreme.insertionDb = 12;
    setup(a, 65536, 1, extreme);
    for (double delay : {32768., 1e-12}) {
      a.setDelaySeconds(delay);
      for (int i = 0; i < 100; ++i) {
        require(std::isfinite(a.process(std::numeric_limits<float>::max())),
                "extreme configured output");
        require(a.finiteState(), "extreme configured state");
      }
    }
    std::cout << "M2.2 character qualification passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
