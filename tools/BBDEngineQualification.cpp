#include "../tests/AllocationTracker.h"
#include "../tests/reference_m26/DriftEngine.h"
#include "dsp/DriftEngine.h"
#include <chrono>
#include <complex>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace drift;
namespace {
void require(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
constexpr double rates[] = {44100, 48000, 88200, 96000, 192000};
constexpr std::size_t stages[] = {512, 1024, 2048, 4096};
constexpr std::size_t blocks[] = {1, 17, 32, 64, 127, 256, 511, 1024};
const char *variants[] = {"Wander", "PaperNarrowband", "PhaseDrift"};
const char *stimuli[] = {
    "impulse",    "sine100",   "sine500", "sine3000",         "white",
    "percussive", "harmonic",  "program", "strong_transient", "sine_burst",
    "silence",    "log_sweep", "dc_burst"};
double signal(int kind, int n, double sr) {
  double t = n / sr;
  std::uint32_t x = std::uint32_t(n) + 1;
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  switch (kind) {
  case 0:
    return n == 0 ? 1 : 0;
  case 1:
    return .2 * std::sin(2 * pi * 100 * t);
  case 2:
    return .2 * std::sin(2 * pi * 500 * t);
  case 3:
    return .2 * std::sin(2 * pi * 3000 * t);
  case 4:
    return .2 * (double(x) / 4294967296. * 2 - 1);
  case 5:
    return std::exp(-100 * std::fmod(t, .15)) * std::cos(2 * pi * 700 * t) * .7;
  case 6:
    return .15 * std::sin(2 * pi * 173 * t) + .07 * std::sin(2 * pi * 346 * t) +
           .03 * std::sin(2 * pi * 519 * t);
  case 7:
    return (.2 * std::sin(2 * pi * 173 * t) + .09 * std::sin(2 * pi * 509 * t) +
            .04 * std::sin(2 * pi * 1481 * t)) *
           (.1 + .9 * (1 + std::cos(2 * pi * 3 * t)) * .5);
  case 8:
    return n == 0 ? 4 : 0;
  case 9:
    return t < .1 ? .5 * std::sin(2 * pi * 500 * t) : 0;
  case 11: {
    double duration = 2, lo = 20, hi = 20000, k = std::log(hi / lo) / duration;
    return .2 * std::sin(2 * pi * lo * (std::exp(k * t) - 1) / k);
  }
  case 12:
    return t < 1 ? 1 : 0;
  default:
    return 0;
  }
}
struct Meter {
  double power = 0, sum = 0, peak = 0;
  std::size_t n = 0;
  void add(double x) {
    power += x * x;
    sum += x;
    peak = std::max(peak, std::abs(x));
    ++n;
  }
  double rms() const { return std::sqrt(power / std::max(std::size_t(1), n)); }
  double dc() const { return sum / std::max(std::size_t(1), n); }
};
struct Corr {
  double x = 0, y = 0, xx = 0, yy = 0, xy = 0, n = 0;
  void add(double a, double b) {
    x += a;
    y += b;
    xx += a * a;
    yy += b * b;
    xy += a * b;
    ++n;
  }
  double value() const {
    double d = (xx - x * x / n) * (yy - y * y / n);
    return d > 0 ? (xy - x * y / n) / std::sqrt(d) : 0;
  }
};
struct Observation {
  double maximumEffectiveFeedback = 0;
  double residenceMin = 1e10, residenceMax = 0;
  std::uint64_t transported = 0;
  Meter out[2], bandIn[4], bandOut[4], early, late;
  Meter loopReturn;
  Corr stereo, delays, clocks;
  std::array<std::complex<double>, 5> harmonics{};
  double delayMin = 1e10, delayMax = 0, clockMin = 1e10, clockMax = 0,
         clockSum = 0, maxClockDerivative = 0, lastClock[2][4] = {},
         trackingError = 0;
  std::uint64_t host = 0, events = 0, hidden = 0, guards = 0;
  std::uint32_t maxEvents = 0;
  void add(const DriftEngine &e, const std::array<double, 2> &y, int n,
           double sr, int length) {
    out[0].add(y[0]);
    out[1].add(y[1]);
    stereo.add(y[0], y[1]);
    if (n > length / 4 && n < length / 2)
      early.add(y[0]);
    if (n > length * 3 / 4)
      late.add(y[0]);
    for (int k = 0; k < 5; ++k)
      harmonics[k] += y[0] * std::polar(1., -2 * pi * 500 * (k + 1) * n / sr);
    const auto &t = e.telemetry();
    maximumEffectiveFeedback =
        std::max(maximumEffectiveFeedback, t.effectiveFeedback);
    for (int b = 0; b < 4; ++b) {
      bandIn[b].add(t.bandInput[0][b]);
      bandOut[b].add(t.bandWet[0][b]);
      const auto &l = e.bbdVoice(0, b).signalPath().core.telemetry();
      const auto &r = e.bbdVoice(1, b).signalPath().core.telemetry();
      delays.add(t.delaySeconds[0][b], t.delaySeconds[1][b]);
      clocks.add(l.effectiveClockHz, r.effectiveClockHz);
      for (int ch = 0; ch < 2; ++ch) {
        const auto &v = e.bbdVoice(ch, b);
        loopReturn.add(v.feedbackWet());
        const auto &c = v.signalPath().core.telemetry();
        require(v.finiteState(), "BBD finite state");
        delayMin = std::min(delayMin, c.effectiveDelaySeconds);
        delayMax = std::max(delayMax, c.effectiveDelaySeconds);
        clockMin = std::min(clockMin, c.effectiveClockHz);
        clockMax = std::max(clockMax, c.effectiveClockHz);
        clockSum += c.effectiveClockHz;
        maxEvents = std::max(maxEvents, c.eventsThisHostSample);
        events += c.eventsThisHostSample;
        trackingError =
            std::max(trackingError, std::abs(c.effectiveClockHz -
                                             double(c.stageCount) /
                                                 (2 * t.delaySeconds[ch][b])));
        if (host)
          maxClockDerivative =
              std::max(maxClockDerivative,
                       std::abs(c.effectiveClockHz - lastClock[ch][b]) * sr);
        lastClock[ch][b] = c.effectiveClockHz;
        require(!c.wasClamped, "hidden core delay clamp");
      }
    }
    ++host;
  }
  void finish(const DriftEngine &e) {
    for (int ch = 0; ch < 2; ++ch)
      for (int b = 0; b < 4; ++b) {
        const auto &stats = e.bbdVoice(ch, b).signalPath().core.operatingStats;
        transported += stats.transportedCount;
        if (stats.transportedCount) {
          residenceMin = std::min(residenceMin, stats.minimumBucketResidence);
          residenceMax = std::max(residenceMax, stats.maximumBucketResidence);
        }
        hidden += e.bbdVoice(ch, b).hiddenClamps;
        guards += e.bbdVoice(ch, b).numericalGuards;
      }
    if (!transported)
      residenceMin = residenceMax = std::numeric_limits<double>::quiet_NaN();
    require(hidden == 0 && guards == 0, "hidden clamp or numerical guard");
  }
};
void configure(DriftEngine &e, DelayBackend backend, const EngineParameters &p,
               double sr, std::size_t count = 1024, int variant = 0,
               BankMode bank = BankMode::Gentle,
               BBDVoiceConfig config = BBDVoiceConfig::fullResearchFixture()) {
  config.physicalStages = count;
  require(e.setDelayBackend(backend, config), "pre-prepare configuration");
  e.setParameters(p);
  e.setOrganicVariant(static_cast<OrganicVariant>(variant));
  e.setBankMode(bank);
  e.prepare(sr, 77);
}
Observation render(DriftEngine &e, double sr, int kind, int length,
                   bool mono = false, std::ostream *voiceReport = nullptr,
                   int variant = 0, double motion = 0) {
  Observation o;
  for (int n = 0; n < length; ++n) {
    double x = signal(kind, n, sr);
    auto y = e.processSample(x, mono ? x : signal(kind, n + 29, sr), mono);
    o.add(e, y, n, sr, length);
    if (voiceReport && n % 2048 == 0)
      for (int ch = 0; ch < 2; ++ch)
        for (int b = 0; b < 4; ++b) {
          const auto &path = e.bbdVoice(ch, b).signalPath();
          const auto &t = path.core.telemetry();
          const auto &s = path.core.operatingStats;
          *voiceReport << variants[variant] << ',' << motion << ',' << n << ','
                       << ch << ',' << b << ',' << t.requestedDelaySeconds
                       << ',' << t.effectiveDelaySeconds << ','
                       << t.effectiveClockHz << ',' << t.eventsThisHostSample
                       << ','
                       << std::sqrt(s.sumSquares /
                                    std::max(std::uint64_t(1), s.count))
                       << ',' << s.peak << ','
                       << double(s.nominalCount) /
                              std::max(std::uint64_t(1), s.count)
                       << ',' << path.compressor.levelAverager().value() << ','
                       << path.expander.levelAverager().value() << ','
                       << path.core.heldOutput() << '\n';
        }
  }
  o.finish(e);
  return o;
}
std::ofstream file(const std::filesystem::path &root, const char *name,
                   const char *header) {
  std::ofstream f(root / name);
  f.exceptions(std::ios::failbit | std::ios::badbit);
  f.precision(17);
  f << std::unitbuf; // Fail on writes, including buffered stream failures.
  f << header << '\n';
  return f;
}
void regression(std::ostream *report = nullptr) {
  for (auto bank : {BankMode::Gentle, BankMode::Selective})
    for (int variant = 0; variant < 3; ++variant)
      for (bool mono : {false, true}) {
        DriftEngine e;
        PreM27DriftEngine old;
        EngineParameters p;
        configure(e, DelayBackend::DigitalFractional, p, 48000, 1024, variant,
                  bank);
        old.setParameters(p);
        old.setBankMode(bank);
        old.setOrganicVariant(static_cast<OrganicVariant>(variant));
        old.prepare(48000, 77);
        auto before = allocations.load();
        for (int n = 0; n < 12000; ++n) {
          if (n % 997 == 0) {
            for (std::size_t i = 0; i < Count; ++i)
              p.values[i] =
                  parameterSpecs[i].minimum +
                  (parameterSpecs[i].maximum - parameterSpecs[i].minimum) *
                      (.5 + .5 * std::sin(n * .003 + i));
            e.setParameters(p);
            old.setParameters(p);
          }
          auto a = e.processSample(signal(7, n, 48000), signal(4, n, 48000),
                                   mono),
               b = old.processSample(signal(7, n, 48000), signal(4, n, 48000),
                                     mono);
          require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
                  "digital frozen engine bit identity");
        }
        require(before == allocations.load(), "digital process allocations");
        if (report)
          *report << int(bank) << ',' << variants[variant] << ',' << mono
                  << ",12000,0,BIT_IDENTICAL\n";
      }
}
void invariance(std::ostream *report = nullptr) {
  for (auto backend :
       {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD})
    for (bool mono : {false, true}) {
      std::vector<float> referenceL, referenceR;
      for (auto block : blocks) {
        DriftEngine e;
        EngineParameters p;
        configure(e, backend, p, 48000);
        std::vector<float> l(8192), r(8192);
        for (int n = 0; n < 8192; ++n) {
          l[n] = float(signal(7, n, 48000));
          r[n] = float(signal(4, n, 48000));
        }
        auto before = allocations.load();
        for (std::size_t offset = 0; offset < l.size(); offset += block) {
          float *channels[] = {l.data() + offset, r.data() + offset};
          e.process(channels, mono ? 1 : 2, std::min(block, l.size() - offset));
        }
        require(before == allocations.load(), "block process allocations");
        if (referenceL.empty()) {
          referenceL = l;
          referenceR = r;
        } else {
          require(std::memcmp(l.data(), referenceL.data(),
                              l.size() * sizeof(float)) == 0 &&
                      std::memcmp(r.data(), referenceR.data(),
                                  r.size() * sizeof(float)) == 0,
                  "block bit identity");
        }
        e.reset(77);
        for (int n = 0; n < 8192; ++n) {
          auto y = e.processSample(float(signal(7, n, 48000)),
                                   float(signal(4, n, 48000)), mono);
          const float sample[] = {float(y[0]), float(y[1])};
          require(
              std::memcmp(sample, &l[n], sizeof(float)) == 0 &&
                  (mono || std::memcmp(sample + 1, &r[n], sizeof(float)) == 0),
              "reset bit identity");
        }
        if (report)
          *report << int(backend) << ',' << mono << ',' << block
                  << ",0,0,BIT_IDENTICAL\n";
      }
    }
}
void tests() {
  regression();
  invariance();
  {
    DriftEngine e;
    EngineParameters p;
    configure(e, DelayBackend::ExperimentalBBD, p, 48000);
    std::array<std::array<double, 2>, 512> expected;
    for (int n = 0; n < 512; ++n)
      expected[n] = e.processSample(signal(7, n, 48000), signal(4, n, 48000));
    std::array<std::array<std::complex<double>, 5>, 8> inState, outState;
    std::array<double, 8> detector, held;
    std::array<std::array<double, 512>, 8> buckets;
    for (int i = 0; i < 8; ++i) {
      const auto &path = e.bbdVoice(i / 4, i % 4).signalPath();
      inState[i] = path.core.inputFilterState();
      outState[i] = path.core.outputFilterState();
      detector[i] = path.compressor.currentGain();
      held[i] = path.core.heldOutput();
      for (int b = 0; b < 512; ++b)
        buckets[i][b] = path.core.stageValue(b);
    }
    e.prepare(48000, 77);
    const auto before = allocations.load();
    for (int n = 0; n < 512; ++n)
      require(e.processSample(signal(7, n, 48000), signal(4, n, 48000)) ==
                  expected[n],
              "reprepare output identity");
    require(before == allocations.load(), "reprepare callback allocations");
    for (int i = 0; i < 8; ++i) {
      const auto &path = e.bbdVoice(i / 4, i % 4).signalPath();
      require(inState[i] == path.core.inputFilterState() &&
                  outState[i] == path.core.outputFilterState() &&
                  detector[i] == path.compressor.currentGain() &&
                  held[i] == path.core.heldOutput(),
              "reprepare filter/detector/hold state");
      for (int b = 0; b < 512; ++b)
        require(buckets[i][b] == path.core.stageValue(b), "reprepare buckets");
    }
    e.reset(78);
    bool different = false;
    for (int n = 0; n < 512; ++n)
      different |= e.processSample(signal(7, n, 48000), signal(4, n, 48000)) !=
                   expected[n];
    require(different, "different reset seed changes histories");
    e.reset(77);
    for (int n = 0; n < 512; ++n)
      require(e.processSample(signal(7, n, 48000), signal(4, n, 48000)) ==
                  expected[n],
              "restored seed output identity");
  }
  for (double sr : rates)
    for (auto count : stages)
      for (int variant = 0; variant < 3; ++variant) {
        DriftEngine e;
        EngineParameters p;
        p[Center] = .3;
        p[Depth] = 1;
        p[Motion] = 10;
        p[Feedback] = .65;
        p[Dynamics] = 1;
        configure(e, DelayBackend::ExperimentalBBD, p, sr, count, variant);
        require(!e.setDelayBackend(DelayBackend::DigitalFractional),
                "runtime switching prohibited");
        auto before = allocations.load();
        auto o = render(e, sr, 8, 512);
        require(before == allocations.load(), "BBD process allocations");
        require(o.trackingError == 0, "per-sample clock tracking");
      }
  for (int fixture = 0; fixture < 4; ++fixture) {
    DriftEngine e;
    EngineParameters p;
    p[Width] = 0;
    auto c = BBDVoiceConfig::fullResearchFixture();
    if (fixture == 0)
      c = BBDVoiceConfig::linearReference();
    if (fixture == 1)
      c.character.inputNoiseRms = c.character.outputNoiseRms = 0;
    if (fixture == 2)
      c.compander.enabled = false;
    configure(e, DelayBackend::ExperimentalBBD, p, 48000, 1024, 0,
              BankMode::Gentle, c);
    auto before = allocations.load();
    for (int n = 0; n < 4096; ++n) {
      auto y = e.processSample(signal(7, n, 48000), signal(7, n, 48000));
      require(y[0] == y[1], "Width zero mono compatibility");
    }
    require(before == allocations.load(), "mono allocations");
  }
  for (auto backend :
       {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD})
    for (bool mono : {false, true}) {
      DriftEngine e;
      EngineParameters p;
      p[Mix] = 0;
      configure(e, backend, p, 48000);
      for (int n = 0; n < 1024; ++n) {
        double x = signal(7, n, 48000), r = signal(4, n, 48000);
        auto y = e.processSample(x, r, mono);
        require(y[0] == x && y[1] == (mono ? x : r), "Mix zero dry identity");
      }
    }
  {
    DriftEngine e;
    EngineParameters p;
    p[Depth] = 0;
    p[Chaos] = 1;
    p[Motion] = 10;
    configure(e, DelayBackend::ExperimentalBBD, p, 48000);
    double clock = 0;
    for (int n = 0; n < 2048; ++n) {
      e.processSample(0, 0);
      double c =
          e.bbdVoice(0, 0).signalPath().core.telemetry().effectiveClockHz;
      if (n)
        require(c == clock, "Depth zero clock constant");
      clock = c;
    }
    const auto &stats = e.bbdVoice(0, 0).signalPath().core.operatingStats;
    const double residence = 1023. / (2 * clock);
    require(stats.transportedCount > 0 &&
                std::abs(stats.minimumBucketResidence - residence) < 1e-11 &&
                std::abs(stats.maximumBucketResidence - residence) < 1e-11,
            "fixed clock bucket residence");
  }
  {
    DriftEngine measured, plain;
    EngineParameters p;
    configure(measured, DelayBackend::ExperimentalBBD, p, 48000);
    configure(plain, DelayBackend::ExperimentalBBD, p, 48000);
    plain.enableBBDOperatingInstrumentation(false);
    for (int n = 0; n < 2048; ++n) {
      auto a = measured.processSample(signal(7, n, 48000), signal(4, n, 48000));
      auto b = plain.processSample(signal(7, n, 48000), signal(4, n, 48000));
      require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
              "instrumentation audio identity");
    }
  }
  for (double feedback : {0., .25, .5, .65, .75})
    for (int stimulus : {0, 7, 8, 9, 12}) {
      DriftEngine e;
      EngineParameters p;
      p[Feedback] = std::min(.65, feedback);
      p[Dynamics] = feedback == .75 ? 1 : 0;
      p[Mix] = 1;
      configure(e, DelayBackend::ExperimentalBBD, p, 48000);
      render(e, 48000, stimulus, 24000, true);
    }
  // .75 is a guard ceiling, not reachable through the current .65+.08 macros.
  for (int kind : {0, 7, 8, 9}) {
    BBDModulatedDelayVoice voice;
    voice.prepare(48000, BBDVoiceConfig::fullResearchFixture());
    const auto before = allocations.load();
    for (int n = 0; n < 48000; ++n) {
      const double y = voice.process(signal(kind, n, 48000), .008, .75);
      require(std::isfinite(y) && voice.finiteState(),
              "direct .75 voice finite");
    }
    require(voice.hiddenClamps == 0 && voice.numericalGuards == 0,
            "direct .75 voice guards");
    require(before == allocations.load(), "direct .75 allocations");
  }
  // Center automation reaches a physically impossible requested center for
  // 4096 stages at 44.1 kHz; the engine, including slew history, must admit it.
  {
    DriftEngine e;
    EngineParameters p;
    configure(e, DelayBackend::ExperimentalBBD, p, 44100, 4096);
    double previous[2][4] = {};
    for (int n = 0; n < 12000; ++n) {
      if (n == 1000) {
        p[Center] = .3;
        p[Depth] = 1;
        e.setParameters(p);
      }
      if (n == 7000) {
        p[Center] = 30;
        e.setParameters(p);
      }
      e.processSample(signal(7, n, 44100), signal(4, n, 44100));
      for (int ch = 0; ch < 2; ++ch)
        for (int b = 0; b < 4; ++b) {
          const double d = e.telemetry().delaySeconds[ch][b];
          require(d >= e.minimumDelaySeconds() && d <= e.maximumDelaySeconds(),
                  "admitted slew history");
          if (n)
            require(std::abs(d - previous[ch][b]) <= .25 / 44100 + 1e-17,
                    "quarter-sample slew");
          require(!e.bbdVoice(ch, b).signalPath().core.telemetry().wasClamped,
                  "automation core clamp");
          previous[ch][b] = d;
        }
    }
  }
  std::cout << "M2.7: digital bit identity, finite state, zero hidden "
               "clamps/guards, mono, reset, block identity, dry identity, "
               "constant clock and allocation checks passed\n";
}
void spectral(const std::filesystem::path &root) {
  auto f =
      file(root, "bbd_engine_spectral.csv",
           "bank,backend,hz,clean_rms,wet_gain,transfer_real,transfer_imag,thd_"
           "h2_h5,paired_noise_rms,snr_db,settle_s,observation_s");
  for (auto bank : {BankMode::Gentle, BankMode::Selective})
    for (auto backend :
         {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD})
      for (double hz : {0., 100., 500., 3000.}) {
        DriftEngine clean, noisy;
        EngineParameters p;
        p[Depth] = 0;
        p[Feedback] = 0;
        p[Dynamics] = 0;
        p[Mix] = 1;
        p[Width] = 0;
        auto c = BBDVoiceConfig::fullResearchFixture();
        c.character.inputNoiseRms = c.character.outputNoiseRms = 0;
        configure(clean, backend, p, 48000, 1024, 0, bank, c);
        configure(noisy, backend, p, 48000, 1024, 0, bank);
        Meter output, noise;
        std::complex<double> input{}, h[5] = {};
        for (int n = 0; n < 48000; ++n) {
          double x = hz == 0 ? 0 : .2 * std::sin(2 * pi * hz * n / 48000);
          auto a = clean.processSample(x, x, true),
               b = noisy.processSample(x, x, true);
          if (n >= 24000) {
            output.add(a[0]);
            noise.add(b[0] - a[0]);
            for (int k = 0; k < 5; ++k)
              h[k] += a[0] * std::polar(1., -2 * pi * hz * (k + 1) * n / 48000);
            input += x * std::polar(1., -2 * pi * hz * n / 48000);
          }
        }
        double power = 0;
        for (int k = 1; k < 5; ++k)
          power += std::norm(h[k]);
        auto transfer = hz == 0 ? std::complex<double>{} : h[0] / input;
        const double snr = hz == 0 ? std::numeric_limits<double>::quiet_NaN()
                           : noise.rms() == 0
                               ? std::numeric_limits<double>::infinity()
                               : 20 * std::log10(output.rms() / noise.rms());
        f << int(bank) << ',' << int(backend) << ',' << hz << ','
          << output.rms() << ',' << output.rms() / (.2 / std::sqrt(2.)) << ','
          << transfer.real() << ',' << transfer.imag() << ','
          << (hz == 0 ? 0 : std::sqrt(power) / std::max(1e-150, std::abs(h[0])))
          << ',' << noise.rms() << ',' << snr << ",.5,.5\n";
      }
  auto feedback =
      file(root, "bbd_engine_voice_feedback.csv",
           "feedback,stimulus,peak,rms,dc,early_rms,late_rms,internal_peak,"
           "nominal_occupancy,hidden_clamps,numerical_guards");
  for (int kind : {0, 7, 8, 9, 10}) {
    BBDModulatedDelayVoice v;
    v.prepare(48000, BBDVoiceConfig::fullResearchFixture());
    Meter m, early, late;
    for (int n = 0; n < 96000; ++n) {
      double y = v.process(signal(kind, n, 48000), .008, .75);
      require(v.finiteState(), "direct feedback state");
      m.add(y);
      if (n > 24000 && n < 48000)
        early.add(y);
      if (n > 72000)
        late.add(y);
    }
    const auto &s = v.signalPath().core.operatingStats;
    require(v.hiddenClamps == 0 && v.numericalGuards == 0,
            "direct feedback guards");
    feedback << ".75," << stimuli[kind] << ',' << m.peak << ',' << m.rms()
             << ',' << m.dc() << ',' << early.rms() << ',' << late.rms() << ','
             << s.peak << ',' << double(s.nominalCount) / s.count << ','
             << v.hiddenClamps << ',' << v.numericalGuards << '\n';
  }
}
void stageFeasibility(const std::filesystem::path &root) {
  auto f = file(
      root, "bbd_engine_stage_feasibility.csv",
      "rate,stages,min_delay_ms,max_core_delay_s,min_center_ms,"
      "requested_events_at_min_center,status,unrestricted_request_status,all_"
      "centers_supported,pre_m27_conservative_trajectory_min_ms,events_at_"
      "pre_m27_envelope_min");
  for (double sr : rates)
    for (auto count : stages) {
      double min = double(count) / (sr * 128);
      // Right modulation has three nonnegative coefficients whose squared
      // sum is one. Cauchy-Schwarz bounds their sum by sqrt(3), tighter than
      // the engine's deliberately conservative 2*source magnitude bound.
      const double preMinimum =
          .0003 - .85 * (.0003 - 4 / sr) * std::sqrt(3.) / 2;
      f << sr << ',' << count << ',' << min * 1000 << ',' << count / 2.
        << ",0.3," << count / (sr * .0003) << ','
        << (min <= preMinimum ? "FULL_RANGE" : "LIMITED_SHORT_DELAY") << ','
        << (min <= preMinimum ? "FULL_RANGE" : "EVENT_LIMIT_EXCEEDED") << ','
        << (min <= .0003) << ',' << preMinimum * 1000 << ','
        << count / (sr * preMinimum) << '\n';
    }
}
void benchmark(const std::filesystem::path &root) {
  auto f = file(root, "bbd_engine_cpu.csv",
                "rate,stages,block,heavy,backend,seconds_per_audio_second,"
                "realtime_factor,max_events,total_events_per_second,clock_"
                "average,clock_min,clock_max,ratio_vs_digital");
  for (double sr : {44100., 48000., 96000.})
    for (auto count : {512u, 1024u, 2048u})
      for (int block : {32, 64, 128, 256, 512})
        for (bool heavy : {false, true}) {
          double digitalTime = 0;
          for (auto backend : {DelayBackend::DigitalFractional,
                               DelayBackend::ExperimentalBBD}) {
            EngineParameters p;
            if (heavy) {
              p[Depth] = 1;
              p[Motion] = 10;
              p[Chaos] = 1;
              p[Feedback] = .65;
              p[Dynamics] = 1;
            }
            DriftEngine e;
            configure(e, backend, p, sr, count);
            e.enableBBDOperatingInstrumentation(false);
            std::vector<float> l(block), r(block);
            int total = int(sr * .25), done = 0;
            std::uint64_t events = 0;
            std::uint32_t maxEvents = 0;
            double clockSum = 0, clockMin = 1e10, clockMax = 0;
            for (int n = 0; n < 1024; ++n)
              e.processSample(signal(7, n, sr), signal(4, n, sr));
            double seconds = 0;
            while (done < total) {
              int size = std::min(block, total - done);
              for (int n = 0; n < size; ++n) {
                l[n] = float(signal(7, done + n, sr));
                r[n] = float(signal(4, done + n, sr));
              }
              float *channels[] = {l.data(), r.data()};
              auto start = std::chrono::steady_clock::now();
              e.process(channels, 2, size);
              seconds += std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - start)
                             .count();
              done += size;
              if (backend == DelayBackend::ExperimentalBBD)
                for (int ch = 0; ch < 2; ++ch)
                  for (int b = 0; b < 4; ++b) {
                    auto &t = e.bbdVoice(ch, b).signalPath().core.telemetry();
                    maxEvents = std::max(maxEvents, t.eventsThisHostSample);
                    clockSum += t.effectiveClockHz * size;
                    clockMin = std::min(clockMin, t.effectiveClockHz);
                    clockMax = std::max(clockMax, t.effectiveClockHz);
                  }
            }
            if (backend == DelayBackend::ExperimentalBBD)
              for (int ch = 0; ch < 2; ++ch)
                for (int b = 0; b < 4; ++b)
                  events += e.bbdVoice(ch, b)
                                .signalPath()
                                .core.telemetry()
                                .totalEventCount;
            if (backend == DelayBackend::ExperimentalBBD) {
              const auto &t = e.telemetry();
              maxEvents = t.maximumBBDEventsPerSample;
              clockMin = t.minimumBBDClock;
              clockMax = t.maximumBBDClock;
              clockSum = t.accumulatedBBDClock * total / (total + 1024);
            }
            double factor = seconds / (total / sr);
            if (backend == DelayBackend::DigitalFractional)
              digitalTime = factor;
            f << sr << ',' << count << ',' << block << ',' << heavy << ','
              << int(backend) << ',' << factor << ',' << 1 / factor << ','
              << maxEvents << ',' << events / ((total + 1024) / sr) << ','
              << clockSum / (8 * total) << ','
              << (backend == DelayBackend::ExperimentalBBD ? clockMin : 0)
              << ',' << clockMax << ',' << factor / digitalTime << '\n';
          }
        }
}
void feedbackQualification(const std::filesystem::path &root) {
  auto f = file(root, "bbd_engine_feedback.csv",
                "requested_effective_feedback,stimulus,peak,rms,dc,early_rms,"
                "late_rms,late_early_db,h2,h3,h4,h5,internal_peak,min_"
                "occupancy,compressor_detector,expander_detector,observed_"
                "effective_feedback,hidden_clamps,numerical_guards,finite,"
                "noise_enabled,maximum_effective_feedback,per_voice_loop_peak,"
                "pooled_voice_loop_rms,pooled_voice_loop_dc");
  for (bool noiseEnabled : {false, true})
    for (double fb : {0., .25, .5, .65, .75})
      for (int kind : {0, 7, 8, 9, 10, 12}) {
        DriftEngine e;
        EngineParameters p;
        p[Feedback] = std::min(.65, fb);
        p[Dynamics] = fb == .75 ? 1 : 0;
        p[Mix] = 1;
        auto config = BBDVoiceConfig::fullResearchFixture();
        if (!noiseEnabled)
          config.character.inputNoiseRms = config.character.outputNoiseRms = 0;
        configure(e, DelayBackend::ExperimentalBBD, p, 48000, 1024, 0,
                  BankMode::Gentle, config);
        auto o = render(e, 48000, kind, 96000, true);
        double peak = 0, occupancy = 1;
        for (int b = 0; b < 4; ++b) {
          const auto &s = e.bbdVoice(0, b).signalPath().core.operatingStats;
          peak = std::max(peak, s.peak);
          if (s.count)
            occupancy = std::min(occupancy, double(s.nominalCount) / s.count);
        }
        const auto &path = e.bbdVoice(0, 0).signalPath();
        f << fb << ',' << stimuli[kind] << ',' << o.out[0].peak << ','
          << o.out[0].rms() << ',' << o.out[0].dc() << ',' << o.early.rms()
          << ',' << o.late.rms() << ','
          << 20 * std::log10(std::max(1e-150, o.late.rms()) /
                             std::max(1e-150, o.early.rms()));
        for (int k = 1; k < 5; ++k)
          f << ',' << 2 * std::abs(o.harmonics[k]) / o.host;
        f << ',' << peak << ',' << occupancy << ','
          << path.compressor.levelAverager().value() << ','
          << path.expander.levelAverager().value() << ','
          << e.telemetry().effectiveFeedback << ',' << o.hidden << ','
          << o.guards << ",1," << noiseEnabled << ','
          << o.maximumEffectiveFeedback << ',' << o.loopReturn.peak << ','
          << o.loopReturn.rms() << ',' << o.loopReturn.dc() << '\n';
      }
}
void qualification(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  {
    auto f = file(root, "bbd_engine_backend_regression.csv",
                  "bank,variant,mono,samples,max_error,result");
    regression(&f);
  }
  {
    auto f = file(root, "bbd_engine_block_invariance.csv",
                  "backend,mono,block,max_error,reset_error,result");
    invariance(&f);
  }
  stageFeasibility(root);
  {
    auto f = file(root, "bbd_engine_delay_range.csv",
                  "rate,stages,motion,depth,center_ms,requested_excursion_s,"
                  "limited_excursion_s,actual_min_s,actual_max_s,hidden_clamps,"
                  "clock_min,clock_max,max_events,events_per_second,limited_"
                  "sample_percent,observation_samples,pre_m27_excursion_s,"
                  "extra_excursion_reduction_s,bucket_residence_min_s,bucket_"
                  "residence_max_s,transported_count");
    for (double sr : rates)
      for (auto count : stages)
        for (double motion : {.05, .2, .7, 2., 6., 10.})
          for (double depth : {0., .25, .5, .75, 1.})
            for (double center : {.3, .5, 1., 2., 5., 10., 20., 30.}) {
              DriftEngine e;
              EngineParameters p;
              p[Motion] = motion;
              p[Depth] = depth;
              p[Center] = center;
              configure(e, DelayBackend::ExperimentalBBD, p, sr, count);
              auto o = render(e, sr, 10, 256);
              const auto &t = e.telemetry();
              PreM27DriftEngine old;
              old.setParameters(p);
              old.prepare(sr, 77);
              old.processSample(0, 0);
              const double originalExcursion =
                  old.telemetry().actualExcursionSeconds;
              f << sr << ',' << count << ',' << motion << ',' << depth << ','
                << center << ',' << t.requestedExcursionSeconds << ','
                << t.actualExcursionSeconds << ',' << o.delayMin << ','
                << o.delayMax << ',' << o.hidden << ',' << o.clockMin << ','
                << o.clockMax << ',' << o.maxEvents << ','
                << o.events / (o.host / sr) << ','
                << 100. * t.physicalLimitSamples / o.host << ',' << o.host
                << ',' << originalExcursion << ','
                << std::max(0., originalExcursion - t.actualExcursionSeconds)
                << ',' << o.residenceMin << ',' << o.residenceMax << ','
                << o.transported << '\n';
            }
  }
  {
    auto f = file(root, "bbd_engine_modulation_tracking.csv",
                  "variant,rate,stages,delay_min_s,delay_max_s,clock_min,clock_"
                  "max,max_events,tracking_error_hz,max_clock_derivative_hz_"
                  "per_s,hidden_clamps,observation_s,motion_hz,bucket_"
                  "residence_min_s,bucket_residence_max_s,transported_count");
    auto voices = file(
        root, "bbd_engine_voice_telemetry.csv",
        "variant,motion_hz,sample,channel,band,requested_delay_s,effective_"
        "delay_s,clock_hz,events,internal_rms,internal_peak,nominal_occupancy,"
        "compressor_detector,expander_detector,held_output");
    for (int v = 0; v < 3; ++v)
      for (double motion : {.05, .2, .7, 2., 6., 10.}) {
        DriftEngine e;
        EngineParameters p;
        p[Motion] = motion;
        p[Depth] = 1;
        p[Chaos] = 1;
        configure(e, DelayBackend::ExperimentalBBD, p, 48000, 1024, v);
        const double duration = std::max(2., 2 / motion);
        auto o = render(e, 48000, 7, int(48000 * duration), false, &voices, v,
                        motion);
        f << variants[v] << ",48000,1024," << o.delayMin << ',' << o.delayMax
          << ',' << o.clockMin << ',' << o.clockMax << ',' << o.maxEvents << ','
          << o.trackingError << ',' << o.maxClockDerivative << ',' << o.hidden
          << ',' << duration << ',' << motion << ',' << o.residenceMin << ','
          << o.residenceMax << ',' << o.transported << '\n';
      }
  }
  feedbackQualification(root);
  {
    auto f = file(root, "bbd_engine_noise_correlation.csv",
                  "width,voice_a,voice_b,seed_a,seed_b,correlation,maximum_"
                  "pair_difference");
    for (double width : {0., .6}) {
      DriftEngine e;
      EngineParameters p;
      p[Width] = width;
      p[Mix] = 1;
      p[Depth] = width == 0 ? 0 : .5;
      configure(e, DelayBackend::ExperimentalBBD, p, 48000);
      Corr c[8][8];
      double diff[8][8] = {};
      for (int n = 0; n < 48000; ++n) {
        e.processSample(0, 0);
        if (n < 12000)
          continue;
        double x[8];
        for (int i = 0; i < 8; ++i)
          x[i] = e.telemetry().bandWet[i / 4][i % 4];
        for (int i = 0; i < 8; ++i)
          for (int j = 0; j < 8; ++j) {
            c[i][j].add(x[i], x[j]);
            diff[i][j] = std::max(diff[i][j], std::abs(x[i] - x[j]));
          }
      }
      for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j) {
          if (width == 0 && i % 4 == j % 4)
            require(diff[i][j] == 0, "matched band noise equality");
          if (i % 4 != j % 4)
            require(diff[i][j] > 0, "distinct band noise streams");
          f << width << ',' << i << ',' << j << ','
            << bbdBandSeed(77 ^ 570, i % 4) << ','
            << bbdBandSeed(77 ^ 570, j % 4) << ',' << c[i][j].value() << ','
            << diff[i][j] << '\n';
        }
    }
  }
  {
    auto f = file(
        root, "bbd_engine_stereo.csv",
        "coherence,width,delay_correlation,clock_correlation,wet_correlation,"
        "silence_wet_correlation,left_rms,right_rms,left_peak,right_peak");
    for (double coherence : {0., .25, .5, .75, 1.})
      for (double width : {0., .25, .5, .75, 1.}) {
        DriftEngine e, noise;
        EngineParameters p;
        p[Coherence] = coherence;
        p[Width] = width;
        p[Mix] = 1;
        configure(e, DelayBackend::ExperimentalBBD, p, 48000);
        configure(noise, DelayBackend::ExperimentalBBD, p, 48000);
        auto o = render(e, 48000, 7, 12000),
             s = render(noise, 48000, 10, 12000);
        f << coherence << ',' << width << ',' << o.delays.value() << ','
          << o.clocks.value() << ',' << o.stereo.value() << ','
          << s.stereo.value() << ',' << o.out[0].rms() << ',' << o.out[1].rms()
          << ',' << o.out[0].peak << ',' << o.out[1].peak << '\n';
      }
  }
  {
    auto f = file(root, "bbd_engine_dynamics.csv",
                  "dynamics,effective_chaos,effective_feedback,wet_prominence,"
                  "rms,macro_error_vs_digital");
    for (double d : {0., .5, 1.}) {
      DriftEngine e, digital;
      EngineParameters p;
      p[Dynamics] = d;
      configure(e, DelayBackend::ExperimentalBBD, p, 48000);
      configure(digital, DelayBackend::DigitalFractional, p, 48000);
      double error = 0;
      Meter m;
      for (int n = 0; n < 12000; ++n) {
        auto y = e.processSample(signal(7, n, 48000), signal(7, n, 48000));
        digital.processSample(signal(7, n, 48000), signal(7, n, 48000));
        const auto &a = e.telemetry();
        const auto &b = digital.telemetry();
        error = std::max({error, std::abs(a.effectiveChaos - b.effectiveChaos),
                          std::abs(a.effectiveFeedback - b.effectiveFeedback),
                          std::abs(a.wetProminence - b.wetProminence)});
        m.add(y[0]);
      }
      require(error == 0, "Dynamics macro preservation");
      const auto &t = e.telemetry();
      f << d << ',' << t.effectiveChaos << ',' << t.effectiveFeedback << ','
        << t.wetProminence << ',' << m.rms() << ',' << error << '\n';
    }
  }
  {
    auto f = file(
        root, "bbd_engine_band_response.csv",
        "bank,stimulus,band,input_rms,bbd_output_rms,summed_wet_rms,summed_wet_"
        "peak,summed_dc,transfer_real,transfer_imag,observation_s");
    for (auto bank : {BankMode::Gentle, BankMode::Selective})
      for (int kind : {0, 1, 2, 3, 4, 5, 6, 7, 11}) {
        DriftEngine e;
        EngineParameters p;
        p[Depth] = 0;
        p[Feedback] = 0;
        p[Dynamics] = 0;
        p[Mix] = 1;
        configure(e, DelayBackend::ExperimentalBBD, p, 48000, 1024, 0, bank);
        auto o = render(e, 48000, kind, kind == 11 ? 96000 : 24000, true);
        for (int b = 0; b < 4; ++b)
          f << int(bank) << ',' << stimuli[kind] << ',' << b << ','
            << o.bandIn[b].rms() << ',' << o.bandOut[b].rms() << ','
            << o.out[0].rms() << ',' << o.out[0].peak << ',' << o.out[0].dc()
            << ",nan,nan," << (kind == 11 ? 2 : .5) << '\n';
      }
    for (auto bank : {BankMode::Gentle, BankMode::Selective})
      for (double hz : {100., 250., 500., 1000., 2000., 3000., 4000., 8000.,
                        12000., 18000.}) {
        DriftEngine e;
        EngineParameters p;
        p[Depth] = 0;
        p[Feedback] = 0;
        p[Dynamics] = 0;
        p[Mix] = 1;
        configure(e, DelayBackend::ExperimentalBBD, p, 48000, 1024, 0, bank);
        std::complex<double> x{}, y{};
        for (int n = 0; n < 48000; ++n) {
          double in = .2 * std::sin(2 * pi * hz * n / 48000);
          auto out = e.processSample(in, in, true);
          if (n >= 24000) {
            auto phase = std::polar(1., -2 * pi * hz * n / 48000);
            x += in * phase;
            y += out[0] * phase;
          }
        }
        auto h = y / x;
        f << int(bank) << ",sine_" << hz << ",-1,nan,nan,nan,nan,nan,"
          << h.real() << ',' << h.imag() << ",1\n";
      }
  }
  {
    auto f = file(root, "bbd_engine_digital_vs_bbd.csv",
                  "backend,stimulus,rms,peak,dc,wet_stereo_correlation,h2_h5_"
                  "projection_ratio,observation_s");
    for (auto backend :
         {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD})
      for (int kind = 0; kind < 11; ++kind) {
        DriftEngine e;
        EngineParameters p;
        p[Mix] = 1;
        configure(e, backend, p, 48000);
        Meter m;
        Corr c;
        std::complex<double> h[5] = {};
        for (int n = 0; n < 24000; ++n) {
          auto y = e.processSample(signal(kind, n, 48000),
                                   signal(kind, n + 29, 48000));
          m.add(y[0]);
          c.add(y[0], y[1]);
          for (int k = 0; k < 5; ++k)
            h[k] += y[0] * std::polar(1., -2 * pi * 500 * (k + 1) * n / 48000);
        }
        double power = 0;
        for (int k = 1; k < 5; ++k)
          power += std::norm(h[k]);
        f << int(backend) << ',' << stimuli[kind] << ',' << m.rms() << ','
          << m.peak << ',' << m.dc() << ',' << c.value() << ','
          << std::sqrt(power) / std::max(1e-150, std::abs(h[0])) << ",.5\n";
      }
  }
  benchmark(root);
  {
    auto f =
        file(root, "README.txt", "DriftBrigade-M2.7-BBD-Engine-Qualification");
    f << "QUALIFICATION FIXTURE / ENGINEERING NORMALIZATION / NOT PRODUCT "
         "DEFAULT.\nTable1 filters; .47uF/10kohm; M2.4 polynomial strength 1; "
         "M2.2 synthetic output noise 1e-4 (input noise zero), loss "
         "1e-5/stage, leakage .1/stage-second, pole .25, mismatch .001; "
         "M2.6 Nominal gains .25/4. Default 1024 physical stages.\nZero hidden "
         "core clamps and numerical guards are hard gates. Digital default "
         "preserved, frozen M2.6 bit regression. Noise seed = hash(engine seed "
         "XOR fixture seed,band, BBD tag); no channel index.\nRange grid "
         "observes 256 host samples per cell; it is an onset/range enforcement "
         "grid, not an extrema estimate across complete slow modulation "
         "cycles. Bucket residence uses actual capture/output timestamps; "
         "nominal N/(2fclk) differs from physical residence under variable "
         "clock and from full-path group delay. At fixed clock Eq.1 residence "
         "is (N-1)/(2fclk). nan residence means no bucket has completed "
         "transport. Analytic conservative modulation envelope limits are used "
         "by "
         "the engine. Tracking observes at least two Motion cycles in every "
         "variant, minimum 2s (40s at .05Hz).\nFeedback H2-H5 "
         "are whole-run 500Hz projections, not stationary THD. Effective .75 "
         "is an upper engine bound; requested .75 fixture uses Feedback .65 / "
         "Dynamics 1 and records the attained effective coefficient. No hidden "
         "limiter. Silence after impulse/burst is measured for 2s.\nBand "
         "response measures actual nonlinear/noisy fixture; complex sine "
         "response includes delay phase. No EQ compensation. nan denotes "
         "quantities not measured in that row. The spectral CSV separately "
         "measures clean H2-H5 THD and paired-noise RMS at fixed delay after "
         ".5s settling.\nCPU is 250ms audio plus "
         "1024-sample warmup, one timing observation per configuration, "
         "instrumented engine with operating distributions/timestamps "
         "disabled; clock min/max and max events are per-sample "
         "aggregates including warmup; mean clock and event rate include "
         "warmup. Compare on target hardware; no wall-time CI "
         "gate.\nDigital-vs-BBD harmonic ratio only interpretable as THD for "
         "stationary 500Hz sine after adequate settling; other rows are "
         "projections. All CSV duration limits explicit.\n";
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc > 1 && std::string(argv[1]) != "--tests-only") {
      std::filesystem::create_directories(argv[1]);
      if (argc > 2 && std::string(argv[2]) == "--cpu-only") {
        benchmark(argv[1]);
        return 0;
      }
      if (argc > 2 && std::string(argv[2]) == "--feedback-only") {
        feedbackQualification(argv[1]);
        return 0;
      }
      if (argc > 2 && std::string(argv[2]) == "--stage-only") {
        stageFeasibility(argv[1]);
        return 0;
      }
      if (argc > 2 && std::string(argv[2]) == "--spectral-only") {
        spectral(argv[1]);
        return 0;
      }
      auto preflight = file(argv[1], "README.txt",
                            "DriftBrigade-M2.7-BBD-Engine-Qualification");
      preflight.flush();
      preflight.close();
      qualification(argv[1]);
      spectral(argv[1]);
    } else
      tests();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
