#include "BBDCharacterMeasurements.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace characterQualification;
using Clock = std::chrono::steady_clock;
// Offline radix-2 FFT: no audio-path dependency.
void fft(std::vector<std::complex<double>> &x) {
  const auto n = x.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(x[i], x[j]);
  }
  for (std::size_t len = 2; len <= n; len *= 2) {
    auto wlen = std::exp(std::complex<double>(0, -2 * drift::pi / len));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w = 1;
      for (std::size_t j = 0; j < len / 2; ++j) {
        auto u = x[i + j], v = x[i + j + len / 2] * w;
        x[i + j] = u + v;
        x[i + j + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}
int main(int argc, char **argv) {
  std::filesystem::path dir =
      argc > 1 ? argv[1] : "bbd_character_qualification";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec || !std::filesystem::is_directory(dir, ec) || ec)
    return 1;
  const std::array<const char *, 9> names{
      {"bbd_device_gain.csv", "bbd_device_frequency_response.csv",
       "bbd_noise_floor.csv", "bbd_noise_spectrum.csv",
       "bbd_stage_accumulation.csv", "bbd_clock_sweep.csv",
       "bbd_feedback_probe.csv", "README.txt", "bbd_cpu.csv"}};
  std::array<std::ofstream, 9> s;
  for (std::size_t i = 0; i < s.size(); ++i) {
    s[i].open(dir / names[i]);
    s[i] << std::setprecision(17);
    if (!s[i])
      return 1;
  }
  s[0] << "stages,clock_hz,ratio,broadband_gain,device_measured,device_"
          "predicted\n";
  s[1] << "stages,clock_hz,hz,input_filter,hold_sinc,device,output_filter,"
          "combined_predicted,combined_measured\n";
  s[2] << "stages,clock_hz,mode,seed,rms,peak,dc,crest_factor\n";
  s[3] << "stages,clock_hz,mode,seed,low_hz,high_hz,band_rms,expected_band_"
          "rms\n";
  s[4] << "stages,clock_hz,dc_gain,source_rms,filtered_noise_rms\n";
  for (std::size_t stages : {256u, 512u, 1024u, 2048u, 4096u})
    for (double clock : {8000., 16000., 32000.}) {
      auto cfg = example(drift::BBDCharacterMode::LossOnly);
      drift::BBDDeviceLoss device;
      device.update(cfg, stages, clock);
      drift::AsyncAnalogFilter in, out;
      in.paperReference(false);
      out.paperReference(true);
      for (double ratio : {0., .1, .25, .4}) {
        const double hz = clock * ratio;
        double re = 0, im = 0;
        device.reset();
        for (int i = 0; i < 24000; ++i) {
          double y =
              device.process(ratio ? std::sin(2 * drift::pi * ratio * i) : 1);
          if (i >= 4000) {
            re += y * (ratio ? std::cos(2 * drift::pi * ratio * i) : 1);
            im += ratio ? y * std::sin(2 * drift::pi * ratio * i) : 0;
          }
        }
        double measured = (ratio ? 2 : 1) * std::hypot(re, im) / 20000;
        double dev = std::abs(device.response(ratio));
        s[0] << stages << ',' << clock << ',' << ratio << ',' << device.gain
             << ',' << measured << ',' << dev << '\n';
        double sinc =
            ratio ? std::sin(drift::pi * ratio) / (drift::pi * ratio) : 1;
        double predicted =
            std::abs(in.response(hz)) * sinc * dev * std::abs(out.response(hz));
        drift::ClockedBBDCore core;
        setup(core, stages, clock, cfg, true, 384000);
        re = im = 0;
        const int warm =
            static_cast<int>(384000 * (stages / (2 * clock) + .05));
        for (int i = 0; i < warm + 38400; ++i) {
          double y = core.process(
              ratio ? std::sin(2 * drift::pi * hz * i / 384000) : 1);
          if (i >= warm) {
            re += y * (ratio ? std::cos(2 * drift::pi * hz * i / 384000) : 1);
            im += ratio ? y * std::sin(2 * drift::pi * hz * i / 384000) : 0;
          }
        }
        double combined = (ratio ? 2 : 1) * std::hypot(re, im) / 38400;
        if (!std::isfinite(combined) || std::abs(combined - predicted) > .002 ||
            std::abs(measured - dev) > 1e-9) {
          std::cerr << "Frequency qualification failed\n";
          return 1;
        }
        s[1] << stages << ',' << clock << ',' << hz << ','
             << std::abs(in.response(hz)) << ',' << sinc << ',' << dev << ','
             << std::abs(out.response(hz)) << ',' << predicted << ','
             << combined << '\n';
      }
      for (auto mode :
           {drift::BBDCharacterMode::Ideal, drift::BBDCharacterMode::NoiseOnly,
            drift::BBDCharacterMode::FullLinearCharacter})
        for (unsigned seed : {1u, 42u}) {
          auto noise = example(mode);
          noise.seed = seed;
          drift::ClockedBBDCore core;
          setup(core, stages, clock, noise);
          for (int i = 0; i < 48000; ++i)
            core.process(0);
          std::vector<std::complex<double>> samples(65536);
          double sum = 0, square = 0, peak = 0;
          for (auto &v : samples) {
            double y = core.process(0);
            v = y;
            sum += y;
            square += y * y;
            peak = std::max(peak, std::abs(y));
          }
          double rms = std::sqrt(square / samples.size());
          s[2] << stages << ',' << clock << ',' << static_cast<int>(mode) << ','
               << seed << ',' << rms << ',' << peak << ','
               << sum / samples.size() << ',' << (rms ? peak / rms : 0) << '\n';
          if (mode == drift::BBDCharacterMode::NoiseOnly && seed == 1)
            s[4] << stages << ',' << clock << ',' << device.gain << ','
                 << noise.outputNoiseRms * std::sqrt(stages / 1024.) << ','
                 << rms << '\n';
          fft(samples);
          for (auto band :
               std::array<std::array<double, 2>, 4>{{{{20, 200}},
                                                     {{200, 2000}},
                                                     {{2000, 8000}},
                                                     {{8000, 24000}}}}) {
            double energy = 0;
            for (std::size_t k = 1; k < samples.size() / 2; ++k) {
              double hz = k * 48000. / samples.size();
              if (hz >= band[0] && hz < band[1])
                energy += 2 * std::norm(samples[k]);
            }
            // Sampled analogue output PSD includes host aliases. Uniform PRNG
            // source is white at BBD updates; hold and existing output filter
            // supply its shaping, without a second device noise-colour filter.
            double expectedEnergy = 0;
            const double sourceRms =
                mode == drift::BBDCharacterMode::Ideal
                    ? 0
                    : noise.outputNoiseRms * std::sqrt(stages / 1024.);
            for (double f = band[0] + .5; f < band[1]; f += 1) {
              for (int image = -4; image <= 4; ++image) {
                double analogHz = f + 48000 * image;
                double ratio = analogHz / clock;
                double hold = ratio == 0 ? 1
                                         : std::sin(drift::pi * ratio) /
                                               (drift::pi * ratio);
                expectedEnergy += 2 * sourceRms * sourceRms / clock * hold *
                                  hold * std::norm(out.response(analogHz));
              }
            }
            double bandRms = std::sqrt(energy) / samples.size();
            double expectedRms = std::sqrt(expectedEnergy);
            if (!std::isfinite(bandRms) ||
                (expectedRms && std::abs(bandRms / expectedRms - 1) > .25)) {
              std::cerr << "Noise spectral qualification failed\n";
              return 1;
            }
            s[3] << stages << ',' << clock << ',' << static_cast<int>(mode)
                 << ',' << seed << ',' << band[0] << ',' << band[1] << ','
                 << bandRms << ',' << expectedRms << '\n';
          }
        }
    }
  s[5] << "sample,clock_hz,gain,pole,total_edges,held,output\n";
  drift::ClockedBBDCore core;
  setup(core, 1024, 8000, example());
  for (int i = 0; i < 48000; ++i) {
    double clock = 8000 + 24000. * i / 47999;
    core.setDelaySeconds(512 / clock);
    double y = core.process(.1);
    s[5] << i << ',' << clock << ',' << core.deviceCharacter().loss.gain << ','
         << core.deviceCharacter().loss.pole << ','
         << core.telemetry().totalEventCount << ',' << core.heldOutput() << ','
         << y << '\n';
  }
  s[6] << "feedback,noise,second,rms,peak,dc,block_decay_db_per_s,peak_growth_"
          "ratio\n";
  for (double feedback : {0., .5, .9})
    for (bool noise : {false, true}) {
      setup(core, 1024, 16000,
            example(noise ? drift::BBDCharacterMode::FullLinearCharacter
                          : drift::BBDCharacterMode::LossOnly));
      double y = 0, previousRms = 0, previousPeak = 0;
      for (int sec = 0; sec < 5; ++sec) {
        double sum = 0, square = 0, peak = 0;
        for (int i = 0; i < 48000; ++i) {
          y = core.process((sec == 0 && i < 480 ? .1 : 0) + feedback * y);
          sum += y;
          square += y * y;
          peak = std::max(peak, std::abs(y));
          if (!std::isfinite(y) || peak >= 1) {
            std::cerr << "Feedback qualification failed\n";
            return 1;
          }
        }
        const double rms = std::sqrt(square / 48000);
        const double decay =
            sec ? 20 * std::log10(std::max(rms, 1e-150) /
                                  std::max(previousRms, 1e-150))
                : 0;
        s[6] << feedback << ',' << noise << ',' << sec << ',' << rms << ','
             << peak << ',' << sum / 48000 << ',' << decay << ','
             << (previousPeak ? peak / previousPeak : 0) << '\n';
        previousRms = rms;
        previousPeak = peak;
      }
    }
  s[8] << "stages,profile,edge_rate_per_channel,seconds,relative_cpu,stereo_"
          "events_per_wall_second,complex_exp_calls_estimated,analog_advance_"
          "microbenchmark_seconds,complex_exp_microbenchmark_seconds\n";
  for (std::size_t stages : {1024u, 4096u}) {
    double baseline = 0;
    for (int profile = 0; profile < 4; ++profile) {
      drift::ClockedBBDCore l, r;
      auto cfg =
          example(profile == 2   ? drift::BBDCharacterMode::LossOnly
                  : profile == 3 ? drift::BBDCharacterMode::FullLinearCharacter
                                 : drift::BBDCharacterMode::Ideal);
      setup(l, stages, stages / .02, cfg, profile != 0);
      setup(r, stages, stages / .02, cfg, profile != 0);
      std::array<double, 5> timings{};
      for (auto &timing : timings) {
        l.reset();
        r.reset();
        auto trialStart = Clock::now();
        for (int i = 0; i < 48000; ++i) {
          l.process(.1);
          r.process(.1);
        }
        timing =
            std::chrono::duration<double>(Clock::now() - trialStart).count();
      }
      std::sort(timings.begin(), timings.end());
      double elapsed = timings[2];
      auto start = Clock::now();
      if (profile == 0)
        baseline = elapsed;
      auto events =
          l.telemetry().totalEventCount + r.telemetry().totalEventCount;
      auto calls = profile ? 10 * (events + 96000) : 0;
      drift::AsyncAnalogFilter filter;
      filter.paperReference(false);
      start = Clock::now();
      double checksum = 0;
      for (std::uint64_t i = 0; i < (profile ? calls / 5 : 0); ++i)
        checksum += filter.advance((1 + i % 97) * 1e-7, .1);
      double micro =
          std::chrono::duration<double>(Clock::now() - start).count();
      // Vary arguments to prevent constant folding/hoisting. This isolates
      // exponentials but remains a proxy, not an instrumented hot-path profile.
      const std::array<std::complex<double>, 10> poles{{-46580.,
                                                        {-55482., 25082.},
                                                        {-55482., -25082.},
                                                        {-26292., -59437.},
                                                        {-26292., 59437.},
                                                        -176261.,
                                                        {-51468., 21437.},
                                                        {-51468., -21437.},
                                                        {-26276., -59699.},
                                                        {-26276., 59699.}}};
      start = Clock::now();
      for (std::uint64_t i = 0; i < calls; ++i)
        checksum += std::exp(poles[i % 10] * ((1 + i % 97) * 1e-7)).real();
      double exponentialSeconds =
          std::chrono::duration<double>(Clock::now() - start).count();
      if (!std::isfinite(checksum))
        return 1;
      s[8] << stages << ',' << profile << ','
           << 2 * l.telemetry().effectiveClockHz << ',' << elapsed << ','
           << elapsed / baseline << ',' << events / elapsed << ',' << calls
           << ',' << micro << ',' << exponentialSeconds << '\n';
    }
  }
  s[7]
      << "DriftBrigade-M2.2-BBD-Character-Qualification\nSynthetic sensitivity "
         "fixture; NOT calibrated device specifications. Neutral/default is "
         "exact bypass.\nFixture: lossPerStage=1e-5, leakagePerStageSecond=.1, "
         "residualPolePer1024=.25, outputNoiseRms=1e-4, "
         "mismatchFraction=.001.\nNoise mode "
         "0=Ideal,2=NoiseOnly,3=FullLinearCharacter; seeds 1 and 42. Uniform "
         "source at output updates before hold/output filter; sqrt(N/1024) "
         "source scaling is an engineering assumption. One second warmup, "
         "65536 observations at 48kHz; FFT rectangular-window broad-band RMS "
         "excludes DC and Nyquist; expected bands integrate white-source PSD "
         "through one hold and output filter with host aliases; 25% "
         "statistical tolerance.\nGain "
         "component measurement runs at BBD rate. Combined coherent "
         "measurement at 384kHz, 100ms after delay+50ms settling. Prediction "
         "excludes alias images. Only one hold sinc.\nClock sweep coefficients "
         "updated without state resets. Feedback block statistics expose "
         "decay/noise/DC over five seconds, gains 0,.5,.9.\nCPU profiles "
         "0=TransportOnly,1=M2.1 Table1,2=LossOnly,3=FullLinearCharacter; "
         "48kHz stereo,10ms nominal delay; median of five one-second trials. "
         "Estimated exact complex exponential "
         "count is 10 per stereo-core interval/event. Analog advance "
         "microbenchmark is an indicative cost proxy including state "
         "arithmetic; exp-only microbenchmark varies arguments to avoid "
         "hoisting. Both are cost proxies, not exclusive profiler attribution. "
         "No exponential "
         "approximation.\nProduction remains DigitalFractionalDelay. See "
         "docs/m2_2_bbd_device_character.md.\n";
  bool ok = true;
  for (auto &stream : s) {
    stream.flush();
    bool written = bool(stream);
    stream.close();
    ok = ok && written && !stream.fail();
  }
  if (!ok)
    return 1;
  std::cout << "Wrote " << dir << '\n';
}
