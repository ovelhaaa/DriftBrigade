#include "../tests/AllocationTracker.h"
#include "../tests/CompanderTests.h"
#include "BBDCharacterMeasurements.h"
#include <chrono>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace companderQualification;
constexpr double rate = 48000, dt = 1 / rate;
double amplitude(double db) { return std::pow(10., db / 20); }
double dbRatio(double x) { return 20 * std::log10(std::max(x, 1e-300)); }

struct Metrics {
  int count = 0;
  double power = 0, dc = 0, peak = 0, errorPower = 0, errorPeak = 0,
         referencePower = 0, cross = 0;
  std::array<std::complex<double>, 5> spectrum{}, errorSpectrum{}, phase{},
      rotation{};
  explicit Metrics(double hz = 500) {
    for (int k = 0; k < 5; ++k) {
      phase[k] = 1;
      rotation[k] = std::polar(1., -2 * drift::pi * hz * (k + 1) / rate);
    }
  }
  void add(double y, double reference = 0) {
    ++count;
    power += y * y;
    dc += y;
    peak = std::max(peak, std::abs(y));
    const double error = y - reference;
    errorPower += error * error;
    errorPeak = std::max(errorPeak, std::abs(error));
    referencePower += reference * reference;
    cross += y * reference;
    for (int k = 0; k < 5; ++k) {
      spectrum[k] += y * phase[k];
      errorSpectrum[k] += error * phase[k];
      phase[k] *= rotation[k];
    }
  }
  double rms() const { return std::sqrt(power / count); }
  double errorRms() const { return std::sqrt(errorPower / count); }
  double harmonic(int k) const { return 2 * std::abs(spectrum[k]) / count; }
  double thd() const {
    double p = 0;
    for (int k = 1; k < 5; ++k)
      p += harmonic(k) * harmonic(k);
    return std::sqrt(p) / std::max(harmonic(0), 1e-300);
  }
};

void staticRatios(std::ostream &out) {
  out << "cap_uF,range,input_db,compressor_output_db,compressor_local_slope,"
         "compressor_analytic_db,expander_output_db,expander_local_slope,"
         "expander_analytic_db,roundtrip_error,low_frequency_hz,low_frequency_"
         "comp_rms,low_frequency_exp_rms\n";
  for (double cap : {.01e-6, .22e-6, .47e-6, 1e-6, 10e-6}) {
    double prevDb = 0, prevC = 0, prevE = 0;
    bool first = true;
    for (double db :
         {-80., -70., -60., -50., -40., -30., -24., -18., -12., -6., 0.}) {
      drift::BBDFeedbackCompressor c;
      drift::BBDFeedforwardExpander e, round;
      c.configure(compConfig(cap));
      e.configure(compConfig(cap));
      round.configure(compConfig(cap));
      double yc = 0, ye = 0, z = 0;
      const double x = amplitude(db);
      const int settle = int(100 * 10000 * cap * rate) + 1000;
      for (int n = 0; n < settle; ++n) {
        yc = c.process(x, dt);
        ye = e.process(x, dt);
        z = round.process(yc, dt);
      }
      const double cdb = dbRatio(yc), edb = dbRatio(ye);
      out << cap * 1e6 << ','
          << (cap < .22e-6 || cap > 1e-6 ? "ENGINEERING_OUTSIDE_TYPICAL"
                                         : "PAPER_RANGE")
          << ',' << db << ',' << cdb << ',';
      if (!first)
        out << (cdb - prevC) / (db - prevDb);
      out << ',' << db * .5 << ',' << edb << ',';
      if (!first)
        out << (edb - prevE) / (db - prevDb);
      // 0.25 Hz sinusoid has zero crossings and detector ripple; its RMS is
      // descriptive, not a slope oracle.
      c.reset();
      e.reset();
      double pc = 0, pe = 0;
      for (int n = 0; n < 240000; ++n) {
        double v = x * std::sin(2 * drift::pi * .25 * n / rate),
               a = c.process(v, dt), b = e.process(v, dt);
        if (n >= 48000) {
          pc += a * a;
          pe += b * b;
        }
      }
      out << ',' << 2 * db << ',' << z - x << ",0.25," << std::sqrt(pc / 192000)
          << ',' << std::sqrt(pe / 192000) << '\n';
      require(std::abs(cdb - db * .5) < 1e-8 && std::abs(edb - 2 * db) < 1e-8,
              "analytical static ratio artifact");
      if (!first)
        require(std::abs((cdb - prevC) / (db - prevDb) - .5) < 1e-9 &&
                    std::abs((edb - prevE) / (db - prevDb) - 2) < 1e-9,
                "static local slopes");
      first = false;
      prevDb = db;
      prevC = cdb;
      prevE = edb;
    }
  }
}

