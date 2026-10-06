// Qualification-only reports. No production topology, coefficient or state
// changes.
double productDb(double x) { return 20 * std::log10(std::max(x, 1e-300)); }
struct ProductQuality {
  double gain = 0, thd = 0, snr = 0, peak = 0, dc = 0, internalPeak = 0,
         nominalFraction = 1;
};
ProductQuality productQuality(double sr, const BBDProductEnvelope &c,
                              double frequency, BBDHeadroomProfile gain,
                              bool local) {
  auto p = normal();
  p[Depth] = p[Feedback] = p[Dynamics] = p[Width] = 0;
  auto cfg = BBDVoiceConfig::fullResearchFixture();
  cfg.gainStaging = BBDGainStagingConfig::profile(gain);
  auto clean = cfg;
  clean.character.inputNoiseRms = clean.character.outputNoiseRms = 0;
  DriftEngine a, b;
  const double delay = std::max(.008, c.floor(sr));
  configure(a, sr, c.stages, p, delay, clean);
  configure(b, sr, c.stages, p, delay, cfg);
  collection(a, true);
  collection(b, true);
  // Integer cycles at each rate avoid leakage into the harmonic projections.
  int frames = int(std::round(sr / frequency)) *
               int(std::ceil(frequency * (local ? .5 : .04)));
  const double exactFrequency = sr / std::round(sr / frequency);
  int settle = int(sr * (local ? .5 : .05));
  std::array<std::complex<double>, 5> h{};
  Meter out, difference;
  auto before = allocations.load();
  for (int n = -settle; n < frames; ++n) {
    const double x = .2 * std::sin(2 * pi * exactFrequency * n / sr);
    auto y = a.processSample(x, x), z = b.processSample(x, x);
    if (n >= 0) {
      out.add(y[0]);
      difference.add(z[0] - y[0]);
      for (int k = 0; k < 5; ++k)
        if (exactFrequency * (k + 1) < sr / 2)
          h[k] += y[0] *
                  std::polar(1., -2 * pi * exactFrequency * (k + 1) * n / sr);
    }
  }
  require(engineFinite(a) && engineFinite(b) && counts(a).clamps == 0 &&
              counts(b).clamps == 0 && before == allocations.load(),
          "quality finite allocation/clamp gate");
  double harmonics = 0;
  for (int k = 1; k < 5; ++k)
    harmonics += std::norm(h[k]);
  ProductQuality q;
  const double fundamental = 2 * std::abs(h[0]) / frames;
  q.gain = productDb(fundamental / .2);
  q.thd = std::sqrt(harmonics) / std::max(std::abs(h[0]), 1e-300);
  q.snr = productDb(out.rms() / std::max(difference.rms(), 1e-300));
  q.peak = out.peak;
  q.dc = out.dc();
  q.nominalFraction = occupancy(a);
  for (int ch = 0; ch < 2; ++ch)
    for (int band = 0; band < 4; ++band)
      q.internalPeak =
          std::max(q.internalPeak,
                   a.bbdVoice(ch, band).signalPath().core.operatingStats.peak);
  return q;
}
void productQualityReports(const std::filesystem::path &root, bool local) {
  auto trade = productFile(
      root, "bbd_product_stage_tradeoff.csv",
      "candidate,stages,sample_rate,delay_seconds,frequency_hz,gain_db,thd_h2_"
      "h5,paired_noise_snr_db,internal_peak,nominal_fraction,scope");
  auto startup =
      productFile(root, "bbd_product_startup_headroom.csv",
                  "candidate,sample_rate,gain_profile,silence_seconds,"
                  "stimulus,delay_seconds,summed_wet_peak_per_channel,post_"
                  "mix_peak,per_voice_internal_peak,nonlinear_input_peak,"
                  "nominal_fraction,headroom_db,finite,clamps,allocations");
  auto gains = productFile(
      root, "bbd_product_gain_profile.csv",
      "candidate,sample_rate,gain_profile,frequency_hz,gain_db,thd_h2_h5,"
      "paired_noise_snr_db,internal_peak,nominal_fraction");
  auto feedback = productFile(
      root, "bbd_product_feedback_dc.csv",
      "candidate,sample_rate,feedback,duration_seconds,voice,loop_return_dc,"
      "loop_return_rms,bucket_mean,compressor_level,expander_level,output_dc,"
      "output_peak,nominal_fraction,internal_peak,headroom_db,early_decay_rms,"
      "late_decay_rms,thd_h2_h5,finite,clamps,allocations");
  auto frontier =
      productFile(root, "bbd_product_quality_cpu_frontier.csv",
                  "candidate,stages,sample_rate,min_delay_seconds,max_"
                  "clock_hz,p99_utilization_128,gain500_db,snr_db,thd_h2_"
                  "h5,startup_peak,feedback_return_dc,scope");
  for (auto c : candidates)
    for (double sr : rates) {
      if (!productCandidateFilter.empty() && productCandidateFilter != c.name)
        continue;
      ProductQuality nominal;
      for (int g = 0; g < 3; ++g) {
        auto profile = g == 0   ? BBDHeadroomProfile::Conservative
                       : g == 1 ? BBDHeadroomProfile::Nominal
                                : BBDHeadroomProfile::HighDrive;
        const char *name = g == 0   ? "Conservative"
                           : g == 1 ? "Nominal"
                                    : "HighDrive";
        for (double frequency : {100., 500., 5000., 10000.}) {
          auto q = productQuality(sr, c, frequency, profile, local);
          gains << c.name << ',' << sr << ',' << name << ',' << frequency << ','
                << q.gain << ',' << q.thd << ',' << q.snr << ','
                << q.internalPeak << ',' << q.nominalFraction << '\n';
          if (g == 1) {
            trade << c.name << ',' << c.stages << ',' << sr << ','
                  << std::max(.008, c.floor(sr)) << ',' << frequency << ','
                  << q.gain << ',' << q.thd << ',' << q.snr << ','
                  << q.internalPeak << ',' << q.nominalFraction << ','
                  << (local ? "LOCAL_INTEGER_CYCLES" : "CI_SHORT") << '\n';
            if (frequency == 500)
              nominal = q;
          }
        }
      }
      double nominalStartup = 0;
      for (int g = 0; g < 3; ++g)
        for (double silence : {0., .1, 2.})
          for (int kind = 0; kind < 3; ++kind) {
            auto cfg = BBDVoiceConfig::fullResearchFixture();
            cfg.gainStaging = BBDGainStagingConfig::profile(
                g == 0   ? BBDHeadroomProfile::Conservative
                : g == 1 ? BBDHeadroomProfile::Nominal
                         : BBDHeadroomProfile::HighDrive);
            auto p = normal();
            p[Depth] = p[Feedback] = p[Dynamics] = 0;
            DriftEngine e;
            configure(e, sr, c.stages, p, c.floor(sr), cfg);
            collection(e, true);
            for (int n = 0; n < int(silence * sr); ++n)
              e.processSample(0, 0);
            e.qualificationClearMeasurements();
            Inspection m;
            std::array<Meter, 2> summed;
            auto before = allocations.load();
            for (int n = 0; n < int(sr * (local ? .2 : .02)); ++n) {
              double x = kind == 0   ? (n == 0 ? 1. : 0.)
                         : kind == 1 ? std::sin(2 * pi * 500 * n / sr)
                                     : material(4, n, sr);
              auto y = e.processSample(x, x);
              m.sample(e, y);
              for (int ch = 0; ch < 2; ++ch) {
                double sum = 0;
                for (int b = 0; b < 4; ++b)
                  sum += e.telemetry().bandWet[ch][b];
                summed[ch].add(sum);
              }
            }
            auto t = counts(e);
            require(engineFinite(e) && before == allocations.load() &&
                        t.clamps == 0,
                    "startup product finite allocation/clamp gate");
            if (g == 1)
              nominalStartup = std::max(nominalStartup, m.output.peak);
            startup << c.name << ',' << sr << ','
                    << (g == 0   ? "Conservative"
                        : g == 1 ? "Nominal"
                                 : "HighDrive")
                    << ',' << silence << ',' << kind << ',' << c.floor(sr)
                    << ',' << std::max(summed[0].peak, summed[1].peak) << ','
                    << m.output.peak << ',' << m.internalPeak << ','
                    << m.nonlinearPeak << ',' << occupancy(e) << ','
                    << -productDb(m.internalPeak / 2) << ",1," << t.clamps
                    << ",0\n";
          }
      double maximumDC = 0;
      for (double fb : {0., .25, .5, .65, .75}) {
        auto p = normal();
        p[Depth] = p[Dynamics] = 0;
        p[Feedback] = fb;
        // .75 is reached by the engine macro only if effectiveFeedback permits
        // it; actual effective feedback is captured below rather than assumed.
        if (fb == .75)
          p[Dynamics] = 1;
        DriftEngine e;
        configure(e, sr, c.stages, p, c.floor(sr));
        collection(e, true);
        const double duration = local ? 30. : .25;
        const int frames = int(sr * duration),
                  window = int(sr * (local ? 5. : .05));
        Inspection tail;
        Meter early, late;
        std::array<std::complex<double>, 5> harmonics{};
        const int onsetEnd = frames * 2 / 3;
        std::array<double, 8> steadyBucket{};
        double effective = 0;
        auto before = allocations.load();
        for (int n = 0; n < frames; ++n) {
          const double x =
              n < onsetEnd ? .2 * std::sin(2 * pi * 500 * n / sr) : 0;
          auto y = e.processSample(x, x);
          effective = std::max(effective, e.telemetry().effectiveFeedback);
          if (n >= onsetEnd - window && n < onsetEnd) {
            tail.sample(e, y);
            for (int k = 0; k < 5; ++k)
              harmonics[k] +=
                  y[0] * std::polar(1., -2 * pi * 500 * (k + 1) * n / sr);
          }
          if (n >= onsetEnd && n < onsetEnd + window)
            early.add(y[0]);
          if (n >= frames - window)
            late.add(y[0]);
          if (n == onsetEnd - 1)
            for (int i = 0; i < 8; ++i)
              steadyBucket[i] = bucketMean(e.bbdVoice(i / 4, i % 4));
        }
        auto t = counts(e);
        require(engineFinite(e) && before == allocations.load() &&
                    t.clamps == 0,
                "feedback product gate");
        double distortion = 0;
        for (int k = 1; k < 5; ++k)
          distortion += std::norm(harmonics[k]);
        double thd =
            std::sqrt(distortion) / std::max(std::abs(harmonics[0]), 1e-300);
        for (int i = 0; i < 8; ++i) {
          const auto &v = e.bbdVoice(i / 4, i % 4);
          const auto &path = v.signalPath();
          maximumDC = std::max(maximumDC, std::abs(tail.returns[i].dc()));
          feedback << c.name << ',' << sr << ',' << effective << ',' << duration
                   << ',' << i << ',' << tail.returns[i].dc() << ','
                   << tail.returns[i].rms() << ',' << steadyBucket[i] << ','
                   << tail.compressor[i].dc() << ',' << tail.expander[i].dc()
                   << ',' << tail.output.dc() << ',' << tail.output.peak << ','
                   << occupancy(e) << ',' << path.core.operatingStats.peak
                   << ',' << -productDb(path.core.operatingStats.peak / 2)
                   << ',' << early.rms() << ',' << late.rms() << ',' << thd
                   << ",1," << t.clamps << ",0\n";
        }
        feedback.flush();
      }
      frontier << c.name << ',' << c.stages << ',' << sr << ',' << c.floor(sr)
               << ',' << c.clock(sr) << ",NA_PENDING_TIMING_JOIN,"
               << nominal.gain << ',' << nominal.snr << ',' << nominal.thd
               << ',' << nominalStartup << ',' << maximumDC << ','
               << (local ? "LOCAL" : "CI_SHORT") << '\n';
      frontier.flush();
      startup.flush();
      gains.flush();
      trade.flush();
      std::cout << "quality " << c.name << ' ' << sr << std::endl;
    }
}
void productFeedbackExperiment(const std::filesystem::path &root, bool local) {
  auto f = productFile(
      root, "bbd_product_feedback_dc_experiment.csv",
      "candidate,sample_rate,pole_hz,feedback,duration_seconds,loop_return_dc,"
      "bucket_mean,output_dc,peak,early_decay_rms,late_decay_rms,frequency_hz,"
      "blocker_gain_db,blocker_phase_radians,finite,scope");
  for (auto c : candidates)
    for (double sr : {48000., 96000.})
      for (double hz : {0., 1., 2., 5.}) {
        if (!productCandidateFilter.empty() && productCandidateFilter != c.name)
          continue;
        // Dedicated state in this harness only. The output blocker's state is
        // independent.
        BBDModulatedDelayVoice v;
        auto cfg = BBDVoiceConfig::fullResearchFixture();
        cfg.physicalStages = c.stages;
        v.prepare(sr, cfg);
        v.enableOperatingInstrumentation(true);
        const double pole = std::exp(-2 * pi * hz / sr), fb = .65;
        double previousReturn = 0, previousInput = 0, state = 0;
        Meter ret, out, early, late;
        int frames = int(sr * (local ? 30. : .25));
        auto before = allocations.load();
        const int onsetEnd = frames * 2 / 3, window = frames / 6;
        double steadyBucket = 0;
        for (int n = 0; n < frames; ++n) {
          double x = n < onsetEnd ? .2 * std::sin(2 * pi * 100 * n / sr) : 0;
          double y = v.process(x + fb * previousReturn, c.floor(sr), 0);
          const double wet = v.feedbackWet();
          if (hz > 0) {
            state = flushSubnormal(wet - previousInput + pole * state);
            previousInput = wet;
            previousReturn = state;
          } else
            previousReturn = wet;
          if (n >= onsetEnd - window && n < onsetEnd) {
            ret.add(previousReturn);
            out.add(y);
          }
          if (n >= frames - window)
            late.add(y);
          if (n >= onsetEnd && n < onsetEnd + window)
            early.add(y);
          if (n == onsetEnd - 1)
            steadyBucket = bucketMean(v);
        }
        require(voiceFinite(v) && v.hiddenClamps == 0 &&
                    before == allocations.load(),
                "qualification feedback blocker gate");
        for (double frequency : {1., 5., 20., 100., 500., 5000.}) {
          auto z = std::polar(1., -2 * pi * frequency / sr);
          const auto h =
              hz > 0 ? (1. - z) / (1. - pole * z) : std::complex<double>(1, 0);
          f << c.name << ',' << sr << ',' << hz << ',' << fb << ','
            << (local ? 30. : .25) << ',' << ret.dc() << ',' << steadyBucket
            << ',' << out.dc() << ',' << out.peak << ',' << early.rms() << ','
            << late.rms() << ',' << frequency << ',' << productDb(std::abs(h))
            << ',' << std::arg(h) << ",1,QUALIFICATION_ONLY_DIRECT_VOICE\n";
        }
        f.flush();
      }
}
void productCosts(const std::filesystem::path &root, bool local) {
  auto f = productFile(root, "bbd_product_cost_decomposition.csv",
                       "candidate,sample_rate,layer,paths,callback_p99_seconds,"
                       "percent_full_engine,incremental_p99_seconds,scope");
  auto architecture =
      productFile(root, "bbd_product_voice_architecture.csv",
                  "candidate,sample_rate,independent_paths,p99_seconds,status");
  for (auto c : candidates)
    for (double sr : {48000., 96000.}) {
      auto full =
          productTiming(sr, c.stages, c.floor(sr), 128, local ? 2048 : 128);
      double previous = 0;
      for (int layer = 0; layer < 5; ++layer) {
        std::array<BBDFullPath, 8> paths;
        for (auto &path : paths) {
          auto cfg = BBDVoiceConfig::fullResearchFixture();
          if (layer < 2)
            cfg.character = {};
          else if (layer == 2)
            cfg.character.nonlinear = {};
          cfg.compander.enabled = layer == 4;
          path.configure(cfg.compander);
          path.setGainStaging(cfg.gainStaging);
          path.core.setQualificationMode(layer == 0
                                             ? BBDMode::TransportOnly
                                             : BBDMode::AsyncLinearReference,
                                         BBDFilterProfile::HoltersParkerTable1);
          path.core.setCharacterConfig(cfg.character);
          path.prepare(sr, c.stages);
          path.setDelaySeconds(c.floor(sr));
          path.core.collectOperatingStats = false;
        }
        std::vector<double> times(local ? 2048 : 128);
        double sink = 0;
        auto before = allocations.load();
        for (std::size_t callback = 0; callback < times.size(); ++callback) {
          auto start = std::chrono::steady_clock::now();
          for (int n = 0; n < 128; ++n)
            for (auto &path : paths)
              sink += path.process(.2);
          times[callback] = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start)
                                .count();
        }
        require(std::isfinite(sink) && before == allocations.load(),
                "cost finite allocation gate");
        double p99 = rank(times, .99);
        f << c.name << ',' << sr << ','
          << (layer == 0   ? "transport"
              : layer == 1 ? "async_filters"
              : layer == 2 ? "character_loss_noise"
              : layer == 3 ? "nonlinearity"
                           : "compander")
          << ",8," << p99 << ',' << 100 * p99 / full.p99 << ','
          << p99 - previous << ",DIRECT_EIGHT_PATHS_EXCLUDES_ENGINE_BANKS\n";
        previous = p99;
      }
      f << c.name << ',' << sr << ",full_engine,8," << full.p99 << ",100,"
        << full.p99 - previous << ",FULL_ENGINE\n";
      architecture << c.name << ',' << sr << ",8," << full.p99
                   << ",INDEPENDENT_PATHS_MEASURED_SHARED_COMPONENTS_NOT_"
                      "MATHEMATICALLY_JUSTIFIED\n";
      f.flush();
      architecture.flush();
    }
}
