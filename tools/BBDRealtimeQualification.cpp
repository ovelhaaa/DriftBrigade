#include "../tests/AllocationTracker.h"
#include "dsp/DriftEngine.h"
#include <chrono>
#include <complex>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
using namespace drift;
namespace {
constexpr double rates[] = {44100, 48000, 88200, 96000, 176400, 192000};
constexpr std::size_t stages[] = {512, 1024, 2048, 4096};
constexpr int blocks[] = {16, 32, 64, 128, 256, 512, 1024};
constexpr std::uint32_t seed = 77;
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
double noise(std::uint64_t n) {
  std::uint32_t v = std::uint32_t(n + 1);
  v ^= v >> 16;
  v *= 0x7feb352du;
  v ^= v >> 15;
  v *= 0x846ca68bu;
  v ^= v >> 16;
  return double(v) / 4294967296. * 2 - 1;
}
double material(int kind, std::uint64_t n, double sr) {
  double t = n / sr;
  switch (kind) {
  case 0:
    return 0;
  case 1:
    return .2 * std::sin(2 * pi * 500 * t);
  case 2:
    return .2 * noise(n);
  case 3:
    return (.2 * std::sin(2 * pi * 173 * t) +
            .08 * std::sin(2 * pi * 509 * t)) *
           (.6 + .4 * std::cos(2 * pi * .7 * t));
  case 4:
    return std::exp(-100 * std::fmod(t, .2)) * .8 * std::cos(2 * pi * 700 * t);
  default:
    return (std::fmod(t, 1) < .5 ? .5 : 1e-6) * std::sin(2 * pi * 500 * t);
  }
}
EngineParameters normal() {
  EngineParameters p;
  p[Motion] = .7;
  p[Depth] = .5;
  p[Center] = 8;
  p[Chaos] = .55;
  p[Coherence] = .45;
  p[Width] = .6;
  p[Dynamics] = .25;
  p[Feedback] = .12;
  p[Mix] = 1;
  return p;
}
void collection(DriftEngine &e, bool enabled) {
#ifdef DRIFT_BBD_INSTRUMENT
  e.enableBBDOperatingInstrumentation(enabled);
#else
  (void)e;
  (void)enabled;
#endif
}
void configure(DriftEngine &e, double sr, std::size_t count,
               const EngineParameters &p = normal(), double center = 0,
               BBDVoiceConfig c = BBDVoiceConfig::fullResearchFixture(),
               DelayBackend backend = DelayBackend::ExperimentalBBD) {
  c.physicalStages = count;
  require(e.setDelayBackend(backend, c), "backend configuration");
  if (center > 0)
    require(e.qualificationSetCenter(center),
            "qualification center before prepare");
  e.setParameters(p);
  e.prepare(sr, seed);
  collection(e, false);
  e.reset(seed);
}
bool voiceFinite(const BBDModulatedDelayVoice &v) {
  const auto &p = v.signalPath();
  const auto &t = p.core.telemetry();
  return v.finiteState() &&
         std::isfinite(p.compressor.levelAverager().value()) &&
         std::isfinite(p.expander.levelAverager().value()) &&
         std::isfinite(p.compressor.currentGain()) &&
         std::isfinite(p.expander.currentGain()) &&
         std::isfinite(t.accumulatedClockPhase) &&
         t.accumulatedClockPhase >= 0 && t.accumulatedClockPhase < 1;
}
bool engineFinite(const DriftEngine &e) {
  if (!std::isfinite(e.telemetry().envelope))
    return false;
  if (e.delayBackend() == DelayBackend::ExperimentalBBD)
    for (int ch = 0; ch < 2; ++ch)
      for (int b = 0; b < 4; ++b)
        if (!voiceFinite(e.bbdVoice(ch, b)))
          return false;
  return true;
}
struct Counts {
  std::uint64_t events = 0, captures = 0, outputs = 0, clamps = 0, guards = 0;
  double clock = 0;
  std::uint32_t maximumEvents = 0;
};
Counts counts(const DriftEngine &e) {
  Counts s;
  if (e.delayBackend() == DelayBackend::ExperimentalBBD)
    for (int ch = 0; ch < 2; ++ch)
      for (int b = 0; b < 4; ++b) {
        const auto &v = e.bbdVoice(ch, b);
        const auto &t = v.signalPath().core.telemetry();
        s.events += t.totalEventCount;
        s.captures += t.totalCaptureCount;
        s.outputs += t.totalOutputCount;
        s.clock = std::max(s.clock, t.effectiveClockHz);
        s.maximumEvents = std::max(s.maximumEvents, t.eventsThisHostSample);
#ifdef DRIFT_BBD_INSTRUMENT
        s.clamps += v.hiddenClamps;
        s.guards += v.numericalGuards;
#endif
      }
#ifdef DRIFT_BBD_INSTRUMENT
  if (e.delayBackend() == DelayBackend::ExperimentalBBD) {
    s.clock = e.telemetry().maximumBBDClock;
    s.maximumEvents = e.telemetry().maximumBBDEventsPerSample;
  }
#endif
  return s;
}
void disabled(const DriftEngine &e) {
#ifdef DRIFT_BBD_INSTRUMENT
  for (int ch = 0; ch < 2; ++ch)
    for (int b = 0; b < 4; ++b) {
      const auto &c = e.bbdVoice(ch, b).signalPath().core;
      require(!c.collectOperatingStats && c.operatingStats.count == 0 &&
                  c.operatingStats.transportedCount == 0,
              "timing collection disabled across reset");
    }
#else
  (void)e;
#endif
}
void nonfiniteTests() {
  for (auto backend :
       {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD})
    for (double fault : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()})
      for (int channels = 0; channels < 3; ++channels) {
        DriftEngine e, reference;
        configure(e, 48000, 1024, normal(), 0,
                  BBDVoiceConfig::fullResearchFixture(), backend);
        configure(reference, 48000, 1024, normal(), 0,
                  BBDVoiceConfig::fullResearchFixture(), backend);
        const auto before = allocations.load();
        for (int n = 0; n < 2048; ++n) {
          double l = material(3, n, 48000), r = material(2, n, 48000), rl = l,
                 rr = r;
          if (n == 512) {
            if (channels != 1) {
              l = fault;
              rl = 0;
            }
            if (channels != 0) {
              r = fault;
              rr = 0;
            }
          }
          auto y = e.processSample(l, r),
               clean = reference.processSample(rl, rr);
          require(std::isfinite(y[0]) && std::isfinite(y[1]),
                  "host nonfinite output");
          require(std::memcmp(y.data(), clean.data(), sizeof(y)) == 0,
                  "nonfinite host admission must equal zero substitution "
                  "without future contamination");
          if (n % 64 == 0)
            require(engineFinite(e), "host nonfinite state recovery");
        }
        require(before == allocations.load(), "nonfinite recovery allocations");
      }
}
void prepareTests() {
  for (auto count : {std::size_t(0), std::size_t(3), std::size_t(65538),
                     std::numeric_limits<std::size_t>::max()}) {
    DriftEngine e;
    auto c = BBDVoiceConfig::fullResearchFixture();
    c.physicalStages = count;
    require(e.setDelayBackend(DelayBackend::ExperimentalBBD, c),
            "invalid pre-prepare fixture accepted for rejection test");
    bool threw = false;
    try {
      e.prepare(48000, seed);
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    require(threw && !e.isPrepared(),
            "invalid stages rejected / engine not prepared");
    auto y = e.processSample(.2, -.2);
    require(y[0] == 0 && y[1] == 0, "failed prepare cannot enter DSP");
    configure(e, 48000, 1024);
    DriftEngine fresh;
    configure(fresh, 48000, 1024);
    for (int n = 0; n < 512; ++n) {
      auto a = e.processSample(material(3, n, 48000), 0),
           b = fresh.processSample(material(3, n, 48000), 0);
      require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
              "failed prepare valid reprepare identity");
    }
  }
  for (double rate : {0., -1., 1e9, std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::infinity()}) {
    DriftEngine e;
    configure(e, rate, 1024);
    require(e.isPrepared(), "host rate normalization");
    for (int n = 0; n < 64; ++n)
      e.processSample(material(3, n, 48000), 0);
    require(engineFinite(e), "normalized host rate finite");
    e.prepare(48000, seed);
    e.reset(seed);
    DriftEngine fresh;
    configure(fresh, 48000, 1024);
    const auto before = allocations.load();
    for (int n = 0; n < 512; ++n) {
      auto a = e.processSample(material(3, n, 48000), 0),
           b = fresh.processSample(material(3, n, 48000), 0);
      require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
              "invalid rate valid reprepare identity");
    }
    require(before == allocations.load(),
            "reprepared callbacks allocation free");
  }
  auto c = BBDVoiceConfig::fullResearchFixture();
  c.character.insertionDb = std::numeric_limits<double>::quiet_NaN();
  c.character.lossPerStage = -1;
  c.character.outputNoiseRms = std::numeric_limits<double>::infinity();
  c.character.nonlinear.strength = std::numeric_limits<double>::quiet_NaN();
  c.gainStaging.compressorToBBDGain = std::numeric_limits<double>::quiet_NaN();
  c.gainStaging.bbdToExpanderGain = -1;
  c.gainStaging.preCompressorGain = std::numeric_limits<double>::infinity();
  c.gainStaging.postExpanderGain = 0;
  c.gainStaging.nonlinearReferenceLevel =
      std::numeric_limits<double>::quiet_NaN();
  DriftEngine e;
  configure(e, 48000, 1024, normal(), 0, c);
  const auto &normalized = e.bbdVoice(0, 0).signalPath().gainStaging();
  require(normalized.preCompressorGain == 1 &&
              normalized.compressorToBBDGain == 1 &&
              normalized.bbdToExpanderGain == 1 &&
              normalized.postExpanderGain == 1 &&
              normalized.nonlinearReferenceLevel == 1,
          "invalid gain staging uses existing unity normalization");
  for (int n = 0; n < 512; ++n)
    e.processSample(material(3, n, 48000), 0);
  require(engineFinite(e), "invalid character/gain normalization");
  e.reset(seed);
  DriftEngine fresh;
  configure(fresh, 48000, 1024, normal(), 0, c);
  const auto before = allocations.load();
  for (int n = 0; n < 512; ++n) {
    auto a = e.processSample(material(3, n, 48000), 0),
         b = fresh.processSample(material(3, n, 48000), 0);
    require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
            "normalized character gains reset identity");
  }
  require(before == allocations.load(),
          "normalized character callbacks allocation free");
}
void clockTests() {
  for (double sr : rates)
    for (auto count : stages) {
      const double dmin =
          count / (sr * ClockedBBDCore::maximumEventsPerHostSample);
      for (double delay :
           {dmin, 1.05 * dmin, 1.25 * dmin, 2 * dmin, .001, .003, .008}) {
        if (delay < dmin)
          continue;
        auto p = normal();
        p[Depth] = 0;
        p[Feedback] = p[Dynamics] = 0;
        DriftEngine e;
        configure(e, sr, count, p, delay);
        const auto before = allocations.load();
        for (int n = 0; n < 128; ++n) {
          auto y = e.processSample(material(3, n, sr), material(2, n, sr));
          require(std::isfinite(y[0]) && std::isfinite(y[1]),
                  "maximum clock finite output");
        }
        auto s = counts(e);
        require(s.clamps == 0 && s.guards == 0 && engineFinite(e),
                "maximum clock admitted/finite without guards");
        require(s.maximumEvents <= 128, "event limit");
        if (delay == dmin)
          require(s.maximumEvents >= 127, "true maximum clock exercised");
        require(s.events == s.captures + s.outputs,
                "physical capture/output accounting");
        require(before == allocations.load(), "maximum clock allocation");
        disabled(e);
      }
    }
}