double stimulus(int kind, int n) {
  const double t = n / rate, sine = std::sin(2 * drift::pi * 500 * t);
  switch (kind) {
  case 0:
    return .01;
  case 1:
    return .1 * sine;
  case 2:
    return (t < .25 ? .01 : t < .75 ? .5 : .01) * sine;
  case 3:
    return (.005 + .495 * std::min(t, 1.)) * sine;
  case 4:
    return (t >= .25 && t < .5 ? .5 : 0) * sine;
  case 5:
    return .2 * (1 + .8 * std::sin(2 * drift::pi * 3 * t)) * sine;
  case 6:
    return (.02 + .15 * (1 + std::sin(2 * drift::pi * 1.7 * t))) *
           (.6 * sine + .25 * std::sin(2 * drift::pi * 713 * t) +
            .15 * std::sin(2 * drift::pi * 1301 * t));
  case 7:
    return n == int(.25 * rate) ? .5 : 0;
  default:
    return (int(t * 8) % 2 ? .01 : .5) * sine;
  }
}
const char *stimulusName(int kind) {
  const char *names[] = {"DC",
                         "sine",
                         "amplitude_step",
                         "slow_ramp",
                         "tone_burst",
                         "AM_sine",
                         "synthetic_program",
                         "impulse",
                         "rhythmic_envelope"};
  return names[kind];
}

void roundTrips(std::ostream &out, std::ostream &sweep) {
  out << "cap_uF,stimulus,peak_error,rms_error,gain_error_db,envelope_mismatch_"
         "peak,output_dc,error_dc,recovery_first_sample_after_transition,error_"
         "h1,error_h2,error_h3,error_h4,error_h5\n";
  sweep << "cap_uF,tau_ms,range,stimulus,roundtrip_peak_error,compressor_peak,"
           "final_peak,max_gain,peak_envelope_mismatch\n";
  for (double cap : {.01e-6, .22e-6, .47e-6, 1e-6, 10e-6})
    for (int kind = 0; kind < 9; ++kind) {
      drift::BBDFeedbackCompressor c;
      drift::BBDFeedforwardExpander e;
      c.configure(compConfig(cap));
      e.configure(compConfig(cap));
      Metrics m;
      double mismatch = 0, dcError = 0, compPeak = 0, gain = 0;
      for (int n = 0; n < 72000; ++n) {
        const double x = stimulus(kind, n), compressed = c.process(x, dt),
                     y = e.process(compressed, dt);
        m.add(y, x);
        dcError += y - x;
        compPeak = std::max(compPeak, std::abs(compressed));
        gain = std::max(gain, c.currentGain());
        mismatch = std::max(mismatch, std::abs(c.levelAverager().value() -
                                               e.levelAverager().value()));
      }
      const double gainError =
          m.referencePower ? dbRatio(std::sqrt(m.power / m.referencePower)) : 0;
      require(m.errorPeak < 2e-12, "ideal controlled roundtrip error");
      out << cap * 1e6 << ',' << stimulusName(kind) << ',' << m.errorPeak << ','
          << m.errorRms() << ',' << gainError << ',' << mismatch << ','
          << m.dc / m.count << ',' << dcError / m.count << ",0";
      for (auto z : m.errorSpectrum)
        out << ',' << 2 * std::abs(z) / m.count;
      out << '\n';
      sweep << cap * 1e6 << ',' << 10000 * cap * 1000 << ','
            << (cap < .22e-6 || cap > 1e-6 ? "ENGINEERING_OUTSIDE_TYPICAL"
                                           : "PAPER_RANGE")
            << ',' << stimulusName(kind) << ',' << m.errorPeak << ','
            << compPeak << ',' << m.peak << ',' << gain << ',' << mismatch
            << '\n';
    }
}

void ripple(std::ostream &out) {
  out << "cap_uF,hz,detector,mean_level,peak_to_peak,ripple_over_mean,gain_"
         "modulation_depth\n";
  for (double cap : {.22e-6, .47e-6, 1e-6})
    for (double hz : {20., 50., 100., 200., 500., 1000.}) {
      drift::CompanderLevelAverager avg;
      drift::BBDFeedbackCompressor c;
      drift::BBDFeedforwardExpander e;
      avg.configure(compConfig(cap));
      c.configure(compConfig(cap));
      e.configure(compConfig(cap));
      double sum[3]{}, lo[3]{1e300, 1e300, 1e300}, hi[3]{},
          glo[3]{1e300, 1e300, 1e300}, ghi[3]{};
      for (int n = 0; n < 72000; ++n) {
        const double x = .25 * std::sin(2 * drift::pi * hz * n / rate);
        const double a = avg.processMagnitude(std::abs(x), dt);
        c.process(x, dt);
        e.process(x, dt);
        const double levels[] = {a, c.levelAverager().value(),
                                 e.levelAverager().value()},
                     gains[] = {a, c.currentGain(), e.currentGain()};
        if (n >= 24000)
          for (int k = 0; k < 3; ++k) {
            sum[k] += levels[k];
            lo[k] = std::min(lo[k], levels[k]);
            hi[k] = std::max(hi[k], levels[k]);
            glo[k] = std::min(glo[k], gains[k]);
            ghi[k] = std::max(ghi[k], gains[k]);
          }
      }
      for (int k = 0; k < 3; ++k)
        out << cap * 1e6 << ',' << hz << ','
            << (k == 0   ? "reference_rectifier"
                : k == 1 ? "feedback_compressor"
                         : "feedforward_expander")
            << ',' << sum[k] / 48000 << ',' << hi[k] - lo[k] << ','
            << (hi[k] - lo[k]) / (sum[k] / 48000) << ','
            << (ghi[k] - glo[k]) / (ghi[k] + glo[k]) << '\n';
    }
}

