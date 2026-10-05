#include "../tests/AllocationTracker.h"
#include "../tests/reference_m23/ClockedBBDCore.h"
#include "BBDCharacterMeasurements.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
using namespace characterQualification;
using drift::BBDNonlinearTransfer;
void require(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
drift::BBDCharacterConfig config(int mode = 0) {
  auto c = example(mode == 2   ? drift::BBDCharacterMode::FullLinearCharacter
                   : mode == 1 ? drift::BBDCharacterMode::NoiseOnly
                               : drift::BBDCharacterMode::Ideal);
  c.nonlinear = {drift::BBDNonlinearityMode::EngineeringPolynomial, 1};
  return c;
}
void tests() {
  BBDNonlinearTransfer t;
  t.configure(config().nonlinear);
  std::vector<double> inputs;
  for (int i = 0; i <= 4000; ++i)
    inputs.push_back(-2 + i * .001);
  std::vector<double> reference;
  for (double x : inputs)
    reference.push_back(t.process(x));
  for (int i = 4000; i >= 0; --i)
    require(t.process(inputs[i]) == reference[i], "descending memoryless");
  std::vector<int> order(4001);
  for (int i = 0; i <= 4000; ++i)
    order[i] = i;
  std::mt19937 rng(42);
  std::shuffle(order.begin(), order.end(), rng);
  for (int i : order)
    require(t.process(inputs[i]) == reference[i], "shuffled memoryless");
  for (int i = 1; i <= 4000; ++i)
    require(reference[i] > reference[i - 1] && t.derivative(inputs[i]) > 0,
            "monotonic");
  for (double x : {-1., -.5, 0., .5, 1.})
    require(std::abs((t.process(x + 1e-6) - t.process(x - 1e-6)) / 2e-6 -
                     t.derivative(x)) < 2e-6,
            "derivative");
  for (double x : {double(NAN), double(INFINITY), double(-INFINITY),
                   double(std::numeric_limits<float>::max()),
                   -double(std::numeric_limits<float>::max()), 1e-300})
    require(std::isfinite(t.process(x)), "finite transfer");
  for (auto mode :
       {drift::BBDMode::TransportOnly, drift::BBDMode::AsyncLinearReference})
    for (auto profile : {drift::BBDFilterProfile::ValidationPrototype,
                         drift::BBDFilterProfile::HoltersParkerTable1})
      for (auto character :
           {drift::BBDCharacterMode::Ideal, drift::BBDCharacterMode::LossOnly,
            drift::BBDCharacterMode::NoiseOnly,
            drift::BBDCharacterMode::FullLinearCharacter}) {
        drift::ClockedBBDCore a, b;
        drift_m23::ClockedBBDCore old;
        auto c = example(character);
        auto d = c;
        drift_m23::BBDCharacterConfig frozen;
        frozen.mode = static_cast<drift_m23::BBDCharacterMode>(character);
        frozen.insertionDb = c.insertionDb;
        frozen.lossPerStage = c.lossPerStage;
        frozen.leakagePerStageSecond = c.leakagePerStageSecond;
        frozen.residualPolePer1024 = c.residualPolePer1024;
        frozen.inputNoiseRms = c.inputNoiseRms;
        frozen.outputNoiseRms = c.outputNoiseRms;
        frozen.mismatchFraction = c.mismatchFraction;
        frozen.seed = c.seed;
        old.setCharacterConfig(frozen);
        old.setQualificationMode(
            static_cast<drift_m23::BBDMode>(mode),
            static_cast<drift_m23::BBDFilterProfile>(profile));
        old.prepare(48000, 256);
        d.nonlinear = {drift::BBDNonlinearityMode::EngineeringPolynomial, 0};
        a.setCharacterConfig(c);
        b.setCharacterConfig(d);
        a.setQualificationMode(mode, profile);
        b.setQualificationMode(mode, profile);
        a.prepare(48000, 256);
        b.prepare(48000, 256);
        auto before = allocations.load();
        for (int n = 0; n < 10000; ++n) {
          double delay = .01 + .002 * std::sin(n * .001);
          a.setDelaySeconds(n < 5000 ? .01 : delay);
          b.setDelaySeconds(n < 5000 ? .01 : delay);
          old.setDelaySeconds(n < 5000 ? .01 : delay);
          double x = std::sin(n * .17);
          double y = a.process(x);
          require(y == b.process(x), "zero strength identity");
          require(y == old.process(x), "frozen M2.3 bit identity");
          require(a.heldOutput() == old.heldOutput(), "frozen hold identity");
          require(a.telemetry().accumulatedClockPhase ==
                      old.telemetry().accumulatedClockPhase,
                  "frozen phase identity");
          for (unsigned k = 0; k < 128; ++k)
            require(a.stageValue(k) == old.stageValue(k),
                    "frozen bucket identity");
        }
        require(allocations.load() == before, "bypass allocations");
      }
  for (unsigned stages : {256u, 512u, 1024u, 2048u, 4096u})
    for (double clock : {1., 8000., 3072000.}) {
      drift::ClockedBBDCore core;
      setup(core, stages, clock, config(), true);
      auto before = allocations.load();
      for (int n = 0; n < 3000; ++n) {
        double x = n < 1000     ? 1.
                   : n % 7 == 0 ? NAN
                   : n % 7 == 1 ? INFINITY
                   : n % 7 == 2 ? 1e-300
                                : (n % 2 ? 1 : -1) *
                                      double(std::numeric_limits<float>::max());
        require(std::isfinite(core.process(x)), "core output");
      }
      require(core.finiteState(), "core state");
      require(allocations.load() == before, "nonlinear allocations");
      require(core.telemetry().totalOutputCount ==
                  core.telemetry().totalEventCount / 2,
              "output event rate");
    }
  drift::ClockedBBDCore dc;
  setup(dc, 1024, 16000, config(), true);
  for (int n = 0; n < 96000; ++n)
    require(std::isfinite(dc.process(.5)), "long DC finite");
  require(dc.finiteState(), "long DC state");
  for (double a : {std::pow(10., -90. / 20), std::pow(10., -72. / 20), .001}) {
    require(std::abs(t.process(a) / a - 1) < .000126, "weak signal gain");
    drift::ClockedBBDCore linear, nonlinear;
    setup(linear, 1024, 16000, drift::BBDCharacterConfig{}, true);
    setup(nonlinear, 1024, 16000, config(), true);
    double error = 0;
    for (int n = 0; n < 12000; ++n) {
      double x = a * std::sin(2 * drift::pi * 500 * n / 48000);
      error =
          std::max(error, std::abs(linear.process(x) - nonlinear.process(x)));
    }
    require(error / a < .2 * a, "weak full-path convergence");
  }
}
struct Measurement {
  double rms = 0, dc = 0, peak = 0;
  std::array<double, 5> h{};
  double residual = 0;
};
Measurement measure(double amplitude, double hz, unsigned stages, double clock,
                    int path, bool noise = false) {
  constexpr double rate = 96000;
  constexpr int count = 96000;
  drift::ClockedBBDCore core;
  auto c = config(noise ? 1 : 0);
  setup(core, stages, clock, c, path == 2, rate);
  BBDNonlinearTransfer t;
  t.configure(c.nonlinear);
  std::array<std::complex<double>, 5> sums{}, rot{}, phase{};
  for (int k = 0; k < 5; ++k) {
    rot[k] = std::polar(1., -2 * drift::pi * hz * (k + 1) / rate);
    phase[k] = 1;
  }
  Measurement m;
  int warm = int(rate * (stages / (2 * clock) + .1));
  for (int n = 0; n < warm + count; ++n) {
    double x = amplitude * std::sin(2 * drift::pi * hz * n / rate),
           y = path ? core.process(x) : t.process(x);
    if (n >= warm) {
      m.rms += y * y;
      m.dc += y;
      m.peak = std::max(m.peak, std::abs(y));
      for (int k = 0; k < 5; ++k) {
        sums[k] += y * phase[k];
        phase[k] *= rot[k];
      }
    }
  }
  m.rms = std::sqrt(m.rms / count);
  m.dc /= count;
  double power = 0;
  for (int k = 0; k < 5; ++k) {
    m.h[k] = 2 * std::abs(sums[k]) / count;
    power += m.h[k] * m.h[k] / 2;
  }
  m.residual = std::sqrt(std::max(0., m.rms * m.rms - m.dc * m.dc - power));
  return m;
}
void header(std::ostream &s) {
  s << "path,stages,clock_hz,input_db,hz,rms,fundamental_gain,thd_h2_h5,thdn_"
       "ac,h2,h3,h4,h5,dc,peak,noise_enabled\n";
}
void row(std::ostream &s, int path, unsigned stages, double clock, double db,
         double hz, bool noise = false) {
  auto m = measure(std::pow(10., db / 20), hz, stages, clock, path, noise);
  double p = 0;
  for (int k = 1; k < 5; ++k)
    p += m.h[k] * m.h[k];
  s << path << ',' << stages << ',' << clock << ',' << db << ',' << hz << ','
    << m.rms << ',' << m.h[0] / std::pow(10., db / 20) << ','
    << std::sqrt(p) / m.h[0] << ','
    << std::sqrt(p + 2 * m.residual * m.residual) / m.h[0];
  for (int k = 1; k < 5; ++k)
    s << ',' << m.h[k];
  s << ',' << m.dc << ',' << m.peak << ',' << noise << '\n';
  if (path == 0 && db <= 0) {
    double a = std::pow(10., db / 20);
    require(std::abs(m.h[1] - a * a / 16) < 1e-10, "H2 analytical");
    require(std::abs(m.h[2] - a * a * a / 72) < 1e-10, "H3 analytical");
  }
}
struct Timing {
  double seconds, events, evaluations;
};
Timing timing(unsigned stages, double delay, int mode, int voices) {
  std::array<drift::ClockedBBDCore, 8> cores;
  std::array<drift_m23::ClockedBBDCore, 8> old;
  for (int v = 0; v < voices; ++v) {
    auto c = mode == 0 ? example() : config(mode == 2 ? 2 : 0);
    setup(cores[v], stages, stages / (2 * delay), c, true);
    if (mode == 0) {
      drift_m23::BBDCharacterConfig frozen;
      frozen.mode = drift_m23::BBDCharacterMode::FullLinearCharacter;
      frozen.insertionDb = c.insertionDb;
      frozen.lossPerStage = c.lossPerStage;
      frozen.leakagePerStageSecond = c.leakagePerStageSecond;
      frozen.residualPolePer1024 = c.residualPolePer1024;
      frozen.inputNoiseRms = c.inputNoiseRms;
      frozen.outputNoiseRms = c.outputNoiseRms;
      frozen.mismatchFraction = c.mismatchFraction;
      frozen.seed = c.seed;
      old[v].setCharacterConfig(frozen);
      old[v].setQualificationMode(
          drift_m23::BBDMode::AsyncLinearReference,
          drift_m23::BBDFilterProfile::HoltersParkerTable1);
      old[v].prepare(48000, stages);
      old[v].setDelaySeconds(delay);
    }
  }
  auto begin = std::chrono::steady_clock::now();
  volatile double sink = 0;
  for (int n = 0; n < 12000; ++n)
    for (int v = 0; v < voices; ++v)
      sink = mode == 0 ? old[v].process(.1) : cores[v].process(.1);
  (void)sink;
  double seconds = std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - begin)
                       .count(),
         events = 0, evals = 0;
  for (int v = 0; v < voices; ++v) {
    events += mode == 0 ? old[v].telemetry().totalEventCount
                        : cores[v].telemetry().totalEventCount;
    evals += cores[v].deviceCharacter().nonlinear.enabled() ?
                 cores[v].telemetry().totalOutputCount : 0;
  }
  return {seconds, events, evals};
}
int main(int argc, char **argv) {
  try {
    tests();
    for (double db : {-90., -72., -60., -24., -12., -6., 0.}) {
      double a = std::pow(10., db / 20);
      auto m = measure(a, 500, 1024, 16000, 0);
      require(std::abs(m.h[0] - (a - a * a * a / 24)) < 1e-10,
              "fundamental analytical");
      require(std::abs(m.h[1] - a * a / 16) < 1e-10, "H2 analytical");
      require(std::abs(m.h[2] - a * a * a / 72) < 1e-10, "H3 analytical");
      require(m.h[3] < 1e-10 && m.h[4] < 1e-10, "H4 H5 cubic");
      require(std::abs(m.dc + a * a / 16) < 1e-10, "DC analytical");
    }
    auto transported = measure(1, 500, 1024, 16000, 1);
    for (int k = 1; k <= 3; ++k) {
      double hz = 500 * k;
      double hold = std::sin(drift::pi * hz / 16000) /
                    (6 * std::sin(drift::pi * hz / 96000));
      double intrinsic = k == 1 ? 23. / 24 : k == 2 ? 1. / 16 : 1. / 72;
      require(std::abs(transported.h[k - 1] - intrinsic * hold) < 1e-10,
              "transport nonlinear harmonic/hold reference");
    }
    if (argc > 1 && std::string(argv[1]) == "--tests-only") {
      std::cout << "Nonlinear numerical/allocation tests passed\n";
      return 0;
    }
    std::filesystem::path dir =
        argc > 1 ? argv[1] : "bbd_nonlinear_qualification";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
      return 1;
    const char *names[] = {"bbd_nonlinear_transfer_curve.csv",
                           "bbd_nonlinear_level_sweep.csv",
                           "bbd_nonlinear_harmonics.csv",
                           "bbd_nonlinear_frequency_sweep.csv",
                           "bbd_nonlinear_stage_sweep.csv",
                           "bbd_nonlinear_clock_sweep.csv",
                           "bbd_nonlinear_aliasing.csv",
                           "bbd_nonlinear_feedback_probe.csv",
                           "bbd_nonlinear_performance.csv",
                           "bbd_nonlinear_multivoice_performance.csv",
                           "README.txt"};
    std::array<std::ofstream, 11> out;
    for (int i = 0; i < 11; ++i) {
      out[i].open(dir / names[i]);
      out[i] << std::setprecision(17);
      require(bool(out[i]), "artifact open");
    }
    out[10] << "DriftBrigade-M2.4-BBD-Nonlinear-Qualification\n";
    out[10].flush();
    require(bool(out[10]), "initial flush");
    BBDNonlinearTransfer t;
    t.configure(config().nonlinear);
    out[0] << "input,output,derivative,residual_from_linear,mode,stage_count,"
              "paper_reference\n";
    for (unsigned stages : {256u, 512u, 1024u, 2048u, 4096u})
      for (int i = 0; i <= 4000; ++i) {
        double x = -2 + i * .001, z = std::clamp(x, -1., 1.);
        out[0] << x << ',' << t.process(x) << ',' << t.derivative(x) << ','
               << t.process(x) - x << ",EngineeringPolynomial," << stages << ','
               << (x > 1    ? 1 - 1. / 8 - 1. / 18
                   : x < -1 ? -1 - 1. / 8 + 1. / 18
                            : z - z * z / 8 - z * z * z / 18 + 1. / 8)
               << '\n';
      }
    for (int i = 1; i <= 5; ++i)
      header(out[i]);
    for (double db : {-90., -72., -60., -48., -36., -24., -18., -12., -9., -6.,
                      -3., -1., 0., 3., 6.})
      for (int path : {0, 1, 2}) {
        row(out[1], path, 1024, 16000, db, 500);
        row(out[2], path, 1024, 16000, db, 500);
      }
    for (double db : {-24., -12., -6., 0.})
      row(out[1], 2, 1024, 16000, db, 500, true);
    for (double db : {-24., -12., -6., 0.})
      for (double hz : {50., 100., 250., 500., 1000., 2000., 4000.})
        for (int path : {0, 2})
          row(out[3], path, 1024, 16000, db, hz);
    for (unsigned stages : {256u, 512u, 1024u, 2048u, 4096u})
      for (int path : {0, 2})
        row(out[4], path, stages, 16000, 0, 500);
    for (double clock : {8000., 16000., 32000.})
      for (int path : {0, 1, 2})
        row(out[5], path, 1024, clock, 0, 500);
    out[6] << "path,input_db,ratio,harmonic,generated_hz,folded_hz,image_lower_"
              "hz,image_upper_hz,generated_amplitude,folded_composite,image_"
              "lower_composite,image_upper_composite,hold_sinc,output_filter_"
              "magnitude\n";
    for (double db : {-24., -12., -6., 0.})
      for (double ratio : {.05, .1, .2, .3, .4})
        for (int path : {1, 2}) {
          constexpr double clock = 8000, rate = 96000;
          const double hz = ratio * clock, a = std::pow(10., db / 20);
          drift::ClockedBBDCore core;
          setup(core, 256, clock, config(), path == 2, rate);
          std::array<double, 15> f{};
          std::array<std::complex<double>, 15> sum{}, ph{}, rot{};
          for (int k = 0; k < 5; ++k) {
            double folded = std::abs(std::remainder((k + 1) * hz, clock));
            f[3 * k] = folded;
            f[3 * k + 1] = clock - folded;
            f[3 * k + 2] = clock + folded;
          }
          for (int k = 0; k < 15; ++k) {
            ph[k] = 1;
            rot[k] = std::polar(1., -2 * drift::pi * f[k] / rate);
          }
          for (int n = 0; n < 33600; ++n) {
            double y =
                core.process(a * std::sin(2 * drift::pi * hz * n / rate));
            if (n >= 9600)
              for (int k = 0; k < 15; ++k) {
                sum[k] += y * ph[k];
                ph[k] *= rot[k];
              }
          }
          drift::AsyncAnalogFilter filter;
          filter.paperReference(true);
          drift::AsyncAnalogFilter in;
          in.paperReference(false);
          double effective = a * (path == 2 ? std::abs(in.response(hz)) : 1);
          for (int k = 0; k < 5; ++k) {
            double gen =
                k == 0   ? effective - effective * effective * effective / 24
                : k == 1 ? effective * effective / 16
                : k == 2 ? effective * effective * effective / 72
                         : 0;
            out[6] << path << ',' << db << ',' << ratio << ',' << k + 1 << ','
                   << (k + 1) * hz << ',' << f[3 * k] << ',' << f[3 * k + 1]
                   << ',' << f[3 * k + 2] << ',' << gen;
            for (int j = 0; j < 3; ++j)
              out[6] << ','
                     << (f[3 * k + j] == 0 ? 1 : 2) * std::abs(sum[3 * k + j]) /
                            24000;
            double r = f[3 * k] / clock;
            out[6] << ',' << (r ? std::sin(drift::pi * r) / (drift::pi * r) : 1)
                   << ','
                   << (path == 2 ? std::abs(filter.response(f[3 * k])) : 1)
                   << '\n';
          }
        }
    out[7] << "path,mode,feedback,stimulus,block,peak,rms,dc,h1,h2,h3,h4,h5,"
              "noise_floor_rms,finite,rms_ratio,decay_db_per_second\n";
    for (int path : {1, 2})
      for (int mode : {0, 1, 2})
        for (double gain : {0., .5, .8, .9, .95})
          for (int stimulus : {0, 1, 2}) {
            drift::ClockedBBDCore core;
            setup(core, 256, 16000, config(mode), path == 2);
            double previous = 0, previousRms = 0;
            for (int block = 0; block < 200; ++block) {
              double peak = 0, power = 0, dc = 0;
              std::array<std::complex<double>, 5> sums{};
              for (int j = 0; j < 480; ++j) {
                int n = block * 480 + j;
                double input =
                    stimulus == 0 ? (n == 1 ? .5 : 0)
                    : n < 480     ? (stimulus == 1 ? .1 : .5) *
                                    std::sin(2 * drift::pi * 500 * n / 48000)
                              : 0;
                previous = core.process(input + gain * previous);
                peak = std::max(peak, std::abs(previous));
                power += previous * previous;
                dc += previous;
                for (int k = 0; k < 5; ++k)
                  sums[k] += previous * std::polar(1., -2 * drift::pi * 500 *
                                                           (k + 1) * n / 48000);
              }
              out[7] << path << ',' << mode << ',' << gain << ',' << stimulus
                     << ',' << block << ',' << peak << ','
                     << std::sqrt(power / 480) << ',' << dc / 480;
              double harmonicPower = 0;
              for (auto z : sums) {
                double h = 2 * std::abs(z) / 480;
                out[7] << ',' << h;
                harmonicPower += h * h / 2;
              }
              out[7] << ','
                     << std::sqrt(std::max(0., power / 480 -
                                                   dc * dc / (480 * 480) -
                                                   harmonicPower))
                     << ',' << core.finiteState() << ',';
              const double rms = std::sqrt(power / 480);
              if (previousRms > 0 && rms > 0)
                out[7] << rms / previousRms << ','
                       << 2000 * (std::log10(rms) - std::log10(previousRms));
              else
                out[7] << ',';
              out[7] << '\n';
              previousRms = rms;
              require(core.finiteState(), "feedback finite");
            }
          }
    for (int file : {8, 9}) {
      out[file] << "stages,delay_ms,voices,mode,realtime_factor,evaluations_"
                   "per_second,physical_events_per_second,overhead_percent_vs_"
                   "linear,cpu_factor_vs_stereo_baseline\n";
      for (unsigned stages : {512u, 1024u, 2048u, 4096u})
        for (double delay : {.003, .01, .03}) {
          auto baseline = timing(stages, delay, 0, 2);
          int voices = file == 8 ? 2 : 8;
          auto base = voices == 2 ? baseline : timing(stages, delay, 0, 8);
          for (int mode : {0, 1, 2}) {
            std::array<Timing, 5> trials;
            for (auto &trial : trials)
              trial = timing(stages, delay, mode, voices);
            std::sort(trials.begin(), trials.end(),
                      [](auto a, auto b) { return a.seconds < b.seconds; });
            auto m = trials[2];
            out[file] << stages << ',' << delay * 1000 << ',' << voices << ','
                      << mode << ',' << m.seconds / .25 << ','
                      << m.evaluations / .25 << ',' << m.events / .25 << ','
                      << 100 * (m.seconds / base.seconds - 1) << ','
                      << m.seconds / baseline.seconds << '\n';
          }
        }
    }
    out[10]
        << "ENGINEERING APPROXIMATION: strength 1, x-x^2/8-x^3/18, nominal "
           "|x|<=1; rational C1 endpoint extension. Defaults disabled/zero. "
           "Stage and clock independent. Paper reference column includes +1/8 "
           "and constant endpoint branches; not the runtime model. No voltage "
           "calibration or fitted measured data.\n"
        << "Paths: 0 element,1 transport+hold,2 Table1 full async path. Noise "
           "off in spectral sweeps. H2-H5 are absolute peak amplitudes; THD "
           "uses these four bins only; THD+N is AC residual plus harmonics, "
           "including aliases/images. 96kHz host, coherent 1s analysis after "
           "delay+100ms settling. Levels above 0 exercise extension. Harmonic "
           "analytical checks <=1e-10.\n"
        << "Aliasing rows: generated amplitudes are analytic at nonlinear "
           "element (full path uses input filter response); folded/image "
           "amplitudes are measured composite bins. Coincident harmonics, "
           "fundamental, DC and images interfere; rows must NOT be summed or "
           "attributed individually. H4/H5 generated zero in cubic domain; "
           "their measured bins can contain aliases of H2/H3. Hold sinc and "
           "output-filter columns separate attenuation. No oversampling or "
           "additional anti-alias filter.\n"
        << "Feedback: paths 1 transport/hold and 2 Table1 async;10ms "
           "blocks,2s,impulse/.1 burst/.5 burst; h1-h5 at 500Hz multiples. "
           "Residual noise column includes broadband transient/limit-cycle "
           "energy; it is not a calibrated stochastic noise floor. Blank decay "
           "cells indicate zero current/previous RMS; negative dB/sec "
           "indicates decay. Tail "
           "trajectories enable decay/DC/limit-cycle inspection, not proof of "
           "stability for every signal.\n"
        << "Performance:48kHz,250ms per trial,median of 5; mode0 M2.3 "
           "FullLinearCharacter,1 NonlinearOnly,2 FullCharacter. Wall/audio "
           "lower is better. Event/evaluation rates are per audio second. "
           "CPU factor relative to stereo linear baseline (both schemas). Independent "
           "eight voices,not eight worker threads. Timing informational,never "
           "CI gated.\n"
        << "Tests: monotonic/derivative/memoryless,frozen M2.3 bit-identical "
           "disabled/zero-strength bypass,finite extremes,allocations,weak "
           "signal,output-rate evaluations. M2.3 frozen-reference "
           "qualification remains separate. Production DigitalFractionalDelay "
           "unchanged.\n";
    bool ok = true;
    for (auto &s : out) {
      s.flush();
      bool written = bool(s);
      s.close();
      ok = ok && written && !s.fail();
    }
    require(ok, "artifact finalization");
    std::cout << "Nonlinear qualification wrote " << dir << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
