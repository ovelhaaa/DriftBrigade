#include "../tests/AllocationTracker.h"
#include "../tests/reference_m25/BBDCompander.h"
#include "dsp/BBDCompander.h"
#include <array>
#include <chrono>
#include <complex>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace drift;
constexpr double rate = 48000, dt = 1 / rate, pi2 = 6.2831853071795864769;
const std::array<double, 11> levels = {-80, -60, -48, -36, -24, -18,
                                       -12, -9,  -6,  -3,  0};
const char *names[] = {"LegacyM25", "Conservative", "Nominal", "HighDrive"};
const char *signals[] = {"sine",    "low_sine", "burst", "impulse",
                         "stepped", "program",  "dc"};
double amp(double db) { return std::pow(10., db / 20); }
double db(double x) { return 20 * std::log10(std::max(x, 1e-150)); }
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
bool same(double a, double b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
BBDGainStagingConfig profile(int p) {
  return BBDGainStagingConfig::profile(static_cast<BBDHeadroomProfile>(p));
}
BBDCharacterConfig character(int kind = 1, double noise = 0) {
  BBDCharacterConfig c;
  c.nonlinear.mode = BBDNonlinearityMode::EngineeringPolynomial;
  c.nonlinear.strength = kind ? 1 : 0;
  c.mode =
      noise ? BBDCharacterMode::FullLinearCharacter : BBDCharacterMode::Ideal;
  c.inputNoiseRms = noise;
  c.outputNoiseRms = noise;
  c.seed = 570;
  if (kind == 3) {
    c.lossPerStage = 1e-5;
    c.residualPolePer1024 = .4;
    c.mismatchFraction = .01;
  }
  return c;
}
void setup(BBDCompanderQualificationChain &c, BBDGainStagingConfig g,
           int kind = 1, double noise = 0, bool enabled = true,
           std::size_t stages = 1024, double delay = .01) {
  BBDCompanderConfig cfg;
  cfg.enabled = enabled;
  c.configure(cfg);
  c.setGainStaging(g);
  c.core.setQualificationMode(BBDMode::AsyncLinearReference,
                              BBDFilterProfile::HoltersParkerTable1);
  c.core.setCharacterConfig(character(kind, noise));
  c.prepare(rate, stages);
  c.setDelaySeconds(delay);
  c.core.collectOperatingStats = true;
}
double stimulus(int k, int n) {
  double t = n / rate, s = std::sin(pi2 * 500 * t);
  switch (k) {
  case 0:
    return s;
  case 1:
    return std::sin(pi2 * 25 * t);
  case 2:
    return t < .02 ? std::exp(-t * 100) * std::cos(pi2 * 700 * t) : 0;
  case 3:
    return n == 0 ? 1 : 0;
  case 4:
    return (t < .05 ? .01 : t < .15 ? 1 : .1) * s;
  case 5:
    return (.6 * std::sin(pi2 * 173 * t) + .25 * std::sin(pi2 * 509 * t) +
            .15 * std::sin(pi2 * 1481 * t)) *
           (.1 + .9 * .5 * (1 + std::cos(pi2 * 3 * t)));
  default:
    return 1;
  }
}
struct Meter {
  std::size_t n = 0;
  double power = 0, peak = 0, sum = 0, error = 0, errorPeak = 0, cross = 0,
         refPower = 0;
  std::array<std::complex<double>, 5> h{};
  void add(double y, int i, double hz = 500, double ref = 0) {
    ++n;
    power += y * y;
    peak = std::max(peak, std::abs(y));
    sum += y;
    error += (y - ref) * (y - ref);
    errorPeak = std::max(errorPeak, std::abs(y - ref));
    cross += y * ref;
    refPower += ref * ref;
    for (int k = 0; k < 5; ++k)
      h[k] += y * std::polar(1., -pi2 * hz * (k + 1) * i / rate);
  }
  double rms() const { return std::sqrt(power / std::max(std::size_t(1), n)); }
  double harmonic(int k) const {
    return 2 * std::abs(h[k]) / std::max(std::size_t(1), n);
  }
  double thd() const {
    double p = 0;
    for (int k = 1; k < 5; ++k)
      p += std::norm(h[k]);
    return std::sqrt(p) / std::max(std::abs(h[0]), 1e-150);
  }
};
void statsHeader(std::ostream &o) {
  o << "captures,internal_rms,internal_peak,mean_abs,below_001,001_to_01,01_to_"
       "1,1_to_2,above_2,nominal_fraction,useful_fraction,max_overload,above_"
       "nominal_seconds";
}
void stats(std::ostream &o, const ClockedBBDCore::InputOperatingStats &s) {
  double n = double(std::max(std::uint64_t(1), s.count));
  o << s.count << ',' << std::sqrt(s.sumSquares / n) << ',' << s.peak << ','
    << s.sumMagnitude / n;
  for (auto b : s.bins)
    o << ',' << b / n;
  o << ',' << s.nominalCount / n << ',' << s.usefulCount / n << ','
    << std::max(0., s.peak - 1) << ',' << s.aboveNominalSeconds;
}
void numerical() {
  // Independent frozen implementation, full states observable at every host
  // step.
  for (int clock = 0; clock < 4; ++clock) {
    BBDCompanderQualificationChain a;
    setup(a, profile(0), 3, 1e-5);
    frozenM25::BBDCompanderQualificationChain b;
    frozenM25::BBDCompanderConfig cc;
    cc.enabled = true;
    b.configure(cc);
    b.core.setQualificationMode(
        frozenM25::BBDMode::AsyncLinearReference,
        frozenM25::BBDFilterProfile::HoltersParkerTable1);
    frozenM25::BBDCharacterConfig bc;
    bc.mode = frozenM25::BBDCharacterMode::FullLinearCharacter;
    bc.nonlinear.mode = frozenM25::BBDNonlinearityMode::EngineeringPolynomial;
    bc.nonlinear.strength = 1;
    bc.inputNoiseRms = bc.outputNoiseRms = 1e-5;
    bc.seed = 570;
    bc.lossPerStage = 1e-5;
    bc.residualPolePer1024 = .4;
    bc.mismatchFraction = .01;
    b.core.setCharacterConfig(bc);
    b.prepare(rate, 1024);
    b.setDelaySeconds(.01);
    const auto before = allocations.load();
    for (int n = 0; n < 12000; ++n) {
      double delay = clock == 0   ? .01
                     : clock == 1 ? .01 + .002 * std::sin(n * .001)
                     : clock == 2
                         ? .01 + .002 * std::cos(pi2 * (n % 2400) / 2400)
                     : n < 6000 ? .01
                                : .003;
      double cd = a.compressor.levelAverager().value(),
             ed = a.expander.levelAverager().value();
      auto count = a.core.telemetry().totalCaptureCount;
      double bucket = a.core.stageValue(0);
      a.setDelaySeconds(delay);
      b.setDelaySeconds(delay);
      check(same(cd, a.compressor.levelAverager().value()) &&
                same(ed, a.expander.levelAverager().value()) &&
                same(bucket, a.core.stageValue(0)) &&
                count == a.core.telemetry().totalCaptureCount,
            "clock reset state");
      double x = .2 * stimulus(5, n);
      check(same(a.process(x), b.process(x)), "frozen M2.5 output");
      check(same(a.compressor.currentGain(), b.compressor.currentGain()) &&
                same(a.expander.currentGain(), b.expander.currentGain()),
            "frozen detector gain");
      check(same(a.compressor.levelAverager().value(),
                 b.compressor.levelAverager().value()),
            "frozen compressor state");
      check(same(a.expander.levelAverager().value(),
                 b.expander.levelAverager().value()),
            "frozen expander state");
      check(same(a.core.heldOutput(), b.core.heldOutput()), "frozen held");
      check(same(a.core.telemetry().accumulatedClockPhase,
                 b.core.telemetry().accumulatedClockPhase) &&
                a.core.telemetry().totalEventCount ==
                    b.core.telemetry().totalEventCount,
            "frozen scheduler");
      auto ai = a.core.inputFilterState(), bi = b.core.inputFilterState(),
           ao = a.core.outputFilterState(), bo = b.core.outputFilterState();
      check(std::memcmp(&ai, &bi, sizeof(ai)) == 0 &&
                std::memcmp(&ao, &bo, sizeof(ao)) == 0,
            "frozen filters");
      for (int j = 0; j < 512; ++j)
        check(same(a.core.stageValue(j), b.core.stageValue(j)),
              "frozen buckets");
    }
    check(before == allocations.load(), "Legacy allocations");
  }
  // TransportOnly with N=2 and clock=hostRate is the exact identity transport.
  for (int p = 0; p < 4; ++p) {
    BBDCompanderQualificationChain c;
    BBDCompanderConfig cfg;
    cfg.enabled = true;
    c.configure(cfg);
    c.setGainStaging(profile(p));
    c.prepare(rate, 2);
    c.setDelaySeconds(1 / rate);
    auto before = allocations.load();
    for (int n = 0; n < 12000; ++n) {
      double x = stimulus(n % 7, n) * .7;
      double y = c.process(x);
      check(std::abs(y - x) < 3e-14, "inverse gain transient roundtrip");
    }
    for (int n = 0; n < 480000; ++n)
      c.process(0);
    check(std::isfinite(c.process(1)) && c.core.finiteState(),
          "finite silence recovery");
    check(before == allocations.load(), "profile allocations");
  }
  // Explicit pre/post gains cancel around the nonlinear detector pair.
  {
    BBDCompanderQualificationChain c;
    BBDCompanderConfig cfg;
    cfg.enabled = true;
    c.configure(cfg);
    auto g = profile(2);
    g.preCompressorGain = .5;
    g.postExpanderGain = 2;
    c.setGainStaging(g);
    c.prepare(rate, 2);
    c.setDelaySeconds(dt);
    for (int n = 0; n < 12000; ++n) {
      double x = .3 * stimulus(5, n);
      check(std::abs(c.process(x) - x) < 3e-14, "pre/post roundtrip");
    }
  }
  // Capture histogram boundaries must include exactly one bin per event.
  {
    ClockedBBDCore c;
    c.prepare(rate, 2);
    c.setDelaySeconds(dt);
    c.collectOperatingStats = true;
    for (double x : {0., .005, .01, .05, .1, .5, 1., 1.5, 2., 3.})
      c.process(x);
    const auto s = c.operatingStats;
    check(s.count == 10 && s.bins[0] == 2 && s.bins[1] == 2 && s.bins[2] == 3 &&
              s.bins[3] == 2 && s.bins[4] == 1 && s.nominalCount == 7,
          "event histogram boundaries");
  }
  // Binary scaling commutes exactly with the ideal asynchronous filters.
  for (int p = 1; p < 4; ++p) {
    BBDCompanderQualificationChain a, b;
    setup(a, profile(0), 0);
    setup(b, profile(p), 0);
    const auto before = allocations.load();
    for (int n = 0; n < 24000; ++n) {
      double d = .01 + .002 * std::sin(n * .001);
      a.setDelaySeconds(d);
      b.setDelaySeconds(d);
      check(
          same(a.process(.2 * stimulus(5, n)), b.process(.2 * stimulus(5, n))),
          "gain-only clock zipper equivalence");
      check(same(a.compressor.currentGain(), b.compressor.currentGain()) &&
                same(a.expander.currentGain(), b.expander.currentGain()),
            "gain-only detector preservation");
    }
    check(before == allocations.load(), "modulated gain allocations");
  }
  // Reference scaling independent oracle, disabled normalization exact bypass.
  for (double r : {.5, 1., 2.}) {
    ClockedBBDCore c;
    c.setOperatingDomain(1, 1, r);
    auto cfg = character();
    c.setCharacterConfig(cfg);
    c.prepare(rate, 2);
    c.setDelaySeconds(1 / rate);
    BBDNonlinearTransfer t;
    t.configure(cfg.nonlinear);
    for (int i = 0; i < 100; ++i) {
      double x = -3. + 6. * i / 99;
      check(same(c.process(x), t.process(x / r) * r),
            "normalized transfer oracle");
    }
  }
  std::cout << "M2.6 numerical: frozen legacy, unity reference, inverse gains, "
               "finite startup, state and allocations passed\n";
}
struct Result {
  Meter out, clean, noise;
  ClockedBBDCore::InputOperatingStats in;
};
Result run(BBDGainStagingConfig g, double level, int kind = 1,
           double noise = 1e-4, bool enabled = true, int signal = 0) {
  BBDCompanderQualificationChain a, b;
  setup(a, g, kind, noise, enabled);
  setup(b, g, kind, 0, enabled);
  for (int n = 0; n < 12000; ++n) {
    double x = amp(level) * stimulus(signal, n);
    a.process(x);
    b.process(x);
  }
  a.core.operatingStats = {};
  Result r;
  for (int n = 0; n < 12000; ++n) {
    double x = amp(level) * stimulus(signal, n + 12000), y = a.process(x),
           z = b.process(x);
    r.out.add(y, n, signal == 1 ? 25 : 500);
    r.clean.add(z, n, signal == 1 ? 25 : 500);
    r.noise.add(y - z, n);
  }
  r.in = a.core.operatingStats;
  return r;
}
void metricHeader(std::ostream &o) {
  o << "external_peak_db,signal_rms,noise_rms,snr_db,thd,h2,h3,h4,h5,";
  statsHeader(o);
  o << '\n';
}
void metric(std::ostream &o, double level, const Result &r) {
  o << level << ',' << r.clean.rms() << ',' << r.noise.rms() << ','
    << db(r.clean.rms() / std::max(r.noise.rms(), 1e-150)) << ','
    << r.clean.thd();
  for (int k = 1; k < 5; ++k)
    o << ',' << r.clean.harmonic(k);
  o << ',';
  stats(o, r.in);
  o << '\n';
}
void distributions(std::ostream &o) {
  o << "profile,stimulus,external_peak_db,external_rms,external_peak,output_"
       "rms,output_peak,";
  statsHeader(o);
  o << '\n';
  for (int p = 0; p < 4; ++p)
    for (double level : levels)
      for (int k = 0; k < 6; ++k) {
        BBDCompanderQualificationChain c;
        setup(c, profile(p));
        Meter ext, out;
        // Include onset for transient tests; steady sine rows exclude the
        // settling interval.
        if (k < 2)
          for (int n = 0; n < 24000; ++n)
            c.process(amp(level) * stimulus(k, n));
        c.core.operatingStats = {};
        for (int n = 0; n < 24000; ++n) {
          double x = amp(level) * stimulus(k, n);
          ext.add(x, n);
          out.add(c.process(x), n);
        }
        o << names[p] << ',' << signals[k] << ',' << level << ',' << ext.rms()
          << ',' << ext.peak << ',' << out.rms() << ',' << out.peak << ',';
        stats(o, c.core.operatingStats);
        o << '\n';
      }
}
void startup(std::ostream &o) {
  o << "profile,silence_seconds,onset,external_peak_db,detector_before,gain_"
       "before,compressor_peak,nonlinear_input_peak,nonlinear_output_peak,"
       "expander_detector_peak,final_peak,";
  statsHeader(o);
  o << '\n';
  for (int p = 0; p < 4; ++p)
    for (double silence : {0., .01, .1, .5, 2., 10.})
      for (int k = 0; k < 6; ++k) {
        BBDCompanderQualificationChain c;
        setup(c, profile(p));
        for (int n = 0; n < int(silence * rate); ++n)
          c.process(0);
        double before = c.compressor.levelAverager().value(),
               gain = c.compressor.currentGain(), cp = 0, ni = 0, no = 0,
               ed = 0, fp = 0;
        c.core.operatingStats = {};
        double level = k < 3 ? std::array<double, 3>{-60, -24, 0}[k] : 0;
        for (int n = 0; n < 12000; ++n) {
          int sig = k < 3 ? 0 : k == 3 ? 6 : k == 4 ? 3 : 2;
          double y = c.process(amp(level) * stimulus(sig, n));
          cp = std::max(cp, std::abs(amp(level) * stimulus(sig, n) *
                                     c.compressor.currentGain()));
          ni = c.core.operatingStats.nonlinearInputPeak;
          no = c.core.operatingStats.nonlinearOutputPeak;
          ed = std::max(ed, c.expander.levelAverager().value());
          fp = std::max(fp, std::abs(y));
        }
        o << names[p] << ',' << silence << ','
          << (k < 3    ? "sine"
              : k == 3 ? "dc"
              : k == 4 ? "impulse"
                       : "burst")
          << ',' << level << ',' << before << ',' << gain << ',' << cp << ','
          << ni << ',' << no << ',' << ed << ',' << fp << ',';
        stats(o, c.core.operatingStats);
        o << '\n';
      }
}
void roundtrip(std::ostream &o) {
  o << "profile,transport,stimulus,dc_gain,rms_gain,rms_error,peak_error\n";
  for (int p = 0; p < 4; ++p)
    for (int k : {6, 0, 2, 3, 4}) {
      BBDCompanderQualificationChain a, b;
      BBDCompanderConfig cfg;
      cfg.enabled = true;
      a.configure(cfg);
      a.setGainStaging(profile(p));
      a.prepare(rate, 2);
      a.setDelaySeconds(dt);
      setup(b, profile(p), 0, 0);
      Meter exact, delayed, input;
      for (int n = 0; n < 24000; ++n) {
        double x = .5 * stimulus(k, n), y = a.process(x), z = b.process(x);
        exact.add(y, n, 500, x);
        delayed.add(z, n, 500, x);
        input.add(x, n);
      }
      for (int j = 0; j < 2; ++j) {
        auto m = j ? delayed : exact;
        o << names[p] << ','
          << (j ? "async_1024_10ms_unaligned" : "identity_2_stage") << ','
          << signals[k] << ',';
        if (std::abs(input.sum) > 1e-12)
          o << m.sum / input.sum;
        o << ',' << m.rms() / std::max(input.rms(), 1e-150) << ','
          << std::sqrt(m.error / m.n) << ',' << m.errorPeak << '\n';
      }
    }
}
double randomTarget(int index) {
  std::uint32_t x = 570u + std::uint32_t(index) * 0x9e3779b9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return double(x) / 4294967296.0 * 2 - 1;
}
void clocks(std::ostream &o) {
  o << "profile,clock,output_peak,output_rms,events,detector_final,expander_"
       "final,";
  statsHeader(o);
  o << '\n';
  for (int p = 0; p < 4; ++p)
    for (int k = 0; k < 4; ++k) {
      BBDCompanderQualificationChain c;
      setup(c, profile(p), 3, 1e-4);
      Meter m;
      for (int n = 0; n < 48000; ++n) {
        double t = n / rate,
               d = k == 0   ? .01
                   : k == 1 ? .01 + .002 * std::sin(pi2 * 3 * t)
                   : k == 2
                       ? .01 +
                             .002 *
                                 ((1 - .5 * (1 - std::cos(pi2 * .5 *
                                                          (n % 8000) / 8000))) *
                                      randomTarget(n / 8000) +
                                  .5 *
                                      (1 -
                                       std::cos(pi2 * .5 * (n % 8000) / 8000)) *
                                      randomTarget(n / 8000 + 1))
                   : n < 24000 ? .01
                               : .003;
        double a = c.compressor.currentGain(), b = c.expander.currentGain(),
               h = c.core.heldOutput();
        auto count = c.core.telemetry().totalEventCount;
        c.setDelaySeconds(d);
        check(same(a, c.compressor.currentGain()) &&
                  same(b, c.expander.currentGain()) &&
                  same(h, c.core.heldOutput()) &&
                  count == c.core.telemetry().totalEventCount,
              "modulation state");
        m.add(c.process(.5 * stimulus(5, n)), n);
        check(c.core.finiteState(), "clock finite");
      }
      o << names[p] << ','
        << std::array<const char *, 4>{"fixed", "sine", "raised_cosine_random",
                                       "abrupt"}[k]
        << ',' << m.peak << ',' << m.rms() << ','
        << c.core.telemetry().totalEventCount << ','
        << c.compressor.currentGain() << ',' << c.expander.currentGain() << ',';
      stats(o, c.core.operatingStats);
      o << '\n';
    }
}
void feedback(std::ostream &o) {
  o << "profile,character,feedback,loop_peak,rms,dc,h2,h3,h4,h5,late_early_db_"
       "per_second,limit_cycle_candidate,";
  statsHeader(o);
  o << '\n';
  for (int p = 0; p < 4; ++p)
    for (int k = 0; k < 4; ++k)
      for (double f : {0., .5, .8, .9, .95}) {
        BBDCompanderQualificationChain c;
        setup(c, profile(p), k, k >= 2 ? 1e-4 : 0, k == 3);
        double held = 0;
        Meter m, early, late;
        for (int n = 0; n < 96000; ++n) {
          double x = (n < 4800 ? .1 * stimulus(0, n) : 0) + f * held;
          held = c.process(x);
          check(std::isfinite(held), "feedback finite");
          m.add(held, n);
          if (n >= 24000 && n < 48000)
            early.add(held, n);
          if (n >= 72000)
            late.add(held, n);
        }
        double slope = db(late.rms() / std::max(early.rms(), 1e-150));
        o << names[p] << ','
          << std::array<const char *, 4>{"linear", "nonlinear",
                                         "nonlinear_noise", "full_compander"}[k]
          << ',' << f << ',' << m.peak << ',' << m.rms() << ',' << m.sum / m.n;
        for (int j = 1; j < 5; ++j)
          o << ',' << m.harmonic(j);
        o << ',' << slope << ','
          << (k < 2 && late.rms() > 1e-8 && std::abs(slope) < 1) << ',';
        stats(o, c.core.operatingStats);
        o << '\n';
      }
}
void stereo(std::ostream &o) {
  o << "profile,input_balance_db,output_balance_db,balance_shift_db,relative_"
       "gain_error_db,correlation,left_peak,right_peak,compressor_gain_"
       "divergence,expander_gain_divergence\n";
  for (int p = 0; p < 4; ++p) {
    BBDCompanderQualificationChain l, r;
    setup(l, profile(p));
    setup(r, profile(p));
    Meter lm, rm, li, ri;
    double cross = 0;
    for (int n = 0; n < 48000; ++n) {
      double x = .5 * stimulus(5, n), z = .05 * stimulus(4, n),
             a = l.process(x), b = r.process(z);
      if (n >= 24000) {
        lm.add(a, n);
        rm.add(b, n);
        li.add(x, n);
        ri.add(z, n);
        cross += a * b;
      }
    }
    double ib = db(li.rms() / ri.rms()), ob = db(lm.rms() / rm.rms());
    o << names[p] << ',' << ib << ',' << ob << ',' << ob - ib << ','
      << db((lm.rms() / li.rms()) / (rm.rms() / ri.rms())) << ','
      << cross / std::sqrt(lm.power * rm.power) << ','
      << l.core.operatingStats.peak << ',' << r.core.operatingStats.peak << ','
      << l.compressor.currentGain() - r.compressor.currentGain() << ','
      << l.expander.currentGain() - r.expander.currentGain() << '\n';
  }
}
volatile double sink = 0;
void performance(std::ostream &o) {
  o << "cores,stages,delay_ms,case,median_seconds,realtime_factor,overhead_"
       "percent,detector_updates,events_per_second,nonlinear_evaluations_per_"
       "second\n";
  for (int cores : {2, 8})
    for (int stages : {512, 1024, 2048, 4096})
      for (double delay : {.003, .01, .03}) {
        std::array<double, 4> times{};
        for (int k = 0; k < 4; ++k) {
          std::vector<double> samples(24000);
          for (int n = 0; n < 24000; ++n)
            samples[n] = .2 * stimulus(5, n);
          std::vector<BBDCompanderQualificationChain> chains(cores);
          std::vector<frozenM25::BBDCompanderQualificationChain> old(cores);
          for (int j = 0; j < cores; ++j) {
            setup(chains[j], k <= 1 ? profile(0) : profile(2), 1, 0, true,
                  stages, delay);
            chains[j].core.collectOperatingStats = false;
            if (k == 3) {
              auto g = profile(2);
              g.nonlinearReferenceLevel = 2;
              chains[j].setGainStaging(g);
            }
            frozenM25::BBDCompanderConfig cfg;
            cfg.enabled = true;
            old[j].configure(cfg);
            old[j].core.setQualificationMode(
                frozenM25::BBDMode::AsyncLinearReference,
                frozenM25::BBDFilterProfile::HoltersParkerTable1);
            frozenM25::BBDCharacterConfig ch;
            ch.nonlinear.mode =
                frozenM25::BBDNonlinearityMode::EngineeringPolynomial;
            ch.nonlinear.strength = 1;
            old[j].core.setCharacterConfig(ch);
            old[j].prepare(rate, stages);
            old[j].setDelaySeconds(delay);
          }
          std::array<double, 5> trials{};
          for (int rep = 0; rep < 5; ++rep) {
            auto start = std::chrono::steady_clock::now();
            double sum = 0;
            for (double x : samples)
              for (int j = 0; j < cores; ++j)
                sum += k == 0 ? old[j].process(x) : chains[j].process(x);
            trials[rep] = std::chrono::duration<double>(
                              std::chrono::steady_clock::now() - start)
                              .count();
            sink = sum;
          }
          std::sort(trials.begin(), trials.end());
          times[k] = trials[2];
          o << cores << ',' << stages << ',' << delay * 1000 << ','
            << std::array<const char *, 4>{"frozen_M25", "explicit_unity",
                                           "gain_staging",
                                           "gain_and_reference"}[k]
            << ',' << times[k] << ',' << times[k] / .5 << ','
            << 100 * (times[k] / times[0] - 1) << ',' << cores * 24000 * 2 << ','
            << cores * stages / delay << ',' << cores * stages / (2 * delay)
            << '\n';
        }
      }
}
struct CheckedStream {
  std::ofstream stream;
  explicit CheckedStream(const std::filesystem::path &p) : stream(p) {
    check(bool(stream), "artifact open failure");
    stream.exceptions(std::ios::badbit | std::ios::failbit);
  }
  CheckedStream(CheckedStream &&) = default;
  operator std::ostream &() { return stream; }
  template <class T> CheckedStream &operator<<(const T &x) {
    stream << x;
    return *this;
  }
  ~CheckedStream() noexcept(false) {
    if (stream.is_open() && std::uncaught_exceptions() == 0) {
      stream.flush();
      stream.close();
    }
  }
};
int main(int argc, char **argv) {
  try {
    numerical();
    if (argc > 1 && std::string(argv[1]) == "--tests-only")
      return 0;
    std::filesystem::path root = argc > 1 ? argv[1] : "bbd-headroom-artifact";
    std::filesystem::create_directories(root);
    auto file = [&](const char *n) {
      CheckedStream o(root / n);
      o << std::setprecision(12);
      return o;
    };
    {
      auto o = file("bbd_gain_profiles.csv");
      o << "profile,pre_compressor,compressor_to_bbd,bbd_to_expander,post_"
           "expander,nonlinear_reference,nominal,headroom,class\n";
      for (int p = 0; p < 4; ++p) {
        auto g = profile(p);
        o << names[p] << ',' << g.preCompressorGain << ','
          << g.compressorToBBDGain << ',' << g.bbdToExpanderGain << ','
          << g.postExpanderGain << ",1,1,2,ENGINEERING_NORMALIZATION\n";
      }
    }
    {
      auto o = file("bbd_internal_level_distribution.csv");
      distributions(o);
    }
    {
      auto o = file("bbd_startup_headroom.csv");
      startup(o);
    }
    std::array<std::array<Result, 11>, 4> results;
    {
      auto o = file("bbd_headroom_tradeoff.csv");
      o << "drive_db,";
      metricHeader(o);
      for (double drive : {-24., -18., -12., -9., -6., -3., 0., 3., 6.})
        for (double level : levels) {
          auto g = profile(0);
          g.compressorToBBDGain = amp(drive);
          g.bbdToExpanderGain = 1 / g.compressorToBBDGain;
          auto r = run(g, level);
          o << drive << ',';
          metric(o, level, r);
        }
    }
    {
      auto o = file("bbd_nonlinearity_headroom.csv"),
           n = file("bbd_noise_headroom.csv"),
           f = file("bbd_snr_distortion_frontier.csv");
      o << "profile,linear_baseline_thd,generated_h2,generated_h3,generated_h4,"
           "generated_h5,";
      metricHeader(o);
      f << "profile,";
      metricHeader(f);
      n << "profile,noise_reference,noise_source_rms,snr_improvement_db,";
      metricHeader(n);
      for (int p = 0; p < 4; ++p)
        for (std::size_t i = 0; i < levels.size(); ++i) {
          auto r = run(profile(p), levels[i]);
          results[p][i] = r;
          auto linear = run(profile(p), levels[i], 0, 0);
          o << names[p] << ',' << linear.clean.thd();
          for (int k = 1; k < 5; ++k)
            o << ','
              << 2 * std::abs(r.clean.h[k] - linear.clean.h[k]) / r.clean.n;
          o << ',';
          metric(o, levels[i], r);
          f << names[p] << ',';
          metric(f, levels[i], r);
          for (int experiment = 0; experiment < 2; ++experiment) {
            double noise =
                1e-4 * (experiment ? profile(p).compressorToBBDGain : 1);
            auto a = experiment ? run(profile(p), levels[i], 1, noise) : r;
            auto b = run(profile(p), levels[i], 1, noise, false);
            double improvement =
                db(a.clean.rms() / std::max(a.noise.rms(), 1e-150)) -
                db(b.clean.rms() / std::max(b.noise.rms(), 1e-150));
            n << names[p] << ','
              << (experiment ? "SNR_ORIENTED_ENGINEERING" : "FIXED_M22_FIXTURE")
              << ',' << noise << ',' << improvement << ',';
            metric(n, levels[i], a);
          }
        }
    }
    {
      auto o = file("bbd_dynamic_range.csv");
      o << "profile,snr_threshold_db,thd_threshold,nominal_threshold,lowest_"
           "tested_db,highest_tested_db,contiguous_span_db,passing_points,"
           "status\n";
      for (int p = 0; p < 4; ++p)
        for (double snr : {20., 40., 60.})
          for (double thd : {.01, .03, .1})
            for (double occupancy : {.95, .99, 1.}) {
              int first = -1, last = -1, bestFirst = -1, bestLast = -1,
                  count = 0;
              double span = -1;
              for (int i = 0; i < 11; ++i) {
                auto r = results[p][i];
                bool ok = db(r.clean.rms() / std::max(r.noise.rms(), 1e-150)) >=
                              snr &&
                          r.clean.thd() <= thd &&
                          double(r.in.nominalCount) / r.in.count >= occupancy;
                if (ok) {
                  ++count;
                  if (first < 0)
                    first = i;
                  last = i;
                  double d = levels[last] - levels[first];
                  if (d > span) {
                    span = d;
                    bestFirst = first;
                    bestLast = last;
                  }
                } else {
                  first = -1;
                  last = -1;
                }
              }
              o << names[p] << ',' << snr << ',' << thd << ',' << occupancy
                << ',';
              if (bestFirst >= 0)
                o << levels[bestFirst] << ',' << levels[bestLast] << ','
                  << span;
              else
                o << ",,";
              o << ',' << count << ','
                << (count ? "DISCRETE_SINE_FIXTURE" : "NO_PASSING_POINTS")
                << '\n';
            }
    }
    {
      auto o = file("bbd_topology_comparison.csv");
      o << "topology,status,compressor_timebase,expander_timebase,"
           "reason\nOutsideFilters,IMPLEMENTED,host_dt,host_dt,"
           "reference\nBBDTerminals,DEFERRED,continuous_filtered_input,"
           "continuous_held_terminal,exact_nonlinear_detector_evolution_"
           "requires_coupled_continuous_solver\n";
    }
    {
      auto o = file("bbd_gain_roundtrip.csv");
      roundtrip(o);
    }
    {
      auto o = file("bbd_clock_headroom.csv");
      clocks(o);
    }
    {
      auto o = file("bbd_feedback_headroom.csv");
      feedback(o);
    }
    {
      auto o = file("bbd_stereo_headroom.csv");
      stereo(o);
    }
    {
      auto o = file("bbd_headroom_performance.csv");
      performance(o);
    }
    {
      auto o = file("README.txt");
      o << "DriftBrigade-M2.6-BBD-Headroom-Qualification\nEngineering "
           "normalized domain; x=1 nominal maximum, no volts. No new product "
           "default.\n48 kHz, 1024 stages, 10 ms, Table1 filters, .47 uF/10k; "
           "nonlinear strength=1; noise RMS=1e-4 per source, "
           "seed=570.\nCapture distributions include intrinsic input noise "
           "where configured; fixed fixture is M2.2 semantics, not measured IC "
           "noise.\nSine frontier: 250ms settle, 250ms measurement, H2-H5 "
           "coherent 500Hz DFT. Paired seeded noisy/clean difference includes "
           "signal-dependent expander response; SNR is effective output "
           "SNR.\nStartup: 250ms onset includes propagation delay. "
           "above_nominal_seconds is capture-weighted sum(1/clock), not exact "
           "analog overload duration.\nUseful band .1-1 and candidate "
           "thresholds are reporting conventions. Dynamic range is largest "
           "contiguous tested sine interval, no interpolation, no universal "
           "musical claim.\nFeedback: external reconstructed output returned "
           "to pre-compressor input with one host observation delay; .1 peak "
           "sine for 100ms then silence to 2s. Late/early RMS windows centered "
           "1s apart. Limit-cycle flag is candidate only (no noise, >1e-8 late "
           "RMS, slope within 1dB/s). H2-H5 are whole-run projections, not "
           "stationary THD.\nPerformance: 5 repeats, median 500ms audio, "
           "stereo/eight cores; detector/event/nonlinear counts analytically "
           "derived from fixed clock. No CI timing threshold.\nBBDTerminals "
           "deferred: sources do not define a detector discretization at "
           "asynchronous captures; updating once per event is a distinct "
           "approximation of continuous rectified filter output.\nProduction "
           "remains DigitalFractionalDelay. See "
           "docs/m2_6_bbd_gain_headroom.md.\n";
    }
    std::cout << "Headroom artifact: " << root << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