void transients(std::ostream &out) {
  out << "cap_uF,case,sample,time_seconds,input,compressor_rectified_output,"
         "compressor_detector,compressor_gain,compressor_output,expander_"
         "rectified_input,expander_detector,expander_gain,final_output\n";
  const char *names[] = {"silence_to_minus24", "silence_to_zero",
                         "minus40_to_minus6",  "minus6_to_minus40",
                         "tone_burst",         "alternating_bursts"};
  for (double cap : {.22e-6, .47e-6, 1e-6})
    for (int kind = 0; kind < 6; ++kind) {
      drift::BBDFeedbackCompressor c;
      drift::BBDFeedforwardExpander e;
      c.configure(compConfig(cap));
      e.configure(compConfig(cap));
      for (int n = 0; n < 24000; ++n) {
        const double t = n / rate;
        const double a = kind == 0   ? (t < .2 ? 0 : amplitude(-24))
                         : kind == 1 ? (t < .2 ? 0 : 1)
                         : kind == 2 ? (t < .2 ? .01 : amplitude(-6))
                         : kind == 3 ? (t < .2 ? amplitude(-6) : .01)
                         : kind == 4 ? (t >= .2 && t < .3 ? .5 : 0)
                                     : (int(t * 20) % 2 ? .5 : .01);
        const double x = kind < 4 ? a : a * std::sin(2 * drift::pi * 500 * t),
                     y = c.process(x, dt), z = e.process(y, dt);
        out << cap * 1e6 << ',' << names[kind] << ',' << n << ',' << t << ','
            << x << ',' << std::abs(y) << ',' << c.levelAverager().value()
            << ',' << c.currentGain() << ',' << y << ',' << std::abs(y) << ','
            << e.levelAverager().value() << ',' << e.currentGain() << ',' << z
            << '\n';
      }
    }
}

struct Path {
  drift::BBDCompanderQualificationChain chain;
  int mode = 0;
  explicit Path(int selected, double cap, unsigned stages, double delay,
                drift::BBDCharacterConfig cc, bool filters = true)
      : mode(selected) {
    auto c = compConfig(cap);
    c.enabled = mode == 2;
    chain.configure(c);
    chain.core.setCharacterConfig(cc);
    chain.core.setQualificationMode(
        filters ? drift::BBDMode::AsyncLinearReference
                : drift::BBDMode::TransportOnly,
        drift::BBDFilterProfile::HoltersParkerTable1);
    chain.prepare(rate, stages);
    chain.setDelaySeconds(delay);
    chain.core.collectOperatingStats = true;
  }
  double process(double x) {
    if (mode == 1)
      return chain.core.process(chain.compressor.process(x, dt));
    return chain.process(x);
  }
};

