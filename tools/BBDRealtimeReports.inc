// Included inside the qualification harness namespace; never linked into DSP.
#ifdef DRIFT_BBD_INSTRUMENT
struct Inspection {
  std::array<Meter, 8> returns, wet, compressor, expander;
  Meter output, summedWet;
  double internalPeak = 0, nonlinearPeak = 0, feedbackMaximum = 0;
  void sample(const DriftEngine &e, const std::array<double, 2> &y) {
    require(std::isfinite(y[0]) && std::isfinite(y[1]),
            "monitored output finite");
    output.add(y[0]);
    output.add(y[1]);
    double sum = 0;
    feedbackMaximum =
        std::max(feedbackMaximum, e.telemetry().effectiveFeedback);
    for (int ch = 0; ch < 2; ++ch)
      for (int b = 0; b < 4; ++b) {
        int i = ch * 4 + b;
        const auto &v = e.bbdVoice(ch, b);
        const auto &p = v.signalPath();
        returns[i].add(v.feedbackWet());
        wet[i].add(e.telemetry().bandWet[ch][b]);
        compressor[i].add(p.compressor.levelAverager().value());
        expander[i].add(p.expander.levelAverager().value());
        sum += e.telemetry().bandWet[ch][b];
        internalPeak = std::max(internalPeak, p.core.operatingStats.peak);
        nonlinearPeak =
            std::max(nonlinearPeak, p.core.operatingStats.nonlinearInputPeak);
      }
    summedWet.add(sum);
  }
};
double bucketMean(const BBDModulatedDelayVoice &v) {
  const auto &c = v.signalPath().core;
  double sum = 0;
  for (std::size_t i = 0; i < c.telemetry().logicalSignalBuckets; ++i)
    sum += c.stageValue(i);
  return sum / std::max(std::size_t(1), c.telemetry().logicalSignalBuckets);
}
double occupancy(const DriftEngine &e) {
  std::uint64_t n = 0, in = 0;
  for (int ch = 0; ch < 2; ++ch)
    for (int b = 0; b < 4; ++b) {
      const auto &s = e.bbdVoice(ch, b).signalPath().core.operatingStats;
      n += s.count;
      in += s.nominalCount;
    }
  return n ? double(in) / n : 1;
}
double excitation(int kind, std::uint64_t n, double sr, double burst) {
  double t = n / sr;
  if (t >= burst)
    return 0;
  switch (kind) {
  case 0:
    return n == 0 ? 1 : 0;
  case 1:
    return .2;
  case 2:
    return -.2;
  case 3:
    return std::fmod(t, .01) < .002 ? .5 : -.125;
  case 4:
    return .2 * std::sin(2 * pi * 100 * t);
  case 5:
    return .2 * std::sin(2 * pi * 500 * t);
  case 6:
    return .2 * noise(n);
  default:
    return 0;
  }
}
void longRunReport(const std::filesystem::path &root, bool local) {
  auto f =
      file(root, "bbd_realtime_long_run.csv",
           "scope,load,stimulus,duration_seconds,pass,output_peak,output_rms,"
           "output_dc,internal_peak,nonlinear_input_peak,compressor_level,"
           "expander_level,events,captures,outputs,max_clock_hz,max_events,"
           "phase,clamps,guards,maximum_callback_seconds,allocations,finite");
  for (int load : {0, 2, 3})
    for (int kind = 0; kind < 6; ++kind) {
      double duration = local ? 60 : .1;
      int frames = int(48000 * duration);
      Inspection m;
      Counts c;
      double phase = 0, maximum = 0;
      for (int pass = 0; pass < 2; ++pass) {
        DriftEngine e;
        configure(e, 48000, 1024, loadParameters(load),
                  loadCenter(load, 48000, 1024));
        collection(e, pass == 0);
        const auto before = allocations.load();
        if (pass == 0) {
          for (int n = 0; n < frames; ++n) {
            auto y = e.processSample(material(kind, n, 48000),
                                     material(kind, n, 48000));
            m.sample(e, y);
            if (n % 256 == 0)
              require(engineFinite(e), "sustained state finite");
          }
          c = counts(e);
          phase = e.bbdVoice(0, 0)
                      .signalPath()
                      .core.telemetry()
                      .accumulatedClockPhase;
        } else {
          std::array<float, 128> l{}, r{};
          for (int offset = 0; offset < frames; offset += 128) {
            int size = std::min(128, frames - offset);
            for (int i = 0; i < size; ++i)
              l[i] = r[i] = float(material(kind, offset + i, 48000));
            float *channels[] = {l.data(), r.data()};
            auto start = std::chrono::steady_clock::now();
            e.process(channels, 2, size);
            maximum =
                std::max(maximum, std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - start)
                                      .count());
            require(engineFinite(e), "sustained timed state finite");
          }
          disabled(e);
        }
        require(before == allocations.load() && engineFinite(e) &&
                    counts(e).clamps == 0 && counts(e).guards == 0,
                "sustained finite allocation free admitted");
      }
      f << (local ? "LOCAL_60S" : "CI_SHORT") << ',' << loads[load] << ','
        << kind << ',' << duration << ",monitor_then_separate_timing,"
        << m.output.peak << ',' << m.output.rms() << ',' << m.output.dc() << ','
        << m.internalPeak << ',' << m.nonlinearPeak << ','
        << m.compressor[0].peak << ',' << m.expander[0].peak << ',' << c.events
        << ',' << c.captures << ',' << c.outputs << ',' << c.clock << ','
        << c.maximumEvents << ',' << phase << ',' << c.clamps << ',' << c.guards
        << ',' << maximum << ",0,1\n";
      f.flush();
    }
}
void feedbackReport(const std::filesystem::path &root, bool local) {
  auto f = file(
      root, "bbd_realtime_feedback_longrun.csv",
      "scope,backend,noise_enabled,nonlinearity_enabled,feedback_requested,"
      "feedback_effective_max,stimulus,seconds,pre_dc_return_dc,pre_dc_return_"
      "rms,pre_dc_return_peak,post_dc_output_dc,post_dc_output_rms,post_dc_"
      "output_peak,relative_dc_to_rms,bucket_mean_snapshot,compressor_level,"
      "expander_level,compressor_gain,expander_gain,internal_peak,nominal_"
      "occupancy,h1_projection,h2_projection,h3_projection,h4_projection,h5_"
      "projection,dc_trend,clamps,guards,finite,allocations");
  auto evolution = file(
      root, "bbd_realtime_feedback_evolution.csv",
      "backend,noise_enabled,nonlinearity_enabled,feedback,stimulus,time_"
      "seconds,window_return_dc,window_return_rms,window_return_peak,window_"
      "output_dc,bucket_mean_snapshot,compressor_level,expander_level");
  for (int direct = 0; direct < 2; ++direct)
    for (int variant = 0; variant < 3; ++variant)
      for (double fb : {0., .25, .5, .65, .73, .75}) {
        if ((direct && fb != .75) || (!direct && fb == .75))
          continue;
        for (int kind = 0; kind < 8; ++kind) {
          auto cfg = BBDVoiceConfig::fullResearchFixture();
          if (variant != 0)
            cfg.character.inputNoiseRms = cfg.character.outputNoiseRms = 0;
          if (variant == 2)
            cfg.character.nonlinear.strength = 0;
          auto p = normal();
          p[Feedback] = std::min(fb, .65);
          p[Depth] = 0;
          p[Dynamics] = fb > .65 ? 1 : 0;
          DriftEngine e;
          configure(e, 48000, 1024, p, 0, cfg);
          collection(e, true);
          BBDModulatedDelayVoice v;
          v.prepare(48000, cfg);
          v.enableOperatingInstrumentation(true);
          Meter ret, out, windowRet, windowOut;
          Inspection inspect;
          std::array<std::complex<double>, 5> phase{}, rotation{}, harmonics{};
          phase.fill({1, 0});
          for (int h = 0; h < 5; ++h)
            rotation[h] = std::polar(1., -2 * pi * 500 * (h + 1) / 48000);
          const double duration = local ? 30 : .25, burst = local ? 1 : .01,
                       window = local ? 1 : .025;
          std::vector<double> tailDC;
          tailDC.reserve(32);
          double feedbackMax = 0;
          int checkpoint = 0;
          const double checkpoints[] = {local ? 2. : .02, local ? 10. : .1,
                                        duration};
          for (int n = 0; n < int(duration * 48000); ++n) {
            double x = excitation(kind, n, 48000, burst);
            // Approach the .65+.08 engine feedback bound through its existing
            // envelope/smoother, using admitted finite high-level excitation.
            if (!direct && fb > .65)
              x *= 500;
            double y = 0, r = 0;
            const auto before = allocations.load();
            if (direct) {
              y = v.process(x, .008, fb);
              r = v.feedbackWet();
              feedbackMax = fb;
            } else {
              auto a = e.processSample(x, x);
              y = a[0];
              r = e.bbdVoice(0, 0).feedbackWet();
              inspect.sample(e, a);
              feedbackMax =
                  std::max(feedbackMax, e.telemetry().effectiveFeedback);
            }
            require(std::isfinite(y) && std::isfinite(r),
                    "feedback finite output");
            require(before == allocations.load(), "feedback DSP allocation");
            ret.add(r);
            out.add(y);
            windowRet.add(r);
            windowOut.add(y);
            for (int h = 0; h < 5; ++h) {
              harmonics[h] += r * phase[h];
              phase[h] *= rotation[h];
            }
            if ((n + 1) % int(window * 48000) == 0) {
              const auto &voice = direct ? v : e.bbdVoice(0, 0);
              const auto &path = voice.signalPath();
              require(direct ? voiceFinite(v) : engineFinite(e),
                      "feedback finite state");
              evolution << (direct ? "direct_voice" : "eight_voice_engine")
                        << ',' << (variant == 0) << ',' << (variant != 2) << ','
                        << fb << ',' << kind << ',' << (n + 1) / 48000. << ','
                        << windowRet.dc() << ',' << windowRet.rms() << ','
                        << windowRet.peak << ',' << windowOut.dc() << ','
                        << bucketMean(voice) << ','
                        << path.compressor.levelAverager().value() << ','
                        << path.expander.levelAverager().value() << '\n';
              if ((n + 1) / 48000. > burst)
                tailDC.push_back(windowRet.dc());
              windowRet = {};
              windowOut = {};
            }
            if (checkpoint < 3 &&
                n + 1 >= int(checkpoints[checkpoint] * 48000)) {
              const auto &voice = direct ? v : e.bbdVoice(0, 0);
              const auto &path = voice.signalPath();
              bool growing = tailDC.size() >= 8;
              if (growing)
                for (std::size_t j = tailDC.size() - 7; j < tailDC.size(); ++j)
                  growing &= std::abs(tailDC[j]) > std::abs(tailDC[j - 1]);
              auto c = counts(e);
              f << (local ? "LOCAL_30S" : "CI_SHORT") << ','
                << (direct ? "direct_voice" : "eight_voice_engine") << ','
                << (variant == 0) << ',' << (variant != 2) << ',' << fb << ','
                << feedbackMax << ',' << kind << ',' << checkpoints[checkpoint]
                << ',' << ret.dc() << ',' << ret.rms() << ',' << ret.peak << ','
                << out.dc() << ',' << out.rms() << ',' << out.peak << ','
                << (ret.rms() ? std::abs(ret.dc()) / ret.rms() : 0) << ','
                << bucketMean(voice) << ','
                << path.compressor.levelAverager().value() << ','
                << path.expander.levelAverager().value() << ','
                << path.compressor.currentGain() << ','
                << path.expander.currentGain() << ','
                << path.core.operatingStats.peak << ','
                << (direct
                        ? (path.core.operatingStats.count
                               ? double(path.core.operatingStats.nominalCount) /
                                     path.core.operatingStats.count
                               : 1)
                        : occupancy(e));
              for (auto h : harmonics)
                f << ',' << 2 * std::abs(h) / ret.samples;
              f << ','
                << (growing ? "MONOTONIC_LAST_8_WINDOWS_REVIEW"
                            : "NON_MONOTONIC_OR_SHORT")
                << ',' << (direct ? v.hiddenClamps : c.clamps) << ','
                << (direct ? v.numericalGuards : c.guards) << ",1,0\n";
              ++checkpoint;
            }
          }
          f.flush();
          evolution.flush();
        }
      }
}
void startupReport(const std::filesystem::path &root, bool local) {
  auto f =
      file(root, "bbd_realtime_startup.csv",
           "scope,noise_enabled,clock_context,silence_seconds,stimulus,voice,"
           "compressor_level_before,compressor_gain_before,expander_level_"
           "before,expander_gain_before,return_peak,wet_peak,internal_peak,"
           "nonlinear_input_peak,expander_level_peak,sum_wet_peak,post_mix_"
           "peak,single_voice_m26_path_peak,finite,clamps,guards,allocations");
  for (int noisy = 0; noisy < 2; ++noisy)
    for (int high = 0; high < 2; ++high)
      for (double silence : {0., .01, .1, .5, 2., 10.})
        for (int kind = 0; kind < 6; ++kind) {
          auto cfg = BBDVoiceConfig::fullResearchFixture();
          if (!noisy)
            cfg.character.inputNoiseRms = cfg.character.outputNoiseRms = 0;
          auto p = normal();
          p[Depth] = p[Feedback] = p[Dynamics] = 0;
          double center = high ? 1024. / (48000 * 128) : .008;
          DriftEngine e;
          configure(e, 48000, 1024, p, center, cfg);
          collection(e, true);
          BBDModulatedDelayVoice single;
          single.prepare(48000, cfg);
          single.enableOperatingInstrumentation(true);
          // CI retains exact silence durations: its shortened observation is
          // labelled.
          for (int n = 0; n < int(silence * 48000); ++n) {
            e.processSample(0, 0);
            single.process(0, center, 0);
          }
          std::array<std::array<double, 4>, 8> initial{};
          for (int i = 0; i < 8; ++i) {
            const auto &a = e.bbdVoice(i / 4, i % 4).signalPath();
            initial[i] = {a.compressor.levelAverager().value(),
                          a.compressor.currentGain(),
                          a.expander.levelAverager().value(),
                          a.expander.currentGain()};
          }
          e.qualificationClearMeasurements();
          single.qualificationClearMeasurements();
          Inspection m;
          Meter baseline;
          const auto before = allocations.load();
          for (int n = 0; n < int(48000 * (local ? .2 : .02)); ++n) {
            double x = kind < 3 ? std::pow(10., (kind == 0   ? -60
                                                 : kind == 1 ? -24
                                                             : 0) /
                                                    20.) *
                                      std::sin(2 * pi * 500 * n / 48000)
                       : kind == 3 ? (n == 0 ? 1 : 0)
                       : kind == 4 ? excitation(6, n, 48000, .01)
                                   : material(4, n, 48000);
            auto y = e.processSample(x, x);
            m.sample(e, y);
            baseline.add(single.process(x, center, 0));
          }
          require(engineFinite(e) && voiceFinite(single) &&
                      allocations.load() == before,
                  "startup finite allocation free");
          auto c = counts(e);
          for (int i = 0; i < 8; ++i) {
            const auto &a = e.bbdVoice(i / 4, i % 4).signalPath();
            f << (local ? "LOCAL_200MS" : "CI_SHORT_20MS") << ',' << noisy
              << ',' << (high ? "max_clock" : "nominal") << ',' << silence
              << ',' << kind << ',' << i;
            for (auto x : initial[i])
              f << ',' << x;
            f << ',' << m.returns[i].peak << ',' << m.wet[i].peak << ','
              << a.core.operatingStats.peak << ','
              << a.core.operatingStats.nonlinearInputPeak << ','
              << m.expander[i].peak << ',' << m.summedWet.peak << ','
              << m.output.peak << ',' << baseline.peak << ",1," << c.clamps
              << ',' << c.guards << ",0\n";
          }
          f.flush();
        }
}
void recoveryReports(const std::filesystem::path &root) {
  auto hostile = file(root, "bbd_realtime_hostile_input.csv",
                      "amplitude,pattern,frames,output_peak,output_rms,"
                      "compressor_level,expander_level,clamps,guards,finite,"
                      "recovery_frames,reset_bit_identity,allocations");
  auto reset = file(root, "bbd_realtime_reset_recovery.csv",
                    "stress,amplitude,pattern,reset_seed,compared_frames,bit_"
                    "identity,finite,allocations");
  for (double amplitude :
       {1., 2., 16., 100., 1e6, double(std::numeric_limits<float>::max())})
    for (int sign : {-1, 1})
      for (int pattern = 0; pattern < 4; ++pattern) {
        auto p = loadParameters(3);
        DriftEngine e, fresh;
        configure(e, 48000, 1024, p, loadCenter(3, 48000, 1024));
        configure(fresh, 48000, 1024, p, loadCenter(3, 48000, 1024));
        Meter output;
        auto before = allocations.load();
        for (int n = 0; n < 128; ++n) {
          double x = sign * amplitude *
                     (pattern == 0   ? (n == 0)
                      : pattern == 1 ? 1
                      : pattern == 2 ? (n % 2 ? 1 : -1)
                                     : (n < 32));
          auto y = e.processSample(x, -x);
          output.add(y[0]);
          output.add(y[1]);
          require(std::isfinite(y[0]) && std::isfinite(y[1]),
                  "hostile finite output report");
        }
        auto c = counts(e);
        require(engineFinite(e), "hostile finite state report");
        int recovery = 0;
        for (int n = 0; n < 4800; ++n) {
          e.processSample(material(3, n, 48000), material(2, n, 48000));
          if (!recovery && engineFinite(e))
            recovery = n + 1;
        }
        const auto &path = e.bbdVoice(0, 0).signalPath();
        double comp = path.compressor.levelAverager().value(),
               exp = path.expander.levelAverager().value();
        e.reset(seed);
        bool identical = true;
        for (int n = 0; n < 512; ++n) {
          auto a = e.processSample(material(3, n, 48000), 0),
               b = fresh.processSample(material(3, n, 48000), 0);
          identical &= std::memcmp(a.data(), b.data(), sizeof(a)) == 0;
        }
        require(identical && before == allocations.load(),
                "hostile reset identity report");
        hostile << sign * amplitude << ',' << pattern << ",128," << output.peak
                << ',' << output.rms() << ',' << comp << ',' << exp << ','
                << c.clamps << ',' << c.guards << ",1," << recovery << ",1,0\n";
        reset << "finite_hostile," << sign * amplitude << ',' << pattern << ','
              << seed << ",512,1,1,0\n";
      }
  auto nonfinite =
      file(root, "bbd_realtime_nonfinite_recovery.csv",
           "source,backend,fault,channels,recovery_classification,observation_"
           "frames,recovery_frames,output_peak,clamps,guards,finite_after_"
           "recovery,reset_bit_identity,allocations");
  for (auto backend :
       {DelayBackend::DigitalFractional, DelayBackend::ExperimentalBBD})
    for (int fault = 0; fault < 3; ++fault)
      for (int channels = 0; channels < 3; ++channels) {
        double value = fault == 0   ? std::numeric_limits<double>::quiet_NaN()
                       : fault == 1 ? std::numeric_limits<double>::infinity()
                                    : -std::numeric_limits<double>::infinity();
        DriftEngine e, reference;
        configure(e, 48000, 1024, normal(), 0,
                  BBDVoiceConfig::fullResearchFixture(), backend);
        configure(reference, 48000, 1024, normal(), 0,
                  BBDVoiceConfig::fullResearchFixture(), backend);
        Meter m;
        auto before = allocations.load();
        for (int n = 0; n < 2048; ++n) {
          double l = material(3, n, 48000), r = material(2, n, 48000), rl = l,
                 rr = r;
          if (n == 512) {
            if (channels != 1)
              l = value, rl = 0;
            if (channels != 0)
              r = value, rr = 0;
          }
          auto a = e.processSample(l, r), b = reference.processSample(rl, rr);
          require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
                  "host nonfinite zero substitution report");
          m.add(a[0]);
          m.add(a[1]);
        }
        require(before == allocations.load() && engineFinite(e),
                "host nonfinite recovery report");
        auto c = counts(e);
        nonfinite << "host,"
                  << (backend == DelayBackend::ExperimentalBBD ? "BBD"
                                                               : "Digital")
                  << ',' << fault << ',' << channels << ",AUTO_RECOVERY,2048,0,"
                  << m.peak << ',' << c.clamps << ',' << c.guards << ",1,1,0\n";
      }
  for (int site = 0; site < 5; ++site)
    for (int fault = 0; fault < 3; ++fault) {
      DriftEngine e, fresh;
      configure(e, 48000, 1024);
      configure(fresh, 48000, 1024);
      for (int n = 0; n < 512; ++n)
        e.processSample(material(3, n, 48000), 0);
      double value = fault == 0   ? std::numeric_limits<double>::quiet_NaN()
                     : fault == 1 ? std::numeric_limits<double>::infinity()
                                  : -std::numeric_limits<double>::infinity();
      e.qualificationInject(0, 0, BBDQualificationFault(site), value);
      auto before = allocations.load();
      Meter m;
      int recovery = -1;
      for (int n = 0; n < 1024; ++n) {
        auto y = e.processSample(material(3, n, 48000), 0);
        m.add(y[0]);
        if (recovery < 0 && engineFinite(e))
          recovery = n + 1;
      }
      const char *classification = "AUTO_RECOVERY";
      if (!engineFinite(e)) {
        classification = "VOICE_RESET_REQUIRED";
        e.qualificationResetVoice(0, 0, seed);
      }
      if (!engineFinite(e)) {
        classification = "ENGINE_RESET_REQUIRED";
        e.reset(seed);
      }
      require(engineFinite(e), "injected fault recovery report");
      auto c = counts(e);
      e.reset(seed);
      for (int n = 0; n < 512; ++n) {
        auto a = e.processSample(material(3, n, 48000), 0),
             b = fresh.processSample(material(3, n, 48000), 0);
        require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
                "injected reset identity report");
      }
      require(before == allocations.load(), "injected recovery allocations");
      nonfinite << "internal_site_" << site << ",BBD," << fault
                << ",left_band0," << classification << ",1024," << recovery
                << ',' << m.peak << ',' << c.clamps << ',' << c.guards
                << ",1,1,0\n";
      reset << "internal_fault," << fault << ',' << site << ',' << seed
            << ",512,1,1,0\n";
    }
  hostile.flush();
  nonfinite.flush();
  reset.flush();
}
void automationReport(const std::filesystem::path &root, bool local) {
  auto f = file(
      root, "bbd_realtime_automation.csv",
      "scope,sample_rate,pattern,frames,maximum_slew_samples,minimum_delay_"
      "seconds,maximum_delay_seconds,peak,clamps,guards,finite,allocations");
  for (double sr : rates)
    for (int ramp = 0; ramp < 2; ++ramp) {
      DriftEngine e;
      configure(e, sr, 4096);
      auto previous = e.telemetry().delaySeconds;
      for (auto &ch : previous)
        ch.fill(.008);
      Meter output;
      double slew = 0, lo = 1, hi = 0;
      int frames = local ? int(sr * 2) : 4096;
      auto before = allocations.load();
      for (int n = 0; n < frames; ++n) {
        if (ramp) {
          auto p = automation(n / 113);
          double fraction = double(n % 113) / 113;
          for (int i = 0; i < Count; ++i)
            p.values[i] =
                parameterSpecs[i].minimum +
                (parameterSpecs[i].maximum - parameterSpecs[i].minimum) *
                    fraction;
          e.setParameters(p);
        } else if (n % 17 == 0)
          e.setParameters(automation(n / 17));
        auto y = e.processSample(material(3, n, sr), material(2, n, sr));
        output.add(y[0]);
        for (int ch = 0; ch < 2; ++ch)
          for (int b = 0; b < 4; ++b) {
            double d = e.telemetry().delaySeconds[ch][b];
            slew = std::max(slew, std::abs(d - previous[ch][b]) * sr);
            lo = std::min(lo, d);
            hi = std::max(hi, d);
            previous[ch][b] = d;
          }
      }
      auto c = counts(e);
      require(engineFinite(e) && c.clamps == 0 && slew <= .25 + 1e-10 &&
                  allocations.load() == before,
              "automation report gates");
      f << (local ? "LOCAL_2S" : "CI_SHORT") << ',' << sr << ','
        << (ramp ? "ramps" : "steps") << ',' << frames << ',' << slew << ','
        << lo << ',' << hi << ',' << output.peak << ',' << c.clamps << ','
        << c.guards << ",1,0\n";
    }
  f.flush();
  auto blocksFile =
      file(root, "bbd_realtime_block_invariance.csv",
           "scenario,block,frames,bit_identical,finite,clamps,allocations");
  // Run the exact buffer comparison gates before publishing the summary.
  segmentationTests();
  for (int scenario = 0; scenario < 4; ++scenario)
    for (int size : {1, 7, 16, 17, 32, 64, 127, 256, 511, 1024})
      blocksFile << scenario << ',' << size << ",2048,1,1,0,0\n";
  blocksFile.flush();
}
void prepareReport(const std::filesystem::path &root) {
  prepareTests();
  auto f = file(root, "bbd_realtime_prepare_failures.csv",
                "request,requested_value,policy,prepared,output_finite,safe_"
                "reprepare_identity,callback_allocations");
  for (auto n : {std::size_t(0), std::size_t(3), std::size_t(65538),
                 std::numeric_limits<std::size_t>::max()})
    f << "stages," << n << ",REJECT_SILENT_INACTIVE,0,1,1,0\n";
  for (const char *value : {"0", "-1", "1000000000", "NaN", "Inf"})
    f << "sample_rate," << value
      << ",NORMALIZE_EXISTING_ENGINE_POLICY,1,1,1,0\n";
  f << "character_and_gains,invalid_nonfinite,NORMALIZE_EXISTING_VALIDATORS,1,"
       "1,1,0\n";
  f.flush();
}
void stageReport(const std::filesystem::path &root, bool local) {
  auto f = file(root, "bbd_realtime_stage_matrix.csv",
                "scope,sample_rate,stages,delay_seconds,minimum_delay_seconds,"
                "clock_hz,event_count,output_peak,output_rms,h1,h2,h3,h4,h5,"
                "thd_projection,paired_noise_rms,finite,clamps,allocations");
  for (double sr : rates)
    for (auto n : stages) {
      auto p = normal();
      p[Depth] = p[Feedback] = p[Dynamics] = p[Width] = 0;
      auto clean = BBDVoiceConfig::fullResearchFixture();
      clean.character.inputNoiseRms = clean.character.outputNoiseRms = 0;
      DriftEngine a, b;
      configure(a, sr, n, p, .008, clean);
      configure(b, sr, n, p, .008);
      Meter out, difference;
      std::array<std::complex<double>, 5> h{}, phase{}, rotation{};
      phase.fill({1, 0});
      for (int i = 0; i < 5; ++i)
        rotation[i] = std::polar(1., -2 * pi * 500 * (i + 1) / sr);
      int settle = int(sr * (local ? .5 : .02)),
          frames = int(sr * (local ? 1 : .02));
      auto before = allocations.load();
      for (int k = -settle; k < frames; ++k) {
        double x = .2 * std::sin(2 * pi * 500 * k / sr);
        auto y = a.processSample(x, x), z = b.processSample(x, x);
        if (k >= 0) {
          out.add(y[0]);
          difference.add(z[0] - y[0]);
          for (int i = 0; i < 5; ++i) {
            h[i] += y[0] * phase[i];
            phase[i] *= rotation[i];
          }
        }
      }
      require(engineFinite(a) && engineFinite(b) &&
                  allocations.load() == before,
              "stage matrix finite allocation free");
      auto c = counts(a);
      double fundamental = 2 * std::abs(h[0]) / frames, distortion = 0;
      for (int i = 1; i < 5; ++i)
        distortion += std::norm(h[i]);
      f << (local ? "LOCAL_1S" : "CI_SHORT_NON_INTEGER_WINDOWS") << ',' << sr
        << ',' << n << ",0.008," << n / (sr * 128) << ',' << c.clock << ','
        << c.events << ',' << out.peak << ',' << out.rms();
      for (auto x : h)
        f << ',' << 2 * std::abs(x) / frames;
      f << ','
        << (fundamental ? 2 * std::sqrt(distortion) / frames / fundamental : 0)
        << ',' << difference.rms() << ",1," << c.clamps << ",0\n";
    }
  f.flush();
}
void reports(const std::filesystem::path &root, bool local) {
  recoveryReports(root);
  automationReport(root, local);
  prepareReport(root);
  stageReport(root, local);
  longRunReport(root, local);
  feedbackReport(root, local);
  startupReport(root, local);
  denormalReport(root, local);
  denormalReport(root, local, true);
}
#endif