double hostile(double amplitude, int pattern, int n, int length) {
  switch (pattern) {
  case 0:
    return n == 0 ? amplitude : 0;
  case 1:
    return amplitude;
  case 2:
    return n % 2 ? -amplitude : amplitude;
  default:
    return n < length / 4 ? amplitude : 0;
  }
}
void hostileTests() {
  for (double amplitude :
       {1., 2., 16., 100., 1e6, double(std::numeric_limits<float>::max())})
    for (double sign : {-1., 1.})
      for (int pattern = 0; pattern < 4; ++pattern) {
        DriftEngine e, reference;
        auto p = normal();
        p[Feedback] = .65;
        p[Dynamics] = 1;
        p[Depth] = 0;
        double dmin = 1024. / (48000 * 128);
        configure(e, 48000, 1024, p, dmin);
        configure(reference, 48000, 1024, p, dmin);
        const auto before = allocations.load();
        for (int n = 0; n < 128; ++n) {
          auto y = e.processSample(hostile(sign * amplitude, pattern, n, 128),
                                   hostile(-sign * amplitude, pattern, n, 128));
          require(std::isfinite(y[0]) && std::isfinite(y[1]),
                  "finite hostile input output");
        }
        require(engineFinite(e), "finite hostile state");
        for (int n = 0; n < 20000; ++n) {
          auto y =
              e.processSample(material(3, n, 48000), material(2, n, 48000));
          require(std::isfinite(y[0]) && std::isfinite(y[1]),
                  "hostile ordinary input recovery");
        }
        for (int ch = 0; ch < 2; ++ch)
          for (int b = 0; b < 4; ++b) {
            const auto &path = e.bbdVoice(ch, b).signalPath();
            require(path.compressor.levelAverager().value() <= 1 &&
                        path.expander.levelAverager().value() <= 1,
                    "hostile detectors release back into unity startup range");
          }
        e.reset(seed);
        disabled(e);
        for (int n = 0; n < 128; ++n) {
          auto y = e.processSample(material(3, n, 48000),
                                   material(2, n, 48000)),
               r = reference.processSample(material(3, n, 48000),
                                           material(2, n, 48000));
          require(std::memcmp(y.data(), r.data(), sizeof(y)) == 0,
                  "hostile stress reset fresh identity");
        }
        require(counts(e).clamps == 0 && before == allocations.load(),
                "hostile reset allocations/clamps");
        e.reset(seed);
        std::array<float, 128> l{}, r{};
        for (int n = 0; n < 128; ++n) {
          l[n] = static_cast<float>(hostile(sign * amplitude, pattern, n, 128));
          r[n] = -l[n];
        }
        float *channels[] = {l.data(), r.data()};
        e.process(channels, 2, 128);
        for (int n = 0; n < 128; ++n)
          require(std::isfinite(l[n]) && std::isfinite(r[n]),
                  "hostile actual float callback finite");
        require(engineFinite(e) && before == allocations.load(),
                "hostile callback state allocation free");
      }
}
EngineParameters automation(std::size_t epoch) {
  EngineParameters p;
  for (std::size_t i = 0; i < Count; ++i) {
    double fraction =
        epoch % 2 ? double((epoch * 13 + i * 7) % 101) / 100 : (i % 2 ? 0 : 1);
    p.values[i] =
        parameterSpecs[i].minimum +
        (parameterSpecs[i].maximum - parameterSpecs[i].minimum) * fraction;
  }
  return p;
}
void segmentationTests() {
  constexpr int length = 2048, step = 113;
  for (int scenario = 0; scenario < 4; ++scenario) {
    std::vector<float> referenceL, referenceR;
    for (int block : {1, 7, 16, 17, 32, 64, 127, 256, 511, 1024}) {
      auto p = normal();
      p[Depth] = 0;
      double center = scenario == 0 ? 4096. / (44100 * 128) : 0;
      if (scenario == 1)
        p[Feedback] = .65, p[Dynamics] = 1;
      DriftEngine e;
      configure(e, 44100, 4096, p, center);
      std::vector<float> l(length), r(length);
      for (int n = 0; n < length; ++n) {
        l[n] = float(scenario == 2 && n < 1024 ? 0 : material(3, n, 44100));
        r[n] = float(scenario == 2 && n < 1024 ? 0 : material(2, n, 44100));
      }
      const auto before = allocations.load();
      for (int offset = 0; offset < length;) {
        if (scenario == 3 && offset % step == 0)
          e.setParameters(automation(offset / step));
        int size = std::min(block, length - offset);
        if (scenario == 3)
          size = std::min(size, step - offset % step);
        float *channels[] = {l.data() + offset, r.data() + offset};
        e.process(channels, 2, size);
        offset += size;
        require(engineFinite(e) && counts(e).clamps == 0,
                "stress segmentation finite/admitted");
      }
      require(before == allocations.load(), "stress segmentation allocations");
      if (referenceL.empty())
        referenceL = l, referenceR = r;
      else
        require(std::memcmp(l.data(), referenceL.data(),
                            length * sizeof(float)) == 0 &&
                    std::memcmp(r.data(), referenceR.data(),
                                length * sizeof(float)) == 0,
                "stress block bit identity");
    }
  }
}
void automationTests() {
  for (double sr : rates) {
    DriftEngine e;
    configure(e, sr, 4096);
    auto previous = e.telemetry().delaySeconds;
    auto initial = normal();
    for (auto &ch : previous)
      ch.fill(initial[Center] * .001);
    const auto before = allocations.load();
    for (int n = 0; n < 2048; ++n) {
      if (n % 17 == 0)
        e.setParameters(automation(n / 17));
      auto y = e.processSample(material(3, n, sr), material(2, n, sr));
      require(std::isfinite(y[0]) && std::isfinite(y[1]),
              "automation output finite");
      for (int ch = 0; ch < 2; ++ch)
        for (int b = 0; b < 4; ++b) {
          double d = e.telemetry().delaySeconds[ch][b];
          require(d >= e.minimumDelaySeconds() - 1e-15 && d <= .055 + 1e-15,
                  "automation physical admission");
          require(std::abs(d - previous[ch][b]) <= .25 / sr + 1e-15,
                  "quarter-sample slew preserved");
          previous[ch][b] = d;
        }
    }
    require(engineFinite(e) && counts(e).clamps == 0 &&
                before == allocations.load(),
            "automation state/clamps/allocations");
    disabled(e);
  }
}
void faultTests() {
  for (auto fault : {BBDQualificationFault::Feedback,
                     BBDQualificationFault::Held, BBDQualificationFault::Bucket,
                     BBDQualificationFault::CompressorDetector,
                     BBDQualificationFault::ExpanderDetector})
    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
      DriftEngine e, fresh;
      configure(e, 48000, 1024);
      configure(fresh, 48000, 1024);
      for (int n = 0; n < 512; ++n)
        e.processSample(material(3, n, 48000), material(2, n, 48000));
      e.qualificationInject(0, 0, fault, value);
      const auto before = allocations.load();
      for (int n = 0; n < 1024; ++n)
        e.processSample(material(3, n, 48000), material(2, n, 48000));
      if (!engineFinite(e))
        e.qualificationResetVoice(0, 0, seed);
      if (!engineFinite(e))
        e.reset(seed);
      require(engineFinite(e),
              "internal injected fault recoverable by documented reset");
      e.reset(seed);
      disabled(e);
      for (int n = 0; n < 256; ++n) {
        auto a = e.processSample(material(3, n, 48000), material(2, n, 48000)),
             b = fresh.processSample(material(3, n, 48000),
                                     material(2, n, 48000));
        require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
                "internal fault engine reset fresh identity");
      }
      require(before == allocations.load(),
              "fault injection/reset/render allocation");
    }
}