void noiseAndNonlinear(std::ostream &noise, std::ostream &nonlinear) {
  noise << "cap_uF,input_db,mode,signal_rms,noise_difference_rms,snr_db,snr_"
           "improvement_db,compressor_level_mean,expander_level_mean,gain_"
           "modulation_depth,noise_induced_expander_gain_difference_rms,seed\n";
  nonlinear
      << "case,input_db,bbd_input_peak,bbd_input_rms,bbd_input_mean_magnitude,"
         "bbd_input_detector_average,nominal_abs_le1_fraction,useful_abs_0p1_"
         "to1_fraction,bbd_intrinsic_thd_h2_h5,nonlinear_added_h2_h5_over_"
         "input_h1,output_rms,fundamental_gain,restored_gain_db,h2,h3,h4,h5,"
         "thd_h2_h5,noise_difference_rms,snr_db,output_dc\n";
  for (double cap : {.22e-6, .47e-6, 1e-6})
    for (double db : {-60., -48., -36., -24., -12., -6., 0.}) {
      double baseline = 0;
      for (int mode = 0; mode < 3; ++mode) {
        drift::BBDCharacterConfig cc;
        cc.mode = drift::BBDCharacterMode::NoiseOnly;
        cc.outputNoiseRms = 1e-4;
        cc.seed = 991;
        auto clean = cc;
        clean.outputNoiseRms = 0;
        Path noisy(mode, cap, 1024, .01, cc),
            quiet(mode, cap, 1024, .01, clean);
        Metrics signal, residual;
        double sumC = 0, sumE = 0, lo = 1e300, hi = 0, gainDifferencePower = 0;
        for (int n = 0; n < 72000; ++n) {
          const double x = amplitude(db) *
                           std::sin(2 * drift::pi * 500 * n / rate),
                       y = noisy.process(x), s = quiet.process(x);
          if (n >= 24000) {
            signal.add(s);
            residual.add(y - s);
            sumC += noisy.chain.compressor.levelAverager().value();
            sumE += noisy.chain.expander.levelAverager().value();
            const double g = noisy.chain.expander.currentGain();
            lo = std::min(lo, g);
            hi = std::max(hi, g);
            const double d = g - quiet.chain.expander.currentGain();
            gainDifferencePower += d * d;
          }
        }
        const double snr = dbRatio(signal.rms() / residual.rms());
        if (mode == 0)
          baseline = snr;
        require(std::isfinite(snr), "finite noise SNR");
        if (mode == 2 && db == -60)
          require(snr - baseline > 25, "weak-signal BBD noise reduction");
        noise << cap * 1e6 << ',' << db << ','
              << (mode == 0   ? "no_compander"
                  : mode == 1 ? "compressor_only"
                              : "full_compander")
              << ',' << signal.rms() << ',' << residual.rms() << ',' << snr
              << ',' << snr - baseline << ',' << sumC / 48000 << ','
              << sumE / 48000 << ',' << (hi - lo) / (hi + lo) << ','
              << std::sqrt(gainDifferencePower / 48000) << ",991\n";
      }
    }
  for (double db : {-60., -48., -36., -24., -12., -6., 0.})
    for (int kind = 0; kind < 5; ++kind) {
      auto cc = kind == 4 ? characterQualification::example()
                          : drift::BBDCharacterConfig{};
      if (kind == 1 || kind == 3 || kind == 4)
        cc.nonlinear = {drift::BBDNonlinearityMode::EngineeringPolynomial, 1};
      auto clean = cc;
      clean.outputNoiseRms = 0;
      clean.inputNoiseRms = 0;
      cc.mode = kind == 4 ? drift::BBDCharacterMode::FullLinearCharacter
                          : drift::BBDCharacterMode::NoiseOnly;
      cc.outputNoiseRms = 1e-4;
      cc.seed = 991;
      Path noisy(kind >= 2 ? 2 : 0, .47e-6, 1024, .01, cc),
          quiet(kind >= 2 ? 2 : 0, .47e-6, 1024, .01, clean);
      Metrics output, residual, intrinsic, nonlinearInput, added;
      drift::CompanderLevelAverager inputDetector;
      inputDetector.configure(compConfig());
      double inputAverage = 0;
      for (int n = 0; n < 72000; ++n) {
        const double x = amplitude(db) *
                         std::sin(2 * drift::pi * 500 * n / rate),
                     y = noisy.process(x), s = quiet.process(x);
        if (n == 23999) {
          noisy.chain.core.operatingStats = {};
          quiet.chain.core.operatingStats = {};
        }
        const double detector = inputDetector.processMagnitude(
            std::abs(quiet.chain.core.operatingStats.lastInput), dt);
        if (n >= 24000) {
          output.add(s);
          residual.add(y - s);
          intrinsic.add(quiet.chain.core.operatingStats.lastNonlinearOutput);
          nonlinearInput.add(
              quiet.chain.core.operatingStats.lastNonlinearInput);
          added.add(quiet.chain.core.operatingStats.lastNonlinearOutput -
                    quiet.chain.core.operatingStats.lastNonlinearInput);
          inputAverage += detector;
        }
      }
      auto stats = quiet.chain.core.operatingStats;
      const char *names[] = {"A_linear_no_compander",
                             "B_nonlinear_no_compander", "C_linear_compander",
                             "D_nonlinear_compander",
                             "E_full_character_nonlinear_compander"};
      double addedPower = 0;
      for (int k = 1; k < 5; ++k)
        addedPower += added.harmonic(k) * added.harmonic(k);
      nonlinear << names[kind] << ',' << db << ',' << stats.peak << ','
                << std::sqrt(stats.sumSquares / stats.count) << ','
                << stats.sumMagnitude / stats.count << ','
                << inputAverage / 48000 << ','
                << double(stats.nominalCount) / stats.count << ','
                << double(stats.usefulCount) / stats.count << ','
                << intrinsic.thd() << ','
                << std::sqrt(addedPower) / nonlinearInput.harmonic(0) << ','
                << output.rms() << ',' << output.harmonic(0) / amplitude(db)
                << ',' << dbRatio(output.harmonic(0) / amplitude(db));
      for (int k = 1; k < 5; ++k)
        nonlinear << ',' << output.harmonic(k);
      nonlinear << ',' << output.thd() << ',' << residual.rms() << ','
                << dbRatio(output.rms() / residual.rms()) << ','
                << output.dc / output.count << '\n';
    }
}

