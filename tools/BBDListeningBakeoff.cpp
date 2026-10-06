#include "../tests/AllocationTracker.h"
#include "../tests/reference_m26/DriftEngine.h"
#include "BBDListeningFixtures.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>

using namespace listening;
namespace {
using Row=std::map<std::string,std::string>;
struct Csv {
  std::ofstream f; std::vector<std::string> keys;
  Csv(const std::filesystem::path &p, std::vector<std::string> columns)
    : f(output(p)),keys(std::move(columns)) {
    for (std::size_t i=0;i<keys.size();++i) f<<(i?",":"")<<keys[i];
    f<<'\n'; f.flush();
  }
  void add(const Row &r) {
    for (std::size_t i=0;i<keys.size();++i) {
      if (i) f<<',';
      auto it=r.find(keys[i]);
      const std::string value=it==r.end()?"NA":it->second;
      f<<'"'; for (char c:value) { if (c=='"') f<<'"'; f<<c; } f<<'"';
    }
    f<<'\n'; f.flush();
  }
  void close() { f.flush(); f.close(); }
};
const std::vector<std::string> manifestColumns={
  "blind_filename","candidate_id","set","group","source","scene","version",
  "backend","stages","gain_profile","organic_variant","bank_mode","seed","source_seed","source_gain",
  "Motion","Depth","Center","Chaos","Coherence","Dynamics","Feedback","Mix","Width",
  "sample_rate","frames","analysis_begin_frame","analysis_end_frame",
  "requested_excursion_seconds","actual_excursion_seconds","modulation_rms","modulation_peak",
  "delay_min_seconds","delay_max_seconds","bbd_clock_min_hz","bbd_clock_max_hz",
  "product_clock_cap_hz","product_delay_floor_seconds","maximum_events_per_sample_per_voice",
  "total_events","hidden_clamp_count","numerical_guard_count","callback_allocations",
  "internal_peak","nominal_domain_occupancy","output_rms","output_peak","output_dc",
  "stereo_correlation","crest_factor_db","raw_rms","raw_peak","level_match_gain_db",
  "matched_rms","matched_peak","rms_matching_error_db","low_band_rms","mid_band_rms","high_band_rms",
  "approximate_realtime_factor","trajectory_max_error_seconds","deterministic_repeat","status"};
struct Render {
  Audio audio;
  std::vector<std::array<double,8>> delay;
  double requested=0, actual=0, minimum=1, maximum=0, modSquares=0, modPeak=0;
  double clockMin=1e300, clockMax=0, internalPeak=0, occupancy=1, seconds=0;
  std::uint64_t events=0, clamps=0, guards=0, callbackAllocations=0;
  std::uint32_t maxEvents=0;
};
void configure(DriftEngine &engine, const Candidate &c, const Scene &s, int sr) {
  auto cfg=BBDVoiceConfig::fullResearchFixture();
  cfg.physicalStages=c.stages;
  cfg.gainStaging=BBDGainStagingConfig::profile(c.gain);
  require(engine.setDelayBackend(c.backend,cfg),"Cannot configure listening backend");
  engine.setParameters(s.p);
  engine.setOrganicVariant(c.organic); engine.setBankMode(c.bank);
  engine.prepare(sr,engineSeed); engine.reset(engineSeed);
  engine.enableBBDOperatingInstrumentation(true);
}
void admit(const Candidate &c, const Scene &s, int sr) {
  const double center=s.p[Center]*.001;
  const double request=depthSeconds(s.p[Motion],s.p[Depth]);
  const double bound=DriftEngine::combinedModulationBound(c.organic);
  const double minimum=c.backend==DelayBackend::DigitalFractional?4./sr:productFloor(c.stages,sr);
  require(center-request*bound>=minimum-1e-12,"Scene violates conservative product floor");
  require(center+request*bound<=.055,"Scene violates maximum delay");
}
Render render(const Audio &dry, const Candidate &c, const Group &g, int sr) {
  admit(c,g.settings,sr);
  Render r;
  r.audio.resize(dry.size()); r.delay.resize(dry.size());
  DriftEngine engine;
  configure(engine,c,g.settings,sr);
  const auto before=allocations.load();
  const auto start=std::chrono::steady_clock::now();
  for (std::size_t n=0;n<dry.size();++n) {
    // A stereo host input preserves the real stereo engine path for mono source C.
    auto y=engine.processSample(dry[n][0]*g.sourceGain,dry[n][1]*g.sourceGain);
    for (double x:y) require(std::isfinite(x) && std::abs(x)<1-1./8388608.,"Listening output nonfinite or clipped");
    r.audio[n]=y;
    const auto &t=engine.telemetry();
    r.requested=t.requestedExcursionSeconds; r.actual=t.actualExcursionSeconds;
    // The common scenes must be fully admitted even before trajectory comparisons.
    require(std::abs(r.requested-r.actual)<1e-15,"Engine reduced requested listening excursion");
    for (int ch=0;ch<2;++ch) for (int band=0;band<4;++band) {
      const double delay=t.delaySeconds[ch][band], mod=t.modulation[ch][band];
      r.delay[n][ch*4+band]=delay;
      r.minimum=std::min(r.minimum,delay); r.maximum=std::max(r.maximum,delay);
      r.modSquares+=mod*mod; r.modPeak=std::max(r.modPeak,std::abs(mod));
      if (c.backend==DelayBackend::ExperimentalBBD) {
        const auto &v=engine.bbdVoice(ch,band);
        const auto &core=v.signalPath().core;
        const auto &ct=core.telemetry();
        require(delay>=productFloor(c.stages,sr)-1e-12,"Observed trajectory below product floor");
        require(ct.effectiveClockHz<=productClock(c.stages,sr)*(1+1e-12),"Product clock cap exceeded");
        require(ct.eventsThisHostSample<=(c.stages==512?8u:16u),"Product event cap exceeded");
        require(v.finiteState(),"Nonfinite BBD state");
        r.clockMin=std::min(r.clockMin,ct.effectiveClockHz);
        r.clockMax=std::max(r.clockMax,ct.effectiveClockHz);
        r.maxEvents=std::max(r.maxEvents,ct.eventsThisHostSample);
      }
    }
  }
  r.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  r.callbackAllocations=allocations.load()-before;
  require(r.callbackAllocations==0,"Listening callbacks allocated");
  std::uint64_t nominal=0, count=0;
  if (c.backend==DelayBackend::ExperimentalBBD)
    for (int ch=0;ch<2;++ch) for (int b=0;b<4;++b) {
      const auto &v=engine.bbdVoice(ch,b);
      const auto &core=v.signalPath().core;
      r.events+=core.telemetry().totalEventCount;
      r.clamps+=v.hiddenClamps; r.guards+=v.numericalGuards;
      r.internalPeak=std::max(r.internalPeak,core.operatingStats.peak);
      nominal+=core.operatingStats.nominalCount; count+=core.operatingStats.count;
    }
  r.occupancy=count?double(nominal)/count:1;
  require(r.clamps==0 && r.guards==0,"Hidden BBD clamp or numerical guard");
  return r;
}
double trajectoryError(const Render &a, const Render &b) {
  require(a.delay.size()==b.delay.size(),"Trajectory frame count mismatch");
  double error=0;
  for (std::size_t n=0;n<a.delay.size();++n) for (int i=0;i<8;++i)
    error=std::max(error,std::abs(a.delay[n][i]-b.delay[n][i]));
  return error;
}
void oneFactor(const Group &g) {
  const auto &base=g.candidates.front();
  for (const auto &c:g.candidates) {
    require(g.set=="A" || c.backend==base.backend,"Confounded backend");
    require(g.set=="B" || c.stages==base.stages,"Confounded stages");
    require(g.set=="C" || c.gain==base.gain,"Confounded gain");
    require(g.set=="D" || c.organic==base.organic,"Confounded modulation");
    require(g.set=="E" || c.bank==base.bank,"Confounded bank");
  }
}
void identical(const Render &a, const Render &b) {
  require(a.audio.size()==b.audio.size() &&
    std::memcmp(a.audio.data(),b.audio.data(),a.audio.size()*sizeof(a.audio[0]))==0,
    "Audio render not bit identical");
  require(trajectoryError(a,b)==0,"Repeated trajectory not exact");
}
void regression(const Audio &dry, const Group &g, int sr) {
  DriftEngine current;
  Candidate digital{"DigitalReference"}; digital.backend=DelayBackend::DigitalFractional;
  configure(current,digital,g.settings,sr);
  PreM27DriftEngine frozen;
  frozen.setParameters(g.settings.p);
  frozen.setOrganicVariant(digital.organic); frozen.setBankMode(digital.bank);
  frozen.prepare(sr,engineSeed); frozen.reset(engineSeed);
  const auto before=allocations.load();
  for (const auto &x:dry) {
    auto a=current.processSample(x[0]*g.sourceGain,x[1]*g.sourceGain);
    auto b=frozen.processSample(x[0]*g.sourceGain,x[1]*g.sourceGain);
    require(std::memcmp(a.data(),b.data(),sizeof(a))==0,"Frozen Digital regression failed");
  }
  require(before==allocations.load(),"Digital regression callback allocated");
}
void objectiveTests(const std::filesystem::path *root=nullptr) {
  std::unique_ptr<Csv> report;
  if (root) report=std::make_unique<Csv>(*root/"objective"/"sanity_rates.csv",
    std::vector<std::string>{"sample_rate","group","candidate","trajectory_error_seconds","clock_max_hz","max_events","clamps","guards","allocations","deterministic","digital_frozen_regression"});
  for (int sr:{44100,48000,88200}) {
    std::array<Audio,3> dry={source(0,sr,.75),source(1,sr,.75),source(2,sr,.75)};
    for (const auto &g:groups()) {
      oneFactor(g);
      auto baseline=render(dry[g.source],g.candidates.front(),g,sr);
      for (std::size_t i=0;i<g.candidates.size();++i) {
        const auto &c=g.candidates[i];
        auto r=render(dry[g.source],c,g,sr);
        identical(r,render(dry[g.source],c,g,sr));
        const double error=trajectoryError(baseline,r);
        if (g.set!="D") require(error==0 && baseline.requested==r.requested && baseline.actual==r.actual,"Common trajectory is not exact");
        if (report) report->add({{"sample_rate",number(sr)},{"group",g.id},{"candidate",c.id},
          {"trajectory_error_seconds",number(error)},{"clock_max_hz",c.backend==DelayBackend::DigitalFractional?"NA":number(r.clockMax)},
          {"max_events",c.backend==DelayBackend::DigitalFractional?"NA":number(r.maxEvents)},
          {"clamps",c.backend==DelayBackend::DigitalFractional?"NA":number(r.clamps)},
          {"guards",c.backend==DelayBackend::DigitalFractional?"NA":number(r.guards)},
          {"allocations",number(r.callbackAllocations)},{"deterministic","PASS"},{"digital_frozen_regression",g.set=="A"?"PASS":"NA"}});
      }
      if (g.set=="A") regression(dry[g.source],g,sr);
    }
  }
  // The short scene is an inversion of the full conservative bound, not Depth=1.
  const auto shortScene=scene("short");
  require(shortScene.p[Depth]<.1,"Short-depth reserve missing");
  const auto plan=groups();
  std::map<std::string,int> index;
  std::map<std::string,std::vector<int>> positions;
  for (const auto &g:plan) {
    const int k=index[g.set]++;
    auto order=permutation(g.candidates.size(),k);
    require(order==permutation(g.candidates.size(),k),"Blind permutation nondeterministic");
    auto &counts=positions[g.set];
    if (counts.empty()) counts.resize(g.candidates.size());
    auto it=std::find(order.begin(),order.end(),0);
    ++counts[std::size_t(it-order.begin())];
  }
  for (const auto &entry:positions) {
    auto bounds=std::minmax_element(entry.second.begin(),entry.second.end());
    require(*bounds.second-*bounds.first<=1,"Blind positions not balanced");
  }
  if (report) report->close();
}
std::array<double,3> spectralBands(const Audio &audio, int sr) {
  // Fixed one-pole analysis filters only; never used on listening output.
  const double a=1-std::exp(-2*pi*500/sr), b=1-std::exp(-2*pi*4000/sr);
  std::array<double,2> low{}, highLP{}; std::array<double,3> energy{};
  const auto begin=std::size_t(onset*sr), end=std::size_t(excitationEnd*sr);
  for (std::size_t n=0;n<end;++n) for (int ch=0;ch<2;++ch) {
    low[ch]+=a*(audio[n][ch]-low[ch]); highLP[ch]+=b*(audio[n][ch]-highLP[ch]);
    if (n>=begin) {
      const double values[]={low[ch],highLP[ch]-low[ch],audio[n][ch]-highLP[ch]};
      for (int band=0;band<3;++band) energy[band]+=values[band]*values[band];
    }
  }
  for (double &v:energy) v=std::sqrt(v/(2*(end-begin)));
  return energy;
}
Row describe(const Group &g, const Candidate &c, const Render &r, const std::string &name,
             const std::string &version, double scalar, double target, double error, bool repeated) {
  const auto begin=std::size_t(onset*listeningRate),end=std::size_t(excitationEnd*listeningRate);
  const auto raw=measure(r.audio,begin,end), all=measure(r.audio,0,r.audio.size());
  const auto matched=measure(r.audio,begin,end,scalar), mout=measure(r.audio,0,r.audio.size(),scalar);
  auto spectral=spectralBands(r.audio,listeningRate);
  const bool bbd=c.backend==DelayBackend::ExperimentalBBD;
  const auto optional=[&](double x){return bbd?number(x):"NA";};
  Row row={{"blind_filename",version+"/"+name},{"candidate_id",c.id},{"set",g.set},{"group",g.id},
    {"source",std::string(1,sourceLabel(g.source))},{"scene",g.settings.id},{"version",version},
    {"backend",backendName(c.backend)},{"stages",optional(double(c.stages))},
    {"gain_profile",bbd?gainName(c.gain):"NA"},{"organic_variant",organicName(c.organic)},
    {"bank_mode",bankName(c.bank)},{"seed",number(engineSeed)},{"source_seed",number(sourceSeed(g.source))},
    {"source_gain",number(g.sourceGain)},{"sample_rate",number(listeningRate)},{"frames",number(double(r.audio.size()))},
    {"analysis_begin_frame",number(double(begin))},{"analysis_end_frame",number(double(end))},
    {"requested_excursion_seconds",number(r.requested)},{"actual_excursion_seconds",number(r.actual)},
    {"modulation_rms",number(std::sqrt(r.modSquares/(8*r.audio.size())))},{"modulation_peak",number(r.modPeak)},
    {"delay_min_seconds",number(r.minimum)},{"delay_max_seconds",number(r.maximum)},
    {"bbd_clock_min_hz",optional(r.clockMin)},{"bbd_clock_max_hz",optional(r.clockMax)},
    {"product_clock_cap_hz",optional(productClock(c.stages,listeningRate))},
    {"product_delay_floor_seconds",optional(productFloor(c.stages,listeningRate))},
    {"maximum_events_per_sample_per_voice",optional(r.maxEvents)},{"total_events",optional(double(r.events))},
    {"hidden_clamp_count",optional(double(r.clamps))},{"numerical_guard_count",optional(double(r.guards))},
    {"callback_allocations",number(double(r.callbackAllocations))},{"internal_peak",optional(r.internalPeak)},
    {"nominal_domain_occupancy",optional(r.occupancy)},{"output_rms",number(matched.rms())},
    {"output_peak",number(mout.peak)},{"output_dc",number(mout.dc())},
    {"stereo_correlation",number(mout.correlation())},{"crest_factor_db",number(db(mout.peak/matched.rms()))},
    {"raw_rms",number(raw.rms())},{"raw_peak",number(all.peak)},
    {"level_match_gain_db",number(db(scalar))},{"matched_rms",number(matched.rms())},
    {"matched_peak",number(mout.peak)},{"rms_matching_error_db",version=="raw"?"NA":number(std::abs(db(matched.rms()/target)))},
    {"low_band_rms",number(spectral[0]*scalar)},{"mid_band_rms",number(spectral[1]*scalar)},
    {"high_band_rms",number(spectral[2]*scalar)},
    {"approximate_realtime_factor",number(r.seconds/(double(r.audio.size())/listeningRate))},
    {"trajectory_max_error_seconds",g.set=="D"?"NA":number(error)},
    {"deterministic_repeat",repeated?"PASS":"SHORT_SANITY_PASS"},{"status","PASS"}};
  for (std::size_t i=0;i<Count;++i) row[parameterSpecs[i].name]=number(g.settings.p.values[i]);
  return row;
}
void artifact(const std::filesystem::path &root) {
  for (const char *dir:{"dry","raw","level_matched","objective"}) std::filesystem::create_directories(root/dir);
  // Fail on README/open/flush errors before expensive rendering; stream-safety gate.
  auto readme=output(root/"README.txt");
  readme<<"M3.0 BBD Listening Bake-off\nPRODUCTIZATION CANDIDATE / NOT SHIPPING DEFAULT\n"
    <<"Listen first; record preference in LISTENING_SCORECARD.csv; then open ANSWER_KEY.csv.\n"
    <<"The manifest and objective reports also reveal identities. Keep them closed while listening.\n"
    <<"26 processed candidates in 12 groups; RAW + LEVEL_MATCHED = 52 listening WAVs, plus 3 dry references.\n"
    <<"All WAVs: 48 kHz stereo PCM 24-bit; 480000 frames (10 s).\n"
    <<"Leading silence 0.5 s; excitation ends at 7.5 s; 2.5 s feedback/noise tail.\n"
    <<"Matching: active-region pooled stereo RMS [0.5,7.5) s. One constant scalar for the entire file.\n"
    <<"Common target=min(member active RMS), reduced for all members if any scaled peak would exceed 0.98.\n"
    <<"No limiter, compressor, EQ matching or waveform rescaling.\n"
    <<"Blind seed: 0x4d333042 ("<<bakeoffSeed<<"); engine seed: 77; BBD fixture seed: 570.\n"
    <<"Seeded cyclic Latin rotations; positions differ by at most one occurrence within a set.\n"
    <<"Source C is mono material duplicated to stereo host channels, preserving stereo modulation.\n"
    <<"C01 uses DRY_A at constant source gain 0.125 for BOTH candidates. Other source gains are 1.\n"
    <<"Balanced: 1024 stages, Table1 filters, full M2.2 character, strength-1 engineering transfer,\n"
    <<"enabled 0.47 uF / 10 kohm compander, Nominal gain, ExternalWetReturn, existing 5 Hz output blocker.\n"
    <<"Economy changes only physical stages and its corresponding M2.9 clock budget.\n"
    <<"No subjective winner was selected. DigitalFractional remains the plugin/default backend.\n\nScene controls:\n";
  for (const char *id:{"subtle","wide","slow","short","feedback","wide_high_chaos"}) {
    auto s=scene(id); readme<<id;
    for (std::size_t i=0;i<Count;++i) readme<<' '<<parameterSpecs[i].name<<'='<<s.p.values[i];
    readme<<'\n';
  }
  readme.flush();
  std::filesystem::copy_file(std::filesystem::path(DRIFT_LISTENING_DOCS)/"m3_0_listening_guide.md",
    root/"LISTENING_GUIDE.md",std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(std::filesystem::path(DRIFT_LISTENING_DOCS)/"m3_0_bbd_listening_bakeoff.md",
    root/"TECHNICAL_NOTES.md",std::filesystem::copy_options::overwrite_existing);
  Csv manifest(root/"listening_render_manifest.csv",manifestColumns);
  Csv key(root/"ANSWER_KEY.csv",{"group","blind_filename","candidate_id","backend","stages","gain_profile","organic_variant","bank_mode"});
  Csv score(root/"LISTENING_SCORECARD.csv",{"set","pair_group","source","scene","blind_candidates","preferred_candidate","preference_strength_1_5","modulation_naturalness_1_5","stereo_quality_1_5","tonal_quality_1_5","transient_quality_1_5","tail_quality_1_5","artifacts_1_5","fatigue_1_5","comments"});
  Csv scenes(root/"scene_parameters.csv",{"scene","Motion","Depth","Center","Chaos","Coherence","Dynamics","Feedback","Mix","Width","conservative_bound_Wander","conservative_bound_PaperNarrowband","short_safe_depth_48k","short_safe_depth_44k1"});
  for (const char *id:{"subtle","wide","slow","short","feedback","wide_high_chaos"}) {
    auto s=scene(id); Row row={{"scene",id},{"conservative_bound_Wander",number(DriftEngine::combinedModulationBound(OrganicVariant::Wander))},
      {"conservative_bound_PaperNarrowband",number(DriftEngine::combinedModulationBound(OrganicVariant::PaperNarrowband))},
      {"short_safe_depth_48k",number(safeDepth(.9,.0025,48000,OrganicVariant::Wander))},
      {"short_safe_depth_44k1",number(safeDepth(.9,.0025,44100,OrganicVariant::Wander))}};
    for (std::size_t i=0;i<Count;++i) row[parameterSpecs[i].name]=number(s.p.values[i]);
    scenes.add(row);
  }
  scenes.close();
  std::array<Audio,3> dry={source(0,listeningRate),source(1,listeningRate),source(2,listeningRate)};
  for (int i=0;i<3;++i) {
    const auto name="dry/DRY_"+std::string(1,sourceLabel(i))+".wav";
    wav(root/name,dry[i],listeningRate);
    const auto m=measure(dry[i],0,dry[i].size());
    const auto active=measure(dry[i],std::size_t(onset*listeningRate),std::size_t(excitationEnd*listeningRate));
    manifest.add({{"blind_filename",name},{"candidate_id","DryReference"},{"source",std::string(1,sourceLabel(i))},
      {"version","dry"},{"source_seed",number(sourceSeed(i))},{"source_gain","1"},{"sample_rate","48000"},
      {"frames",number(double(dry[i].size()))},{"analysis_begin_frame",number(onset*listeningRate)},
      {"analysis_end_frame",number(excitationEnd*listeningRate)},{"output_rms",number(active.rms())},
      {"output_peak",number(m.peak)},{"output_dc",number(m.dc())},{"stereo_correlation",number(m.correlation())},{"status","PASS"}});
  }
  const char *reportNames[]={"backend_comparison.csv","stage_comparison.csv","gain_comparison.csv","modulation_comparison.csv","bank_comparison.csv"};
  std::array<std::unique_ptr<Csv>,5> reports;
  for (int i=0;i<5;++i) reports[i]=std::make_unique<Csv>(root/"objective"/reportNames[i],manifestColumns);
  std::map<std::string,int> groupIndex;
  double maxError=0, maxClock=0, maxPeak=0; std::uint32_t maxEvents=0;
  for (const auto &g:groups()) {
    oneFactor(g);
    std::vector<Render> renders;
    for (const auto &c:g.candidates) renders.push_back(render(dry[g.source],c,g,listeningRate));
    double target=1;
    for (const auto &r:renders) {
      const auto active=measure(r.audio,std::size_t(onset*listeningRate),std::size_t(excitationEnd*listeningRate));
      const auto all=measure(r.audio,0,r.audio.size());
      require(active.rms()>1e-12,"Cannot level-match a silent render");
      target=std::min({target,active.rms(),.98*active.rms()/all.peak});
    }
    const auto order=permutation(renders.size(),groupIndex[g.set]++);
    std::string labels;
    for (std::size_t slot=0;slot<order.size();++slot) {
      const auto index=order[slot]; const auto &c=g.candidates[index]; const auto &r=renders[index];
      const auto name="SET_"+g.id+"_"+std::string(1,char('X'+slot))+".wav";
      if (!labels.empty()) labels+=';'; labels+=name;
      const double error=trajectoryError(renders.front(),r);
      if (g.set!="D") require(error==0 && r.requested==renders.front().requested && r.actual==renders.front().actual,"Listening trajectories differ");
      const bool repeated=g.id=="A01" || (g.id=="B02" && index==0) || g.id=="D02";
      if (repeated) identical(r,render(dry[g.source],c,g,listeningRate));
      if (g.set=="A" && c.backend==DelayBackend::DigitalFractional) regression(dry[g.source],g,listeningRate);
      const double scalar=target/measure(r.audio,std::size_t(onset*listeningRate),std::size_t(excitationEnd*listeningRate)).rms();
      for (const std::string version:{"raw","level_matched"}) {
        const double gain=version=="raw"?1:scalar;
        wav(root/version/name,r.audio,listeningRate,gain);
        auto row=describe(g,c,r,name,version,gain,target,error,repeated);
        manifest.add(row); if (version=="raw") reports[std::size_t(g.set[0]-'A')]->add(row);
        maxPeak=std::max(maxPeak,std::stod(row["output_peak"]));
        if (version=="level_matched") {
          const double matching=std::stod(row["rms_matching_error_db"]);
          require(matching<=.1,"RMS matching error exceeds 0.1 dB"); maxError=std::max(maxError,matching);
        }
      }
      key.add({{"group",g.id},{"blind_filename",name},{"candidate_id",c.id},{"backend",backendName(c.backend)},
        {"stages",c.backend==DelayBackend::DigitalFractional?"NA":number(double(c.stages))},
        {"gain_profile",c.backend==DelayBackend::DigitalFractional?"NA":gainName(c.gain)},
        {"organic_variant",organicName(c.organic)},{"bank_mode",bankName(c.bank)}});
      if (c.backend==DelayBackend::ExperimentalBBD) { maxClock=std::max(maxClock,r.clockMax); maxEvents=std::max(maxEvents,r.maxEvents); }
    }
    Row scores={{"set",g.set},{"pair_group",g.id},{"source",std::string(1,sourceLabel(g.source))},{"scene",g.settings.id},{"blind_candidates",labels}};
    for (const auto &column:score.keys) if (!scores.count(column)) scores[column]="";
    score.add(scores);
    std::cout<<g.id<<": objective gates passed\n"<<std::flush;
  }
  for (auto &report:reports) report->close();
  manifest.close(); key.close(); score.close();
  objectiveTests(&root);
  readme<<"\nObjective results: maximum pre-PCM matching error="<<maxError<<" dB; maximum output peak="<<maxPeak
    <<"; maximum clock="<<maxClock<<" Hz; maximum physical events/sample/voice="<<maxEvents<<".\n"
    <<"Exact Digital/BBD trajectories at every sample/channel/band; zero BBD hidden clamps/guards and callback allocations.\n"
    <<"Full-length deterministic repeats: A01 (both), B02 Economy, D02 (all three); every configuration repeated in short three-rate sanity.\n"
    <<"CPU factor is instrumented render time / audio duration, including telemetry, not a realtime qualification.\n"
    <<"Spectral RMS bands use fixed one-pole analysis at 500 and 4000 Hz, not disjoint FFT bands or an EQ match.\n";
  readme.flush(); readme.close();
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc==2 && std::string(argv[1])=="--tests-only") objectiveTests();
    else {
      require(argc==2,"Usage: drift_bbd_listening_bakeoff OUTPUT | --tests-only");
      artifact(argv[1]);
    }
    std::cout<<"M3.0 objective gates passed. No subjective winner was selected.\n";
    return 0;
  } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