struct Meter {
  double peak = 0, sum = 0, power = 0;
  std::uint64_t samples = 0;
  void add(double x) {
    peak = std::max(peak, std::abs(x));
    sum += x;
    power += x * x;
    ++samples;
  }
  double rms() const {
    return std::sqrt(power / std::max(std::uint64_t(1), samples));
  }
  double dc() const { return sum / std::max(std::uint64_t(1), samples); }
};
std::ofstream file(const std::filesystem::path &root, const std::string &name,
                   const char *header) {
  std::ofstream f(root / name);
  f.exceptions(std::ios::failbit | std::ios::badbit);
  f.precision(17);
  f << header << '\n';
  return f;
}
const char *counterMode() {
#ifdef DRIFT_BBD_INSTRUMENT
  return "minimal_qualification_counters";
#else
  return "production_scheduler_counters";
#endif
}
const char *clockObservation() {
#ifdef DRIFT_BBD_INSTRUMENT
  return "per_sample_max";
#else
  return "callback_end_sample";
#endif
}
const char *loads[] = {"normal", "heavy_musical", "max_clock",
                       "max_clock_feedback", "max_clock_modulation"};
EngineParameters loadParameters(int load) {
  auto p = normal();
  if (load == 1 || load == 4) {
    p[Motion] = 10;
    p[Depth] = 1;
    p[Chaos] = 1;
    p[Coherence] = 0;
    p[Width] = 1;
    p[Dynamics] = 1;
    p[Feedback] = .65;
  }
  if (load == 2 || load == 3) {
    p[Depth] = 0;
    p[Feedback] = load == 2 ? 0 : .65;
    p[Dynamics] = load == 2 ? 0 : 1;
  }
  return p;
}
double loadCenter(int load, double sr, std::size_t count) {
  double minimum = count / (sr * 128);
  return load < 2 ? 0 : minimum * (load == 2 ? 1 : 1.05);
}
struct Distribution {
  double minimum = 0, p50 = 0, p90 = 0, p95 = 0, p99 = 0, p999 = 0, maximum = 0,
         cpu = 0;
  std::uint64_t misses = 0;
  std::vector<double> durations;
  void finish(double deadline) {
    auto sorted = durations;
    std::sort(sorted.begin(), sorted.end());
    auto q = [&](double fraction) {
      return sorted[std::min(
          sorted.size() - 1,
          std::size_t(std::max(1., std::ceil(fraction * sorted.size()))) - 1)];
    };
    minimum = sorted.front();
    maximum = sorted.back();
    p50 = q(.5);
    p90 = q(.9);
    p95 = q(.95);
    p99 = q(.99);
    p999 = q(.999);
    cpu = std::accumulate(sorted.begin(), sorted.end(), 0.) /
          (deadline * sorted.size());
    for (double x : sorted)
      misses += x > deadline;
  }
};
const char *classification(const Distribution &d, double deadline) {
  if (d.misses >= 2)
    return "DEADLINE_FAIL";
  if (d.misses)
    return "DEADLINE_RISK";
  if (d.p99 >= .5 * deadline)
    return "REALTIME_TIGHT";
  return "REALTIME_COMFORTABLE";
}
const char *utilizationBand(double ratio) {
  return ratio < .25   ? "lt25"
         : ratio < .5  ? "25_50"
         : ratio < .75 ? "50_75"
         : ratio <= 1  ? "75_100"
                       : "gt100";
}
struct TimingResult {
  Distribution distribution;
  Counts counters;
  Meter output;
};
TimingResult timeCase(DriftEngine &e, double sr, std::size_t count, int block,
                      int callbacks, const char *name, bool representative,
                      std::ostream &raw) {
  std::vector<float> l(block), r(block);
  TimingResult result;
  result.distribution.durations.reserve(callbacks);
  for (int n = 0; n < 1024; ++n)
    e.processSample(material(3, n, sr), material(2, n, sr));
  disabled(e);
  auto previous = counts(e);
  std::uint64_t sample = 1024;
  for (int callback = 0; callback < callbacks; ++callback) {
    for (int n = 0; n < block; ++n) {
      l[n] = float(material(3, sample + n, sr));
      r[n] = float(material(2, sample + n, sr));
    }
    float *channels[] = {l.data(), r.data()};
    e.qualificationBeginCallback();
    const auto allocationCount = allocations.load();
    const auto start = std::chrono::steady_clock::now();
    e.process(channels, 2, block);
    const double duration =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    require(allocationCount == allocations.load(),
            "timed callback allocations");
    result.distribution.durations.push_back(duration);
    auto now = counts(e);
    result.counters.events += now.events - previous.events;
    result.counters.captures += now.captures - previous.captures;
    result.counters.outputs += now.outputs - previous.outputs;
    result.counters.clock = std::max(result.counters.clock, now.clock);
    result.counters.maximumEvents =
        std::max(result.counters.maximumEvents, now.maximumEvents);
    result.counters.clamps = now.clamps;
    result.counters.guards = now.guards;
    for (int n = 0; n < block; ++n) {
      require(std::isfinite(l[n]) && std::isfinite(r[n]),
              "timed output finite");
      result.output.add(l[n]);
      result.output.add(r[n]);
    }
    require(engineFinite(e) && now.clamps == 0 && now.guards == 0,
            "timed numerical state/admission");
    raw << counterMode() << ',' << int(e.delayBackend()) << ',' << sr << ','
        << count << ',' << block << ',' << name << ',' << representative << ','
        << callback << ',' << duration << ',' << block / sr << ','
        << duration / (block / sr) << ',' << now.events - previous.events << ','
        << now.captures - previous.captures << ','
        << now.outputs - previous.outputs << ',' << now.clock << ','
        << now.maximumEvents << ',' << clockObservation() << '\n';
    previous = now;
    sample += block;
  }
  disabled(e);
  result.distribution.finish(block / sr);
  return result;
}
void timing(const std::filesystem::path &root, bool local,
            bool comparison = false) {
  const std::string prefix =
      comparison ? "bbd_realtime_production_" : "bbd_realtime_";
  auto raw =
      file(root, prefix + "callbacks.csv",
           "counter_mode,backend,rate,stages,block,load,representative,index,"
           "duration_s,deadline_s,deadline_ratio,events,captures,outputs,"
           "maximum_clock,maximum_events_sample,clock_observation");
  auto f = file(
      root, prefix + "callback_distribution.csv",
      "counter_mode,backend,rate,stages,block,load,representative,callbacks,"
      "observation_audio_s,min_s,p50_s,p90_s,p95_s,p99_s,p99_9_s,max_s,cpu_per_"
      "audio_s,misses,miss_percent,maximum_clock,max_events_sample,total_"
      "events_s,captures_s,outputs_s,hidden_clamps,numerical_guards,finite");
  auto margin =
      file(root, prefix + "deadline_margin.csv",
           "counter_mode,backend,rate,stages,block,load,representative,"
           "callbacks,deadline_s,p99_utilization,max_utilization,worst_margin,"
           "misses,miss_percent,utilization_band,classification");
  auto matrix = file(
      root, prefix + "sample_rate_matrix.csv",
      "counter_mode,rate,stages,block,load,representative,min_delay_s,max_"
      "model_clock_hz,max_observed_clock_hz,max_events_sample,cpu_audio_ratio,"
      "p99_deadline_ratio,worst_deadline_ratio,finite,hidden_clamps");
  auto one = [&](double sr, std::size_t count, int block, int load,
                 bool representative) {
    auto p = loadParameters(load);
    int callbacks = representative ? (local ? 10000 : 256) : (local ? 32 : 8);
    for (auto backend :
         {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD}) {
      DriftEngine e;
      configure(e, sr, count, p,
                backend == DelayBackend::ExperimentalBBD
                    ? loadCenter(load, sr, count)
                    : 0,
                BBDVoiceConfig::fullResearchFixture(), backend);
      auto result = timeCase(e, sr, count, block, callbacks, loads[load],
                             representative, raw);
      auto &d = result.distribution;
      double deadline = block / sr;
      f << counterMode() << ',' << int(backend) << ',' << sr << ',' << count
        << ',' << block << ',' << loads[load] << ',' << representative << ','
        << callbacks << ',' << deadline * callbacks << ',' << d.minimum << ','
        << d.p50 << ',' << d.p90 << ',' << d.p95 << ',' << d.p99 << ','
        << d.p999 << ',' << d.maximum << ',' << d.cpu << ',' << d.misses << ','
        << 100. * d.misses / callbacks << ',' << result.counters.clock << ','
        << result.counters.maximumEvents << ','
        << result.counters.events / (deadline * callbacks) << ','
        << result.counters.captures / (deadline * callbacks) << ','
        << result.counters.outputs / (deadline * callbacks) << ','
        << result.counters.clamps << ',' << result.counters.guards << ",1\n";
      margin << counterMode() << ',' << int(backend) << ',' << sr << ','
             << count << ',' << block << ',' << loads[load] << ','
             << representative << ',' << callbacks << ',' << deadline << ','
             << d.p99 / deadline << ',' << d.maximum / deadline << ','
             << (d.maximum ? deadline / d.maximum
                           : std::numeric_limits<double>::infinity())
             << ',' << d.misses << ',' << 100. * d.misses / callbacks << ','
             << utilizationBand(d.p99 / deadline) << ','
             << classification(d, deadline) << '\n';
      if (backend == DelayBackend::ExperimentalBBD)
        matrix << counterMode() << ',' << sr << ',' << count << ',' << block
               << ',' << loads[load] << ',' << representative << ','
               << e.minimumDelaySeconds() << ',' << 64 * sr << ','
               << result.counters.clock << ',' << result.counters.maximumEvents
               << ',' << d.cpu << ',' << d.p99 / deadline << ','
               << d.maximum / deadline << ",1," << result.counters.clamps
               << '\n';
    }
  };
  if (!comparison)
    for (double sr : rates)
      for (auto count : stages)
        for (int block : blocks)
          for (int load = 0; load < 5; ++load)
            one(sr, count, block, load, false);
  one(48000, 1024, 128, 0, true);
  one(48000, 1024, 128, 1, true);
  one(48000, 512, 16, 2, true);
  one(192000, 4096, 16, 2, true);
  one(44100, 1024, 32, 3, true);
  one(88200, 2048, 16, 4, true);
  raw.flush();
  f.flush();
  margin.flush();
  matrix.flush();
}
void maximumClockReport(const std::filesystem::path &root, bool local) {
  auto f = file(root, "bbd_realtime_max_clock.csv",
                "rate,stages,delay_s,min_delay_s,clock_hz,expected_clock_hz,"
                "max_edges_sample,edges_s,captures_s,outputs_s,hidden_clamps,"
                "numerical_guards,finite,block,callbacks,p50_s,p95_s,p99_s,p99_"
                "9_s,max_s,worst_deadline_ratio");
  auto raw =
      file(root, "bbd_realtime_max_clock_callbacks.csv",
           "counter_mode,backend,rate,stages,block,load,representative,index,"
           "duration_s,deadline_s,deadline_ratio,events,captures,outputs,"
           "maximum_clock,maximum_events_sample,clock_observation");
  for (double sr : rates)
    for (auto count : stages) {
      double minimum = count / (sr * 128);
      for (double delay : {minimum, 1.05 * minimum, 1.25 * minimum, 2 * minimum,
                           .001, .003, .008}) {
        if (delay < minimum)
          continue;
        auto p = normal();
        p[Depth] = p[Feedback] = p[Dynamics] = 0;
        DriftEngine e;
        configure(e, sr, count, p, delay);
        int callbacks = local ? 32 : 8;
        auto result = timeCase(e, sr, count, 64, callbacks, "fixed_delay_grid",
                               false, raw);
        auto &d = result.distribution;
        auto &c = result.counters;
        double seconds = 64. * callbacks / sr;
        f << sr << ',' << count << ',' << delay << ',' << minimum << ','
          << c.clock << ',' << count / (2 * delay) << ',' << c.maximumEvents
          << ',' << c.events / seconds << ',' << c.captures / seconds << ','
          << c.outputs / seconds << ',' << c.clamps << ',' << c.guards
          << ",1,64," << callbacks << ',' << d.p50 << ',' << d.p95 << ','
          << d.p99 << ',' << d.p999 << ',' << d.maximum << ','
          << d.maximum / (64 / sr) << '\n';
      }
    }
  f.flush();
  raw.flush();
}