void delayAlignment(std::ostream &out) {
  out << "delay_ms,effective_delay_ms,stimulus,filters,lag_host_samples,peak_"
         "error,rms_error,envelope_mismatch_rms,input_onset_sample,output_"
         "onset_sample\n";
  for (double delay : {.003, .01, .03, .1, .3})
    for (int kind : {7, 4, 2, 8})
      for (bool filters : {false, true}) {
        Path p(2, .47e-6, 1024, delay, drift::BBDCharacterConfig{}, filters);
        constexpr int count = 96000;
        std::vector<double> x(count), y(count), c(count), e(count);
        int inOnset = -1, outOnset = -1;
        for (int n = 0; n < count; ++n) {
          x[n] = stimulus(kind, n);
          y[n] = p.process(x[n]);
          c[n] = p.chain.compressor.levelAverager().value();
          e[n] = p.chain.expander.levelAverager().value();
          if (inOnset < 0 && std::abs(x[n]) > 1e-8)
            inOnset = n;
          if (outOnset < 0 && std::abs(y[n]) > 1e-8)
            outOnset = n;
        }
        // Search +/- 16 samples around physical delay; no sidechain delay is
        // applied.
        int bestLag = int(delay * rate);
        double bestError = 1e300;
        for (int lag = std::max(0, bestLag - 16); lag <= int(delay * rate) + 16;
             ++lag) {
          double power = 0;
          for (int n = lag; n < count; ++n)
            power += (y[n] - x[n - lag]) * (y[n] - x[n - lag]);
          if (power < bestError) {
            bestError = power;
            bestLag = lag;
          }
        }
        double peak = 0, env = 0;
        for (int n = bestLag; n < count; ++n) {
          peak = std::max(peak, std::abs(y[n] - x[n - bestLag]));
          env += (e[n] - c[n - bestLag]) * (e[n] - c[n - bestLag]);
        }
        out << delay * 1000 << ','
            << p.chain.core.telemetry().effectiveDelaySeconds * 1000 << ','
            << stimulusName(kind) << ',' << filters << ',' << bestLag << ','
            << peak << ',' << std::sqrt(bestError / (count - bestLag)) << ','
            << std::sqrt(env / (count - bestLag)) << ',' << inOnset << ','
            << outOnset << '\n';
      }
}

void clockModulation(std::ostream &out) {
  out << "case,sample,delay_ms,input,output,compressor_level,expander_level,"
         "compressor_gain,expander_gain,output_delta,compressor_level_delta,"
         "expander_level_delta,cache_compressor_jump,cache_expander_jump,phase_"
         "before,phase_after_cache,bucket_before,bucket_after_cache\n";
  const char *names[] = {"constant", "sine", "smooth_random", "abrupt"};
  for (int kind = 0; kind < 4; ++kind) {
    auto cc = characterQualification::example();
    cc.nonlinear = {drift::BBDNonlinearityMode::EngineeringPolynomial, 1};
    Path p(2, .47e-6, 512, .01, cc);
    double previousY = 0, previousC = 1, previousE = 1;
    const double targets[] = {.0, .8, -.4, .3, -.9, .1};
    for (int n = 0; n < 48000; ++n) {
      const double t = n / rate;
      const int segment = std::min(4, int(t * 5));
      const double f = t * 5 - segment;
      const double random =
          targets[segment] + (targets[segment + 1] - targets[segment]) *
                                 (1 - std::cos(drift::pi * f)) * .5;
      const double delay = kind == 0 ? .01
                           : kind == 1
                               ? .01 + .003 * std::sin(2 * drift::pi * 2 * t)
                           : kind == 2 ? .01 + .003 * random
                           : t < .5    ? .01
                                       : .003;
      const double cBefore = p.chain.compressor.levelAverager().value(),
                   eBefore = p.chain.expander.levelAverager().value();
      const double phase = p.chain.core.telemetry().accumulatedClockPhase,
                   bucket = p.chain.core.stageValue(0);
      p.chain.setDelaySeconds(delay);
      const double cJump = p.chain.compressor.levelAverager().value() - cBefore,
                   eJump = p.chain.expander.levelAverager().value() - eBefore;
      const double phaseAfter = p.chain.core.telemetry().accumulatedClockPhase,
                   bucketAfter = p.chain.core.stageValue(0);
      require(cJump == 0 && eJump == 0 && phaseAfter == phase &&
                  bucketAfter == bucket,
              "clock cache continuity");
      const double x = stimulus(5, n), y = p.process(x),
                   c = p.chain.compressor.levelAverager().value(),
                   e = p.chain.expander.levelAverager().value();
      out << names[kind] << ',' << n << ',' << delay * 1000 << ',' << x << ','
          << y << ',' << c << ',' << e << ','
          << p.chain.compressor.currentGain() << ','
          << p.chain.expander.currentGain() << ',' << y - previousY << ','
          << c - previousC << ',' << e - previousE << ',' << cJump << ','
          << eJump << ',' << phase << ',' << phaseAfter << ',' << bucket << ','
          << bucketAfter << '\n';
      previousY = y;
      previousC = c;
      previousE = e;
    }
  }
}

