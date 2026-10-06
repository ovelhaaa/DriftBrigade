// Reuse the M2.8 fixtures, finite/allocation gates and measurement vocabulary.
// The renamed entry point is never called; no production source is changed.
#define main m28QualificationEntryPoint
#include "BBDRealtimeQualification.cpp"
#undef main
#include "dsp/DepthMapping.h"
#include <iomanip>
namespace {
std::string productCandidateFilter;
std::ofstream productFile(const std::filesystem::path &root,
                          const std::string &name, const char *header) {
  auto stream = file(root, name, header);
  stream.flush(); // Catch buffered /dev/full and other write failures before
                  // measuring.
  return stream;
}
struct BBDProductEnvelope {
  const char *name;
  std::size_t stages;
  double clockCap, edgeCap;
  double target = .5, tight = .7;
  int minimumQualificationBlock = 64;
  double clock(double sr) const { return std::min(clockCap, edgeCap * sr / 2); }
  double floor(double sr) const { return stages / (2 * clock(sr)); }
};
constexpr BBDProductEnvelope candidates[] = {
    {"Economy", 512, 192000, 8},
    {"Balanced", 1024, 384000, 16},
    {"Extended", 2048, 384000, 16},
    {"LargeStageResearch", 4096, 384000, 16}};
struct ProductTimingResult {
  double p50 = 0, p95 = 0, p99 = 0, p999 = 0, maximum = 0, utilization = 0;
  std::uint64_t misses = 0, events = 0;
};
double rank(std::vector<double> v, double p) {
  std::sort(v.begin(), v.end());
  return v.at(std::max(std::size_t(1), std::size_t(std::ceil(p * v.size()))) -
              1);
}
ProductTimingResult
productTiming(double sr, std::size_t stageCount, double delay, int block,
              int callbacks, std::ofstream *raw = nullptr,
              const char *label = "", int run = 0,
              BBDVoiceConfig cfg = BBDVoiceConfig::fullResearchFixture(),
              bool modulated = false) {
  auto p = normal();
  p[Depth] = p[Feedback] = p[Dynamics] = 0;
  double center = delay;
  if (modulated) {
    center = delay * 1.2;
    const double excursion =
        (center - delay) * .85 /
        DriftEngine::combinedModulationBound(OrganicVariant::Wander);
    for (int d = 0; d <= 1000; ++d)
      if (depthSeconds(p[Motion], d / 1000.) <= excursion)
        p[Depth] = d / 1000.;
  }
  DriftEngine e;
  configure(e, sr, stageCount, p, center, cfg);
  std::vector<float> left(block), right(block);
  std::vector<double> times(callbacks);
  // Identical deterministic fixture per callback avoids including generation in
  // timing.
  for (int n = 0; n < block; ++n)
    left[n] = right[n] = float(material(3, n, sr));
  for (int n = 0; n < 2048; ++n)
    e.processSample(material(3, n, sr), material(2, n, sr));
  disabled(e);
  auto startEvents = counts(e).events;
  auto before = allocations.load();
  ProductTimingResult t;
  const double deadline = block / sr;
  for (int k = 0; k < callbacks; ++k) {
    // Restore outside timer; processing mutates host buffers.
    for (int n = 0; n < block; ++n)
      left[n] = right[n] = float(material(3, n, sr));
    float *channels[] = {left.data(), right.data()};
    auto start = std::chrono::steady_clock::now();
    e.process(channels, 2, block);
    auto end = std::chrono::steady_clock::now();
    times[k] = std::chrono::duration<double>(end - start).count();
    t.misses += times[k] > deadline;
  }
  require(before == allocations.load(), "product callbacks allocate");
  auto c = counts(e);
  for (auto &ch : e.telemetry().delaySeconds)
    for (double d : ch)
      require(d >= delay - 1e-12, "product modulated trajectory below floor");
  require(engineFinite(e) && c.clamps == 0 && c.guards == 0,
          "product finite unclamped timing");
  t.events = c.events - startEvents;
  t.p50 = rank(times, .5);
  t.p95 = rank(times, .95);
  t.p99 = rank(times, .99);
  t.p999 = rank(times, .999);
  t.maximum = rank(times, 1);
  t.utilization = t.p99 / deadline;
  if (raw)
    for (int k = 0; k < callbacks; ++k)
      *raw << label << ',' << sr << ',' << stageCount << ',' << delay << ','
           << block << ',' << run << ',' << k << ',' << times[k] << ','
           << deadline << '\n';
  return t;
}
void envelopeTests() {
  recoveryReportingTests();
  for (auto c : candidates)
    for (double sr : rates) {
      const double floor = c.floor(sr), edges = 2 * c.clock(sr) / sr;
      require(c.clock(sr) <= sr * 128 / 2 && edges <= c.edgeCap,
              "product/model event separation");
      require(std::abs(floor * 2 * c.clock(sr) - c.stages) < 1e-9,
              "delay-floor identity");
      auto p = normal();
      p[Depth] = p[Feedback] = p[Dynamics] = 0;
      DriftEngine e;
      configure(e, sr, c.stages, p, floor);
      auto before = allocations.load();
      for (int n = 0; n < 256; ++n) {
        auto y = e.processSample(material(3, n, sr), material(2, n, sr));
        require(std::isfinite(y[0]) && std::isfinite(y[1]),
                "product output finite");
      }
      auto t = counts(e);
      require(engineFinite(e) && t.clamps == 0 && t.guards == 0,
              "product floor hidden core clamp");
      require(t.maximumEvents <= std::ceil(edges), "product event accounting");
      require(t.events == t.captures + t.outputs,
              "product physical edges accounting");
      require(before == allocations.load(), "product floor allocations");
    }
  // Explicit backend selection must preserve the default digital stream bit for
  // bit.
  DriftEngine defaultDigital, explicitDigital;
  defaultDigital.setParameters(normal());
  defaultDigital.prepare(48000, seed);
  defaultDigital.reset(seed);
  configure(explicitDigital, 48000, 1024, normal(), 0,
            BBDVoiceConfig::fullResearchFixture(),
            DelayBackend::DigitalFractional);
  for (int n = 0; n < 4096; ++n) {
    auto a = defaultDigital.processSample(material(3, n, 48000),
                                          material(2, n, 48000));
    auto b = explicitDigital.processSample(material(3, n, 48000),
                                           material(2, n, 48000));
    require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
            "product digital regression");
  }
  segmentationTests();
}
void productCoverage(std::ofstream &coverage, const BBDProductEnvelope &c,
                     double sr) {
  const double bound =
      DriftEngine::combinedModulationBound(OrganicVariant::Wander);
  int full = 0, reduced = 0, admitted = 0, total = 0;
  int clockFull = 0, clockReduced = 0;
  for (double m : {.05, .2, .7, 2., 6., 10.})
    for (double depth : {0., .25, .5, .75, 1.})
      for (double d : {.3, .5, 1., 2., 5., 10., 20., 30.}) {
        double excursion = std::max(
            0., std::min(depthSeconds(m, depth),
                         (d * .001 - c.stages / (sr * 128)) * .85 / bound));
        ++total;
        if (d * .001 < c.floor(sr))
          ++admitted;
        else if (excursion > (d * .001 - c.floor(sr)) * .85 / bound)
          ++reduced;
        else
          ++full;
        if (d * .001 >= c.floor(sr)) {
          if (excursion > (d * .001 - c.floor(sr)) / bound)
            ++clockReduced;
          else
            ++clockFull;
        }
      }
  for (double m : {.05, .7, 4., 9.})
    for (double center : {1., 2., 4., 8., 16.}) {
      double allowed =
                 std::max(0., (center * .001 - c.floor(sr)) * .85 / bound),
             maxDepth = 0;
      for (int d = 0; d <= 1000; ++d)
        if (depthSeconds(m, d / 1000.) <= allowed)
          maxDepth = d / 1000.;
      coverage << c.name << ',' << sr << ',' << total << ','
               << 100. * full / total << ',' << 100. * reduced / total << ','
               << 100. * admitted / total << ',' << c.floor(sr) * 1000 << ','
               << m << ',' << center << ',' << maxDepth << ','
               << 100. * clockFull / total << ','
               << 100. * clockReduced / total << ',' << .85 << '\n';
    }
  coverage.flush();
}
void productReports(const std::filesystem::path &root, bool local) {
  std::filesystem::create_directories(root);
  auto readme =
      productFile(root, "README.txt",
                  "ENGINEERING PRODUCT ENVELOPE / NOT FINAL SHIPPING DEFAULT");
  readme
      << "Scope: " << (local ? "LOCAL_RELEASE" : "CI_SHORT")
      << "\nCompiler: " << DRIFT_PRODUCT_COMPILER << "\nBuilt: " << __DATE__
      << ' ' << __TIME__
      << "\nTiming: nearest-rank; statistics disabled; instrumentation build "
         "with minimal guards; production comparison remains necessary.\n"
      << "Seed 77. Eight voices. Timing never gates CI. 50% p99 target; 70% "
         "tight threshold. Support classification remains provisional until "
         "repeated local trials.\n";
  readme.flush();
  auto profiles = productFile(
      root, "bbd_product_candidate_profiles.csv",
      "candidate,stages,max_clock_hz,max_edges_per_sample,target_p99_"
      "utilization,tight_p99_utilization,minimum_qualification_block,"
      "headroom,compander,feedback_policy");
  auto floors = productFile(
      root, "bbd_product_delay_floor.csv",
      "candidate,sample_rate,stages,product_min_delay_seconds,product_max_"
      "clock_hz,max_edges_per_sample,model_min_delay_seconds");
  auto ceilings = productFile(
      root, "bbd_product_clock_ceiling.csv",
      "candidate,sample_rate,clock_hz,edges_per_sample,model_clock_hz,status");
  auto coverage = productFile(
      root, "bbd_product_parameter_coverage.csv",
      "candidate,sample_rate,grid_points,fully_reproducible_percent,"
      "excursion_reduction_percent,center_admission_percent,minimum_"
      "center_ms,motion,center_ms,max_effective_depth,clock_cap_only_fully_reproducible_percent,clock_cap_only_excursion_reduction_percent,proposed_excursion_margin");
  auto support =
      productFile(root, "bbd_product_sample_rate_support.csv",
                  "candidate,sample_rate,classification,block64_worst_p99_"
                  "utilization,block64_total_misses,reason");
  auto timingFile = productFile(
      root, "bbd_product_repeated_timing.csv",
      "candidate,sample_rate,block,runs,callbacks_per_run,median_p99,"
      "worst_p99,median_p999,worst_p999,total_misses,runs_with_misses,"
      "worst_p99_utilization,tiny_block_risk");
  auto raw = productFile(root, "bbd_product_raw_timing.csv",
                         "candidate,sample_rate,stages,delay_seconds,block,run,"
                         "callback,seconds,deadline_seconds");
  auto events =
      productFile(root, "bbd_product_event_budget.csv",
                  "sample_rate,stages,target_edges_per_sample,delay_seconds,"
                  "clock_hz,actual_edges_per_sample,total_events_per_second,"
                  "p50,p95,p99,p999,max,p99_utilization,misses,callbacks");
  auto recommendation = productFile(root, "bbd_product_recommendation.csv",
                                    "candidate,sample_rate,status,reason");
  const int callbacks = local ? 10000 : 128, runs = local ? 5 : 1;
  for (auto c : candidates) {
    profiles << c.name << ',' << c.stages << ',' << c.clockCap << ','
             << c.edgeCap << ',' << c.target << ',' << c.tight << ','
             << c.minimumQualificationBlock
             << ",Nominal,fullResearchFixture,ExternalWetReturn_review\n";
    for (double sr : rates) {
      floors << c.name << ',' << sr << ',' << c.stages << ',' << c.floor(sr)
             << ',' << c.clock(sr) << ',' << 2 * c.clock(sr) / sr << ','
             << c.stages / (sr * 128) << '\n';
      ceilings << c.name << ',' << sr << ',' << c.clock(sr) << ','
               << 2 * c.clock(sr) / sr << ',' << sr * 64
               << ",HYPOTHESIS_PENDING_TIMING\n";
      productCoverage(coverage, c, sr);
      double keyUtil = 0;
      std::uint64_t keyMiss = 0;
      int keyMissRuns = 0;
      for (int block : blocks) {
        const int blockRuns = block == 64 ? runs : 1;
        const int blockCallbacks = block == 64 ? callbacks : local ? 512 : 128;
        std::vector<double> p99, p999;
        std::uint64_t misses = 0;
        int missRuns = 0;
        // Long repetition at block64; explicitly shorter exploration elsewhere.
        for (int run = 0; run < blockRuns; ++run) {
          auto t = productTiming(sr, c.stages, c.floor(sr), block,
                                 blockCallbacks, &raw, c.name, run);
          p99.push_back(t.p99);
          p999.push_back(t.p999);
          misses += t.misses;
          missRuns += t.misses > 0;
        }
        double util = rank(p99, 1) / (block / sr);
        timingFile << c.name << ',' << sr << ',' << block << ',' << blockRuns
                   << ',' << blockCallbacks << ',' << rank(p99, .5) << ','
                   << rank(p99, 1) << ',' << rank(p999, .5) << ','
                   << rank(p999, 1) << ',' << misses << ',' << missRuns << ','
                   << util << ','
                   << (util > .7   ? "PRODUCT_RISK"
                       : util > .5 ? "TIGHT"
                                   : "TARGET")
                   << '\n';
        timingFile.flush();
        raw.flush();
        if (block == 64)
          keyUtil = util, keyMiss = misses, keyMissRuns = missRuns;
      }
      // Finalization also requires production-counter modulation repetitions.
      // A static-only report must not advertise product support.
      std::string classification = "EXPERIMENTAL";
      support << c.name << ',' << sr << ',' << classification << ',' << keyUtil
              << ',' << keyMiss
              << ",local_machine_only_and_reduced_parameter_range\n";
      recommendation << c.name << ',' << sr << ','
                     << (classification == "SUPPORTED_WITH_REDUCED_BBD_RANGE"
                             ? "TIMING_ELIGIBLE_PRODUCTIZATION_CANDIDATE"
                             : "REVIEW_REQUIRED")
                     << ",listening_headroom_feedback_and_production_timing_"
                        "review_required\n";
      std::cout << c.name << ' ' << sr << " block64 worst p99 utilization "
                << keyUtil << std::endl;
    }
  }
  for (double sr : rates)
    for (double edges : {4., 8., 12., 16., 24., 32., 48., 64., 96., 128.}) {
      const auto count = std::size_t(1024);
      const double delay = count / (sr * edges);
      auto t = productTiming(sr, count, delay, 128, local ? 2048 : 128);
      const double actual = double(t.events) / (8 * 128 * (local ? 2048 : 128));
      require(std::abs(actual - edges) < .01,
              "event sweep actual physical load");
      events << sr << ',' << count << ',' << edges << ',' << delay << ','
             << sr * edges / 2 << ',' << actual << ',' << actual * sr * 8 << ','
             << t.p50 << ',' << t.p95 << ',' << t.p99 << ',' << t.p999 << ','
             << t.maximum << ',' << t.utilization << ',' << t.misses << ','
             << (local ? 2048 : 128) << '\n';
    }
  profiles.flush();
  floors.flush();
  ceilings.flush();
  coverage.flush();
  support.flush();
  timingFile.flush();
  raw.flush();
  events.flush();
  recommendation.flush();
}
#ifdef DRIFT_BBD_INSTRUMENT
#include "BBDProductQualityReports.inc"
#endif
} // namespace
int main(int argc, char **argv) {
  try {
    envelopeTests();
    if (argc > 1 && std::string(argv[1]) != "--tests-only") {
      bool local = false, qualityOnly = false, timingOnly = false,
           productionReference = false, measurementsOnly = false,
           costsOnly = false;
      for (int i = 2; i < argc; ++i) {
        local |= std::string(argv[i]) == "--local";
        qualityOnly |= std::string(argv[i]) == "--quality-only";
        timingOnly |= std::string(argv[i]) == "--timing-only";
        productionReference |= std::string(argv[i]) == "--production-reference";
        measurementsOnly |= std::string(argv[i]) == "--measurements-only";
        costsOnly |= std::string(argv[i]) == "--cost-only";
        if (std::string(argv[i]).rfind("--candidate=", 0) == 0)
          productCandidateFilter = std::string(argv[i]).substr(12);
      }
      std::filesystem::create_directories(argv[1]);
      for (int i = 2; i < argc; ++i)
        if (std::string(argv[i]) == "--coverage-only") {
          auto f = productFile(
              argv[1], "bbd_product_parameter_coverage.csv",
              "candidate,sample_rate,grid_points,fully_reproducible_percent,"
              "excursion_reduction_percent,center_admission_percent,minimum_"
              "center_ms,motion,center_ms,max_effective_depth,clock_cap_only_fully_reproducible_percent,clock_cap_only_excursion_reduction_percent,proposed_excursion_margin");
          for (auto c : candidates)
            for (double sr : rates)
              productCoverage(f, c, sr);
          return 0;
        }
      if (productionReference) {
        auto f = productFile(
            argv[1],
            std::string("bbd_product_") + counterMode() + "_modulation.csv",
            "candidate,sample_rate,block,runs,callbacks_per_run,"
            "median_p99,worst_p99,median_p999,worst_p999,total_"
            "misses,runs_with_misses,worst_p99_utilization");
        auto raw = productFile(
            argv[1],
            std::string("bbd_product_") + counterMode() + "_modulation_raw.csv",
            "candidate,sample_rate,stages,delay_seconds,block,run,"
            "callback,seconds,deadline_seconds");
        for (auto c : candidates)
          for (double sr : {44100., 48000., 88200., 96000.})
            for (int block : (c.stages == 1024 ? std::vector<int>{64, 128}
                                               : std::vector<int>{64})) {
              std::vector<double> p99, p999;
              std::uint64_t misses = 0;
              int missRuns = 0;
              for (int run = 0; run < 5; ++run) {
                auto t = productTiming(
                    sr, c.stages, c.floor(sr), block, 10000, &raw, c.name, run,
                    BBDVoiceConfig::fullResearchFixture(), true);
                p99.push_back(t.p99);
                p999.push_back(t.p999);
                misses += t.misses;
                missRuns += t.misses > 0;
              }
              f << c.name << ',' << sr << ',' << block << ",5,10000,"
                << rank(p99, .5) << ',' << rank(p99, 1) << ',' << rank(p999, .5)
                << ',' << rank(p999, 1) << ',' << misses << ',' << missRuns
                << ',' << rank(p99, 1) / (block / sr) << '\n';
              f.flush();
              raw.flush();
            }
        return 0;
      }
      if (!qualityOnly && !measurementsOnly && !costsOnly)
        productReports(argv[1], local);
#ifdef DRIFT_BBD_INSTRUMENT
      if (!timingOnly) {
        if (!costsOnly) {
          productQualityReports(argv[1], local);
          productFeedbackExperiment(argv[1], local);
        }
        if (!measurementsOnly)
          productCosts(argv[1], local);
      }
#endif
    }
    std::cout << "M2.9 product-envelope objective gates passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
