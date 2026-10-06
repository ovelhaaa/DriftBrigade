#pragma once
#include "dsp/BBDCompander.h"
#include "reference_m24/ClockedBBDCore.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace companderQualification {
inline void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
inline bool same(double a, double b) {
  return std::memcmp(&a, &b, sizeof(a)) == 0;
}
inline drift::BBDCompanderConfig compConfig(double cap = .47e-6) {
  drift::BBDCompanderConfig c;
  c.enabled = true;
  c.rectifierCapFarads = cap;
  return c;
}
inline void mathematicalTests() {
  using namespace drift;
  for (double cap : {.01e-6, .22e-6, .47e-6, 1e-6, 10e-6}) {
    CompanderLevelAverager avg;
    avg.configure(compConfig(cap));
    avg.reset(0);
    const long double tau = 10000.L * cap;
    require(std::abs(avg.timeConstantSeconds() - double(tau)) < 1e-16,
            "physical RC");
    long double oracle = avg.value();
    for (int n = 0; n < 2000; ++n) {
      const double dt = n % 2 ? 1. / 48000 : 1. / 123456;
      const double m = std::abs(std::sin(n * .17));
      oracle = (dt * m + tau * oracle) / (tau + dt);
      require(std::abs(avg.processMagnitude(m, dt) - double(oracle)) < 3e-14,
              "RC independent reference");
    }
    // Independent closed-form step response for a constant interval.
    avg.reset(0);
    const double dt = 1. / 48000, beta = double(tau / (tau + dt));
    for (int n = 1; n <= int(48000 * double(tau) * 10) + 1; ++n) {
      const double actual = avg.processMagnitude(1, dt);
      require(std::abs(actual - (1 - std::pow(beta, n))) < 2e-12,
              "capacitor step time constant");
    }
    CompanderLevelAverager pos, neg;
    pos.configure(compConfig(cap));
    neg.configure(compConfig(cap));
    for (int n = 0; n < 2000; ++n)
      require(same(pos.processMagnitude(.2, dt), neg.processMagnitude(-.2, dt)),
              "full wave rectification");
    BBDFeedbackCompressor compressor;
    BBDFeedforwardExpander expander;
    compressor.configure(compConfig(cap));
    expander.configure(compConfig(cap));
    // Independent long-double bisection of L - beta*Lprev - alpha*abs(x)/L.
    for (int n = 0; n < 1000; ++n) {
      const double x = (n % 2 ? 1 : -1) * std::pow(10., -8 + 8. * n / 999);
      const long double prev = compressor.levelAverager().value();
      const long double alpha = dt / (tau + dt), b = tau * prev / (tau + dt);
      long double lo = 0, hi = std::max(2.L, prev + std::sqrt(std::abs(x)));
      for (int k = 0; k < 100; ++k) {
        const auto mid = (lo + hi) / 2;
        if (mid - b - alpha * std::abs(x) / mid > 0)
          hi = mid;
        else
          lo = mid;
      }
      const double y = compressor.process(x, dt);
      require(std::abs(compressor.levelAverager().value() -
                       double((lo + hi) / 2)) < 2e-14,
              "positive root independent solve");
      require(std::abs(expander.process(y, dt) - x) < 2e-14,
              "transient ideal round trip");
    }
    for (double db :
         {-80., -70., -60., -50., -40., -30., -24., -18., -12., -6., 0.}) {
      const double x = std::pow(10., db / 20);
      compressor.reset();
      expander.reset();
      double y = 0, z = 0;
      for (int n = 0; n < int(48000 * double(tau) * 100) + 1000; ++n) {
        y = compressor.process(x, dt);
        z = expander.process(x, dt);
      }
      require(std::abs(y / std::sqrt(x) - 1) < 2e-10,
              "compressor analytical DC");
      require(std::abs(z / (x * x) - 1) < 2e-10, "expander analytical DC");
    }
  }
  for (double dt : {1. / 8000, 1. / 48000, 1. / 384000, 1. / 3072000}) {
    BBDFeedbackCompressor c;
    BBDFeedforwardExpander e;
    c.configure(compConfig(1e-6));
    e.configure(compConfig(1e-6));
    for (int exponent = -320; exponent <= 38; exponent += 2) {
      for (double initial : {0., 1.}) {
        c.reset(initial);
        e.reset(initial);
        const double x = std::pow(10., exponent);
        for (int n = 0; n < 20; ++n) {
          const double y = c.process(x, dt), z = e.process(y, dt);
          require(std::isfinite(y) && std::isfinite(z) &&
                      std::isfinite(c.currentGain()),
                  "magnitude decades finite");
          if (x >= std::numeric_limits<double>::min())
            require(std::abs(z - x) <= std::abs(x) * 2e-13,
                    "magnitude decades identity");
        }
      }
    }
  }
  BBDFeedbackCompressor c;
  BBDFeedforwardExpander e;
  require(CompanderLevelAverager::normalProduct(.001, 1e-306) == 0,
          "intermediate subnormal contribution flushed");
  require(CompanderLevelAverager::normalProduct(.5, 1e-300) == .5e-300,
          "normal tail product preserved");
  for (double interval : {0., -1., double(NAN), double(INFINITY)}) {
    const double previousC = c.levelAverager().value();
    const double previousE = e.levelAverager().value();
    require(std::isfinite(c.process(.4, interval)) &&
                std::isfinite(e.process(.4, interval)),
            "invalid interval finite");
    require(same(previousC, c.levelAverager().value()) &&
                same(previousE, e.levelAverager().value()),
            "invalid interval holds state");
  }
  c.configure(compConfig());
  e.configure(compConfig());
  const auto before = allocations.load();
  for (int n = 0; n < 500000; ++n)
    require(c.process(0, 1. / 48000) == 0 && e.process(0, 1. / 48000) == 0,
            "finite silence");
  require(c.levelAverager().value() == CompanderLevelAverager::minimumFloor,
          "silence reaches binary floor");
  for (double x : {std::numeric_limits<double>::denorm_min(), double(NAN),
                   double(INFINITY), -double(INFINITY),
                   double(std::numeric_limits<float>::max()),
                   -double(std::numeric_limits<float>::max())}) {
    require(std::isfinite(c.process(x, 1. / 48000)) &&
                std::isfinite(e.process(x, 1. / 48000)),
            "safety input extremes");
  }
  c.reset();
  e.reset();
  require(std::abs(c.process(1, 1. / 48000)) <= 1 + 4e-15,
          "unity startup pulse");
  require(allocations.load() == before, "component allocations");
}