void stereo(std::ostream &out) {
  out << "path,case,input_balance_db,output_balance_db,balance_change_db,input_"
         "correlation,output_correlation,correlation_change,compressor_gain_"
         "mismatch_rms,expander_gain_mismatch_rms\n";
  for (bool bbd : {false, true})
    for (int kind = 0; kind < 3; ++kind) {
      Path left(2, .47e-6, 512, .01, drift::BBDCharacterConfig{}),
          right(2, .47e-6, 512, .01, drift::BBDCharacterConfig{});
      double ipL = 0, ipR = 0, opL = 0, opR = 0, ic = 0, oc = 0, cMismatch = 0,
             eMismatch = 0;
      for (int n = 0; n < 72000; ++n) {
        const double l = .3 * (1 + .7 * std::sin(n * .0003)) *
                         std::sin(n * .06544984694978735);
        const double r = kind == 0   ? l * .1
                         : kind == 1 ? .15 * std::sin(n * .083)
                                     : l * (n % 12000 < 6000 ? .1 : 1);
        auto process = [bbd](Path &p, double x) {
          return bbd ? p.process(x)
                     : p.chain.expander.process(
                           p.chain.compressor.process(x, dt), dt);
        };
        const double a = process(left, l), b = process(right, r);
        if (n >= 24000) {
          ipL += l * l;
          ipR += r * r;
          opL += a * a;
          opR += b * b;
          ic += l * r;
          oc += a * b;
          const double cg = left.chain.compressor.currentGain() -
                            right.chain.compressor.currentGain(),
                       eg = left.chain.expander.currentGain() -
                            right.chain.expander.currentGain();
          cMismatch += cg * cg;
          eMismatch += eg * eg;
        }
      }
      const double ib = 10 * std::log10(ipL / ipR),
                   ob = 10 * std::log10(opL / opR),
                   corrI = ic / std::sqrt(ipL * ipR),
                   corrO = oc / std::sqrt(opL * opR);
      out << (bbd ? "OutsideFilters_BBD" : "ideal_channel") << ','
          << (kind == 0   ? "asymmetric_same_tone"
              : kind == 1 ? "different_tones"
                          : "alternating_balance")
          << ',' << ib << ',' << ob << ',' << ob - ib << ',' << corrI << ','
          << corrO << ',' << corrO - corrI << ','
          << std::sqrt(cMismatch / 48000) << ',' << std::sqrt(eMismatch / 48000)
          << '\n';
    }
}