#ifdef DRIFT_BBD_INSTRUMENT
struct Subnormals {
  std::uint64_t detectors = 0, filters = 0, feedback = 0, dc = 0, banks = 0,
                envelope = 0;
};
Subnormals subnormals(const DriftEngine &e) {
  Subnormals s;
  auto tiny = [](double x) { return std::fpclassify(x) == FP_SUBNORMAL; };
  for (int ch = 0; ch < 2; ++ch) {
    for (double x : e.qualificationBankState(ch))
      s.banks += tiny(x);
    for (int b = 0; b < 4; ++b) {
      const auto &v = e.bbdVoice(ch, b);
      const auto &p = v.signalPath();
      s.detectors += tiny(p.compressor.levelAverager().value()) +
                     tiny(p.expander.levelAverager().value());
      for (auto x : p.core.inputFilterState())
        s.filters += tiny(x.real()) + tiny(x.imag());
      for (auto x : p.core.outputFilterState())
        s.filters += tiny(x.real()) + tiny(x.imag());
      s.feedback += tiny(v.feedbackWet());
      s.dc += tiny(v.qualificationDCState());
    }
  }
  s.envelope = tiny(e.qualificationEnvelopeState()) +
               tiny(e.qualificationEnvelopeControlState());
  return s;
}
void denormalReport(const std::filesystem::path &root, bool local,
                    bool high = false) {
  auto f = file(root,
                high ? "bbd_realtime_denormals_max_clock.csv"
                     : "bbd_realtime_denormals.csv",
                "time_s,counter_mode,noise_enabled,detector_subnormals,filter_"
                "subnormals,feedback_subnormals,dc_subnormals,bank_subnormals,"
                "envelope_subnormals,interval_p50_callback_s,interval_max_"
                "callback_s,output_rms,finite,hidden_clamps,numerical_guards");
  auto p = normal();
  p[Depth] = p[Feedback] = p[Dynamics] = 0;
  auto c = BBDVoiceConfig::fullResearchFixture();
  c.character.inputNoiseRms = c.character.outputNoiseRms = 0;
  DriftEngine e;
  configure(e, 48000, 1024, p, high ? 1024. / (48000 * 128) : 0, c);
  std::vector<double> durations;
  durations.reserve(400);
  constexpr int block = 128;
  std::array<float, block> l{}, r{};
  Meter output;
  double maximum = 0;
  int total = int(48000 * (local ? (high ? 60. : 300.) : .1)),
      next = int(48000 * (local ? 1. : .025));
  for (int offset = 0; offset < total; offset += block) {
    l.fill(0);
    r.fill(0);
    if (offset == 0)
      l[0] = r[0] = 1;
    int size = std::min(block, total - offset);
    float *channels[] = {l.data(), r.data()};
    auto before = allocations.load();
    auto start = std::chrono::steady_clock::now();
    e.process(channels, 2, size);
    double duration =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    require(before == allocations.load(), "denormal callback allocation");
    durations.push_back(duration);
    maximum = std::max(maximum, duration);
    for (int n = 0; n < size; ++n)
      output.add(l[n]);
    if (offset + size >= next || offset + size == total) {
      auto tiny = subnormals(e);
      std::sort(durations.begin(), durations.end());
      auto c = counts(e);
      require(engineFinite(e) && c.clamps == 0,
              "denormal long tail finite/admitted");
      f << (offset + size) / 48000. << ',' << counterMode() << ",0,"
        << tiny.detectors << ',' << tiny.filters << ',' << tiny.feedback << ','
        << tiny.dc << ',' << tiny.banks << ',' << tiny.envelope << ','
        << durations[durations.size() / 2] << ',' << maximum << ','
        << output.rms() << ",1," << c.clamps << ',' << c.guards << '\n';
      durations.clear();
      maximum = 0;
      output = {};
      next += int(48000 * (local ? 1. : .025));
    }
  }
  disabled(e);
  f.flush();
}
#endif