inline void coreTests() {
  using namespace drift;
  for (auto mode : {BBDMode::TransportOnly, BBDMode::AsyncLinearReference})
    for (auto profile : {BBDFilterProfile::ValidationPrototype,
                         BBDFilterProfile::HoltersParkerTable1})
      for (bool modulation : {false, true}) {
        BBDCompanderQualificationChain now;
        drift_m24::ClockedBBDCore frozen;
        BBDCharacterConfig cc;
        cc.mode = BBDCharacterMode::FullLinearCharacter;
        cc.inputNoiseRms = 1e-5;
        cc.outputNoiseRms = 1e-4;
        cc.seed = 123;
        cc.lossPerStage = 1e-5;
        cc.leakagePerStageSecond = .1;
        cc.residualPolePer1024 = .25;
        cc.mismatchFraction = .01;
        cc.nonlinear = {BBDNonlinearityMode::EngineeringPolynomial, 1};
        drift_m24::BBDCharacterConfig fc;
        fc.mode = drift_m24::BBDCharacterMode::FullLinearCharacter;
        fc.inputNoiseRms = cc.inputNoiseRms;
        fc.outputNoiseRms = cc.outputNoiseRms;
        fc.seed = cc.seed;
        fc.lossPerStage = cc.lossPerStage;
        fc.leakagePerStageSecond = cc.leakagePerStageSecond;
        fc.residualPolePer1024 = cc.residualPolePer1024;
        fc.mismatchFraction = cc.mismatchFraction;
        fc.nonlinear = {drift_m24::BBDNonlinearityMode::EngineeringPolynomial,
                        1};
        now.core.setCharacterConfig(cc);
        frozen.setCharacterConfig(fc);
        now.core.setQualificationMode(mode, profile);
        frozen.setQualificationMode(
            static_cast<drift_m24::BBDMode>(mode),
            static_cast<drift_m24::BBDFilterProfile>(profile));
        now.configure(BBDCompanderConfig{});
        now.prepare(48000, 256);
        frozen.prepare(48000, 256);
        const auto before = allocations.load();
        for (int n = 0; n < 10000; ++n) {
          const double delay =
              modulation ? (n == 5000 ? .003 : .01 + .002 * std::sin(n * .002))
                         : .01;
          now.setDelaySeconds(delay);
          frozen.setDelaySeconds(delay);
          const double x = n == 10 ? -0. : .2 * std::sin(n * .17);
          require(same(now.process(x), frozen.process(x)),
                  "frozen M2.4 output bit identity/RNG progression");
          require(same(now.core.heldOutput(), frozen.heldOutput()),
                  "frozen held bit identity/nonlinear results");
          require(
              same(now.core.inputFilterValue(), frozen.inputFilterValue()) &&
                  same(now.core.outputFilterValue(),
                       frozen.outputFilterValue()),
              "frozen filters");
          require(same(now.core.telemetry().accumulatedClockPhase,
                       frozen.telemetry().accumulatedClockPhase),
                  "frozen phase");
          require(now.core.telemetry().totalEventCount ==
                      frozen.telemetry().totalEventCount,
                  "frozen event history");
          for (int k = 0; k < 128; ++k)
            require(same(now.core.stageValue(k), frozen.stageValue(k)),
                    "frozen bucket memory");
        }
        require(allocations.load() == before, "bypass allocations");
      }
  constexpr int count = 12000;
  std::vector<std::array<double, 3>> reference(count);
  for (int block : {1, 17, 64, 127, 256, 511, 1024}) {
    BBDCompanderQualificationChain chain;
    chain.configure(compConfig());
    chain.core.setQualificationMode(BBDMode::AsyncLinearReference,
                                    BBDFilterProfile::HoltersParkerTable1);
    BBDCharacterConfig cc;
    cc.mode = BBDCharacterMode::NoiseOnly;
    cc.outputNoiseRms = 1e-4;
    cc.nonlinear = {BBDNonlinearityMode::EngineeringPolynomial, 1};
    chain.core.setCharacterConfig(cc);
    chain.prepare(48000, 512);
    const auto before = allocations.load();
    for (int start = 0; start < count; start += block)
      for (int n = start; n < std::min(count, start + block); ++n) {
        chain.setDelaySeconds(.01 + .002 * std::sin(n * .003));
        const double y =
            chain.process(.1 * std::sin(n * .13) * (n % 1000 < 500 ? 1 : .1));
        std::array<double, 3> current{y,
                                      chain.compressor.levelAverager().value(),
                                      chain.expander.levelAverager().value()};
        if (block == 1)
          reference[n] = current;
        else
          require(current == reference[n],
                  "block output/detector bit invariance");
      }
    require(allocations.load() == before, "enabled processing allocations");
  }
  for (unsigned stages : {256u, 512u, 1024u, 2048u, 4096u}) {
    BBDCompanderQualificationChain chain;
    chain.configure(compConfig());
    chain.core.setQualificationMode(BBDMode::AsyncLinearReference,
                                    BBDFilterProfile::HoltersParkerTable1);
    BBDCharacterConfig cc;
    cc.mode = BBDCharacterMode::FullLinearCharacter;
    cc.outputNoiseRms = 1e-4;
    cc.nonlinear = {BBDNonlinearityMode::EngineeringPolynomial, 1};
    chain.core.setCharacterConfig(cc);
    chain.prepare(48000, stages);
    chain.core.collectOperatingStats = true;
    chain.setDelaySeconds(1e-12);
    auto before = allocations.load();
    for (int n = 0; n < 4000; ++n) {
      if (n % 100 == 0) {
        const double c = chain.compressor.levelAverager().value(),
                     e = chain.expander.levelAverager().value();
        const double phase = chain.core.telemetry().accumulatedClockPhase,
                     bucket = chain.core.stageValue(0),
                     held = chain.core.heldOutput();
        chain.setDelaySeconds(n % 200 ? 1e-12 : .003);
        require(same(c, chain.compressor.levelAverager().value()) &&
                    same(e, chain.expander.levelAverager().value()),
                "clock does not reset detectors");
        require(same(phase, chain.core.telemetry().accumulatedClockPhase) &&
                    same(bucket, chain.core.stageValue(0)) &&
                    same(held, chain.core.heldOutput()),
                "cache preserves history");
      }
      const double x =
          n % 7 == 0   ? NAN
          : n % 7 == 1 ? INFINITY
          : n % 7 == 2 ? -INFINITY
          : n % 7 == 3 ? 1e-320
          : n % 7 == 4
              ? 0
              : (n % 2 ? 1 : -1) * double(std::numeric_limits<float>::max());
      require(std::isfinite(chain.process(x)), "max event stress output");
    }
    require(chain.core.finiteState() &&
                std::isfinite(chain.compressor.currentGain()) &&
                std::isfinite(chain.expander.currentGain()),
            "max event finite state");
    require(chain.core.operatingStats.count > 0 &&
                std::isfinite(chain.core.operatingStats.sumSquares),
            "instrumented capture statistics finite");
    require(allocations.load() == before, "max event allocations");
  }
}
inline void tests() {
  mathematicalTests();
  coreTests();
  drift::BBDCompanderQualificationChain dc;
  dc.configure(compConfig());
  dc.core.setQualificationMode(drift::BBDMode::AsyncLinearReference,
                               drift::BBDFilterProfile::HoltersParkerTable1);
  drift::BBDCharacterConfig character;
  character.mode = drift::BBDCharacterMode::FullLinearCharacter;
  character.inputNoiseRms = 1e-5;
  character.outputNoiseRms = 1e-4;
  character.nonlinear = {drift::BBDNonlinearityMode::EngineeringPolynomial, 1};
  dc.core.setCharacterConfig(character);
  dc.prepare(48000, 1024);
  dc.setDelaySeconds(.01);
  const auto before = allocations.load();
  for (int n = 0; n < 96000; ++n)
    require(std::isfinite(dc.process(.5)), "long DC compander core");
  require(dc.core.finiteState() && allocations.load() == before,
          "long DC finite and allocation free");
}
} // namespace companderQualification