struct Timing {
  double seconds = 0, detectors = 0, compressors = 0, expanders = 0;
};
Timing timing(unsigned stages, double delay, int mode, int voices) {
  std::array<drift::ClockedBBDCore, 8> cores;
  std::array<drift::BBDFeedbackCompressor, 8> c;
  std::array<drift::BBDFeedforwardExpander, 8> e;
  for (int v = 0; v < voices; ++v) {
    auto cc = drift::BBDCharacterConfig{};
    if (mode >= 4) {
      cc = characterQualification::example();
      cc.nonlinear = {drift::BBDNonlinearityMode::EngineeringPolynomial, 1};
    }
    cores[v].setCharacterConfig(cc);
    cores[v].setQualificationMode(drift::BBDMode::AsyncLinearReference,
                                  drift::BBDFilterProfile::HoltersParkerTable1);
    cores[v].prepare(rate, stages);
    cores[v].setDelaySeconds(delay);
    c[v].configure(compConfig());
    e[v].configure(compConfig());
  }
  constexpr int count = 12000;
  std::array<double, count> input{};
  for (int n = 0; n < count; ++n)
    input[n] = .1 * std::sin(n * .06544984694978735);
  volatile double sink = 0;
  auto render = [&]() {
    for (double x : input)
      for (int v = 0; v < voices; ++v) {
        double y = x;
        if (mode == 1 || mode == 3 || mode == 4)
          y = c[v].process(y, dt);
        y = cores[v].process(y);
        if (mode == 2 || mode == 3 || mode == 4)
          y = e[v].process(y, dt);
        sink = y;
      }
  };
  render(); // warm caches and detector states; excluded from timing
  auto begin = std::chrono::steady_clock::now();
  render();
  Timing result;
  result.seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - begin)
          .count();
  result.compressors =
      (mode == 1 || mode == 3 || mode == 4) ? voices * rate : 0;
  result.expanders = (mode == 2 || mode == 3 || mode == 4) ? voices * rate : 0;
  result.detectors = result.compressors + result.expanders;
  (void)sink;
  return result;
}
Timing medianTiming(unsigned stages, double delay, int mode, int voices) {
  std::array<Timing, 3> times;
  for (auto &t : times)
    t = timing(stages, delay, mode, voices);
  std::sort(times.begin(), times.end(),
            [](auto a, auto b) { return a.seconds < b.seconds; });
  return times[1];
}
void performance(std::ostream &stereoOut, std::ostream &multiOut) {
  const char *header =
      "stages,delay_ms,voices,mode,realtime_factor,detector_updates_per_second,"
      "compressor_evaluations_per_second,expander_evaluations_per_second,"
      "overhead_percent_vs_M24,cpu_factor_vs_stereo_baseline,overhead_percent_"
      "vs_matching_character\n";
  stereoOut << header;
  multiOut << header;
  const char *names[] = {"M24_baseline",
                         "compressor_only",
                         "expander_only",
                         "full_compander",
                         "full_character_nonlinear_compander",
                         "M24_full_character_nonlinear_baseline"};
  for (unsigned stages : {512u, 1024u, 2048u, 4096u})
    for (double delay : {.003, .01, .03}) {
      const auto stereoBase = medianTiming(stages, delay, 0, 2),
                 multiBase = medianTiming(stages, delay, 0, 8);
      for (int voices : {2, 8}) {
        const auto characterBase = medianTiming(stages, delay, 5, voices);
        for (int mode = 0; mode < 6; ++mode) {
          const auto baseline = voices == 2 ? stereoBase : multiBase;
          const auto m = mode == 0 ? baseline
                         : mode == 5
                             ? characterBase
                             : medianTiming(stages, delay, mode, voices);
          auto &out = voices == 2 ? stereoOut : multiOut;
          out << stages << ',' << delay * 1000 << ',' << voices << ','
              << names[mode] << ',' << m.seconds / .25 << ',' << m.detectors
              << ',' << m.compressors << ',' << m.expanders << ','
              << 100 * (m.seconds / baseline.seconds - 1) << ','
              << m.seconds / stereoBase.seconds << ','
              << 100 * (m.seconds / (mode >= 4 ? characterBase.seconds
                                               : baseline.seconds) -
                        1)
              << '\n';
        }
      }
    }
}