void underflowTests() {
#ifdef DRIFT_BBD_INSTRUMENT
  auto p = normal();
  p[Depth] = p[Dynamics] = p[Feedback] = 0;
  auto c = BBDVoiceConfig::fullResearchFixture();
  c.character.inputNoiseRms = c.character.outputNoiseRms = 0;
  for (double center : {.008, 1024. / (48000 * 128)}) {
    DriftEngine e;
    configure(e, 48000, 1024, p, center, c);
    e.qualificationInjectTail(0x1p-1060);
    const auto before = allocations.load();
    for (int n = 0; n < 64; ++n)
      e.processSample(0, 0);
    auto s = subnormals(e);
    require(
        s.filters + s.banks + s.dc + s.envelope + s.detectors + s.feedback == 0,
        "persistent underflow states cleared locally");
    require(engineFinite(e) && before == allocations.load(),
            "underflow recovery finite allocation free");
  }
#endif
}
#include "BBDRealtimeReports.inc"
void tests() {
  nonfiniteTests();
  prepareTests();
  clockTests();
  hostileTests();
  automationTests();
  segmentationTests();
  faultTests();
  underflowTests();
  std::cout << "M2.8 short numerical stress passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc > 1 && std::string(argv[1]) != "--tests-only") {
      bool local = false, comparison = false, denormalsOnly = false,
           denormalsHigh = false, timingOnly = false, reportsOnly = false,
           feedbackOnly = false, startupOnly = false, recoveryOnly = false;
      for (int i = 2; i < argc; ++i) {
        local |= std::string(argv[i]) == "--local";
        comparison |= std::string(argv[i]) == "--comparison";
        denormalsOnly |= std::string(argv[i]) == "--denormals-only";
        denormalsHigh |= std::string(argv[i]) == "--denormals-high";
        timingOnly |= std::string(argv[i]) == "--timing-only";
        reportsOnly |= std::string(argv[i]) == "--reports-only";
        feedbackOnly |= std::string(argv[i]) == "--feedback-only";
        startupOnly |= std::string(argv[i]) == "--startup-only";
        recoveryOnly |= std::string(argv[i]) == "--recovery-only";
      }
      std::filesystem::create_directories(argv[1]);
      auto readme =
          file(argv[1], comparison ? "README_production.txt" : "README.txt",
               "M2.8 ENGINEERING ROBUSTNESS / NOT PRODUCT DEFAULT");
      readme << (local ? "LOCAL: 10000 representative callbacks; 60s "
                         "sustained; 30s feedback; 300s denormal tail.\n"
                       : "CI_SHORT: 256 representative callbacks; 0.1s "
                         "sustained; 0.25s feedback; 0.1s denormal. No "
                         "long-run/realtime guarantee.\n")
             << "All timing informational. Detailed operating statistics "
                "disabled inside timed callbacks; monitoring is a separate "
                "pass.\nNearest-rank percentiles; each raw callback has its "
                "deadline. Qualification-only center reaches model Dmin below "
                "product range.\nSeed 77; Release build required for CPU "
                "interpretation. See docs/m2_8_bbd_realtime_hardening.md for "
                "scope and limitations.\n";
      readme.flush();
#ifdef DRIFT_BBD_INSTRUMENT
      if (recoveryOnly) {
        recoveryReports(argv[1]);
        prepareReport(argv[1]);
        return 0;
      }
      if (startupOnly) {
        startupReport(argv[1], local);
        return 0;
      }
      if (feedbackOnly) {
        feedbackReport(argv[1], local);
        return 0;
      }
      if (denormalsOnly) {
        denormalReport(argv[1], local, denormalsHigh);
        return 0;
      }
#endif
      if (!reportsOnly)
        timing(argv[1], local, comparison);
      if (!comparison && !reportsOnly)
        maximumClockReport(argv[1], local);
#ifdef DRIFT_BBD_INSTRUMENT
      if (!comparison && !timingOnly)
        reports(argv[1], local);
#endif
    } else
      tests();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