int main(int argc, char **argv) {
  try {
    tests();
    if (argc > 1 && std::string(argv[1]) == "--tests-only") {
      std::cout << "Compander mathematical, frozen M2.4, finite, block and "
                   "allocation tests passed\n";
      return 0;
    }
    const std::filesystem::path dir =
        argc > 1 ? argv[1] : "bbd_compander_qualification";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    require(!ec, "artifact directory");
    const char *names[] = {"bbd_compander_static_ratio.csv",
                           "bbd_compander_roundtrip.csv",
                           "bbd_compander_detector_ripple.csv",
                           "bbd_compander_transients.csv",
                           "bbd_compander_capacitor_sweep.csv",
                           "bbd_compander_noise_reduction.csv",
                           "bbd_compander_nonlinearity_interaction.csv",
                           "bbd_compander_delay_alignment.csv",
                           "bbd_compander_clock_modulation.csv",
                           "bbd_compander_stereo.csv",
                           "bbd_compander_performance.csv",
                           "bbd_compander_multivoice_performance.csv",
                           "README.txt"};
    std::array<std::ofstream, 13> out;
    for (int i = 0; i < 13; ++i) {
      out[i].open(dir / names[i]);
      out[i] << std::setprecision(17);
      require(bool(out[i]), "artifact open");
    }
    out[12] << "DriftBrigade-M2.5-BBD-Compander-Qualification\n";
    out[12].flush();
    require(bool(out[12]), "artifact initial flush");
    staticRatios(out[0]);
    roundTrips(out[1], out[4]);
    ripple(out[2]);
    transients(out[3]);
    noiseAndNonlinear(out[5], out[6]);
    delayAlignment(out[7]);
    clockModulation(out[8]);
    stereo(out[9]);
    performance(out[10], out[11]);
    out[12]
        << "PAPER-BACKED: feedback y=x/avg(abs(y)), feedforward "
           "y=avg(abs(x))*x; full-wave RC recurrence alpha=dt/(10000*C+dt), "
           "beta=10000*C/(10000*C+dt). Physical units. Typical caps .22/.47/1 "
           "uF, tau 2.2/4.7/10 ms.\n"
        << "DERIVED FROM PAPER EQUATIONS: current-sample positive root "
           "L=(beta*previous+sqrt((beta*previous)^2+4*alpha*abs(x)))/2. "
           "Separate detector states. Ideal DC analytical slopes .5 and 2. "
           "Matched initial states imply unity-channel transient identity "
           "within rounding.\n"
        << "ENGINEERING APPROXIMATION: unity reset, binary floor 2^-500, "
           "double subnormal input/intermediate/output flushing and "
           "float-range admission; no gain "
           "limiter. OutsideFilters at host rate 48kHz, Table1 async filters "
           "unless filters=0. Terminal and feedback topologies deferred. "
           "Extreme .01/10uF outside typical range. No voltage calibration, IC "
           "noise/distortion or final capacitor selection.\n"
        << "Roundtrip: 1.5s complete trajectories, no channel latency; error "
           "spectrum bins 500Hz through 2.5kHz, descriptive for nonstationary "
           "signals. Recovery=0 means error remains below 2e-12 at "
           "transitions. Static DC settles for 100*tau; 0.25Hz RMS "
           "observations do not determine slopes. Ripple: .25 sine amplitude, "
           ".5s warmup then 1s coherent measurement; reference/expander "
           "detector mean and ripple are directly comparable, compressor has "
           "output-derived detection. Modulation "
           "depth=(gain_max-gain_min)/(gain_max+gain_min). Transient CSV "
           "contains every sample for .5s per case.\n"
        << "Noise: deterministic M2.2 output noise RMS=1e-4, same seed 991, "
           "stage1024/delay10ms; no IC noise. Paired noisy minus clean "
           "trajectories isolate the incremental noise including its envelope "
           "coupling. SNR uses clean signal total RMS/noisy-clean residual "
           "RMS, not a single-bin residual. Noise off/on changes only noise "
           "sources. Pumping depth includes deterministic full-wave ripple and "
           "noise coupling; separate gain-difference RMS isolates "
           "noise-induced detector change. It is not a perceptual score.\n"
        << "Nonlinearity: A-E all use Table1, .47uF, 500Hz tone; output "
           "harmonics from CLEAN paired path, noise from difference. BBD input "
           "peak/RMS/mean magnitude and range fractions are accumulated at "
           "actual CAPTURE events after capture noise (clean path has none); "
           "nominal=abs<=1, useful=.1<=abs<=1, an engineering observation "
           "band. Input detector averages the held capture tap at host rate. "
           "Intrinsic THD is the nonlinear-only output tap held at output "
           "events and observed at host rate; includes detector-generated "
           "harmonics already in compressed input and hold response, so "
           "differences C versus D distinguish incremental nonlinear "
           "interaction. Added nonlinear harmonic ratio uses nonlinear output "
           "minus input over input H1. Full character uses the existing "
           "synthetic M2.2 "
           "fixture. Never calibrated. Harmonics H2-H5 are absolute peak; THD "
           "excludes other bins/aliases.\n"
        << "Alignment: 2s, lag search +/-16 host samples around physical "
           "delay; reference shifted only in ANALYSIS, no sidechain delay. "
           "Impulse onsets use 1e-8 threshold. DC-envelope step stimuli use "
           "carrier amplitude steps; rhythmic 8Hz envelopes. At300ms,stage1024 "
           "clock1706.7Hz can alias500Hz harmonics; this is retained and not "
           "attributed to envelope misalignment alone.\n"
        << "Clock CSV: all samples, constant/sine/raised-cosine deterministic "
           "random/abrupt delay. Cache-only detector, phase and bucket changes "
           "must be exactly zero; actual audio differences include physical "
           "transport/time warp and filter evolution. No reset or manual "
           "sidechain. Stereo detectors independent, ideal and filtered BBD "
           "comparisons, balance/correlation use raw moments over final1s, no "
           "linked production mode.\n"
        << "CPU: 48kHz,250ms measured after250ms warmup, median3, two/eight "
           "independent cores (not worker threads). Wall/audio lower better; "
           "counts analytically from host updates, per audio second; baseline "
           "M2.4 ideal async path, mode4 adds full character+nonlinearity as "
           "well as compander; mode5 supplies a matching "
           "full-character/nonlinear M2.4 baseline for separate overhead. "
           "Counter instrumentation compiled only in this "
           "tool; operating-stat collection OFF during timing. Absolute timing "
           "includes instrumented async operation counts shared across all "
           "measured live modes; production has none. Informational only, no "
           "wall-clock CI gates.\n"
        << "Tests: independent long-double RC recurrence/closed step/bisection "
           "feedback root/DC oracles, frozen M2.4 "
           "output/hold/filter/bucket/phase/noise identity fixed/modulated "
           "clocks, floors/startup/silence/subnormals/nonfinite/float "
           "extremes, maximum event rates256-4096stages, zero ordinary C++ "
           "heap allocations, block1/17/64/127/256/511/1024 exact "
           "output/detector trajectories. Linux CI supplies ASan/UBSan. "
           "Production remains DigitalFractionalDelay.\n";
    bool ok = true;
    for (auto &stream : out) {
      stream.flush();
      const bool written = bool(stream);
      stream.close();
      ok = ok && written && !stream.fail();
    }
    require(ok, "artifact finalization");
    std::cout << "Compander qualification wrote " << dir << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
