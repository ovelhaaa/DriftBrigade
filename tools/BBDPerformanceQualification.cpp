#include "dsp/ClockedBBDCore.h"
#include "../tests/reference/ClockedBBDCore.h"
#include "../tests/AllocationTracker.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
using Clock=std::chrono::steady_clock;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct Error {
 double absolute=0,relative=0,square=0; std::uint64_t n=0;
 void add(double a,double b) { require(std::isfinite(a)&&std::isfinite(b),"nonfinite comparison"); const double e=std::abs(a-b); absolute=std::max(absolute,e); relative=std::max(relative,e/std::max(1e-15,std::abs(b))); square+=e*e; ++n; }
 double rms() const { return n?std::sqrt(square/n):0; }
};
std::uint32_t randomState=42;
double randomUnit() { randomState^=randomState<<13; randomState^=randomState>>17; randomState^=randomState<<5; return randomState/4294967296.; }
void setup(drift::ClockedBBDCore& c,double rate,std::size_t stages,int profile,int character) {
 drift::BBDCharacterConfig cfg; cfg.mode=static_cast<drift::BBDCharacterMode>(character); cfg.seed=42;
 cfg.insertionDb=-1; cfg.lossPerStage=1e-5; cfg.leakagePerStageSecond=.1; cfg.residualPolePer1024=.25;
 cfg.inputNoiseRms=1e-5; cfg.outputNoiseRms=1e-4; cfg.mismatchFraction=.001;
 c.setCharacterConfig(cfg); c.setQualificationMode(profile?drift::BBDMode::AsyncLinearReference:drift::BBDMode::TransportOnly,
 profile==1?drift::BBDFilterProfile::ValidationPrototype:drift::BBDFilterProfile::HoltersParkerTable1); c.prepare(rate,stages);
}
void setup(drift_reference::ClockedBBDCore& c,double rate,std::size_t stages,int profile,int character) {
 drift_reference::BBDCharacterConfig cfg; cfg.mode=static_cast<drift_reference::BBDCharacterMode>(character); cfg.seed=42;
 cfg.insertionDb=-1; cfg.lossPerStage=1e-5; cfg.leakagePerStageSecond=.1; cfg.residualPolePer1024=.25;
 cfg.inputNoiseRms=1e-5; cfg.outputNoiseRms=1e-4; cfg.mismatchFraction=.001;
 c.setCharacterConfig(cfg); c.setQualificationMode(profile?drift_reference::BBDMode::AsyncLinearReference:drift_reference::BBDMode::TransportOnly,
 profile==1?drift_reference::BBDFilterProfile::ValidationPrototype:drift_reference::BBDFilterProfile::HoltersParkerTable1); c.prepare(rate,stages);
}
double delayAt(int trajectory,int i,int n,double nominal,double control) {
 switch(trajectory) {
 case 1: return nominal*(1+.15*std::sin(6.283185307179586*i/48000.));
 case 2: return nominal*(.5+1.5*i/n);
 case 3: return nominal*(i<n/2?1:.4);
 case 4: return nominal*(1+.3*control);
 default: return nominal;
 }
}
void filters(std::ostream& out,double& maxError,double& stressRelative) {
 out<<"profile,stimulus,interval,max_absolute,max_relative,rms,normalized_max_absolute\n";
 for(int profile=0;profile<3;++profile) for(int stimulus=0;stimulus<5;++stimulus) for(int interval=0;interval<7;++interval) {
  drift::AsyncAnalogFilter a; drift_reference::AsyncAnalogFilter b;
  if(profile==0) { a.prototype(2000); b.prototype(2000); } else { a.paperReference(profile==2); b.paperReference(profile==2); }
  Error e; const double scale=stimulus==4?static_cast<double>(std::numeric_limits<float>::max()):1;
  randomState=42;
  for(int i=0;i<4096;++i) {
   double dt=1./48000;
   switch(interval) { case 1: dt=1./51200; break; case 2: dt=1./3072000; break; case 3: dt=3.14159e-6; break; case 4: dt=1e-14; break; case 5: dt=.1; break; case 6: dt=1e-9+randomUnit()*1e-4; break; }
   double x=stimulus==0?(i==0?1:0):stimulus==1?1:stimulus==2?std::sin(i*.17):stimulus==3?2*randomUnit()-1:(i%2?scale:-scale);
   if(stimulus==0) { a.injectImpulse(x/48000); b.injectImpulse(x/48000); x=0; }
   // Alternate explicit cached API and the arbitrary-interval convenience path.
   const auto transition=a.makeTransition(dt);
   const double y=i%2?a.advance(dt,x):a.advanceWithTransition(transition,x);
   e.add(y,b.advance(dt,x)); require(a.finiteState()&&b.finiteState(),"filter state finite");
  }
  out<<profile<<','<<stimulus<<','<<interval<<','<<e.absolute<<','<<e.relative<<','<<e.rms()<<','<<e.absolute/scale<<'\n';
  require(e.absolute/scale<=1e-11,"filter differential tolerance");
  if(stimulus!=4) maxError=std::max(maxError,e.absolute); else stressRelative=std::max(stressRelative,e.absolute/scale);
 }
}
void cores(std::ostream& out,double& maxError) {
 out<<"host_rate,stages,profile,character,trajectory,max_output_absolute,max_relative,rms,max_hold_error,max_filter_output_error,max_gain_error,max_filter_state_error,telemetry_exact\n";
 for(double rate:{44100.,48000.,88200.,96000.}) for(std::size_t stages:{256u,512u,1024u,2048u,4096u})
 for(int profile:{1,2}) for(int character=0;character<4;++character) for(int trajectory=0;trajectory<7;++trajectory) {
  drift::ClockedBBDCore a; drift_reference::ClockedBBDCore b; setup(a,rate,stages,profile,character); setup(b,rate,stages,profile,character);
  Error e,h,f,g,state; randomState=42; double control=0; const int n=trajectory==5?1024:8192;
  for(int i=0;i<n;++i) {
   control=.995*control+.005*(2*randomUnit()-1);
   const double delay=trajectory==5 ? stages/(rate*128)*(i<n/2?1:1.001) : trajectory==6 ? stages/(rate*.1) : delayAt(trajectory,i,n,.01,control); a.setDelaySeconds(delay); b.setDelaySeconds(delay);
   const double input=.3*std::sin(i*.13)+.1*(2*randomUnit()-1)+(i==0?.5:0);
   e.add(a.process(input),b.process(input)); h.add(a.heldOutput(),b.heldOutput());
   f.add(a.inputFilterValue(),b.inputFilterValue()); f.add(a.outputFilterValue(),b.outputFilterValue());
   const auto ai=a.inputFilterState(),bi=b.inputFilterState(),ao=a.outputFilterState(),bo=b.outputFilterState();
   for(std::size_t j=0;j<5;++j) { state.add(ai[j].real(),bi[j].real()); state.add(ai[j].imag(),bi[j].imag()); state.add(ao[j].real(),bo[j].real()); state.add(ao[j].imag(),bo[j].imag()); }
   g.add(a.deviceCharacter().loss.gain,b.deviceCharacter().loss.gain);
   const auto& x=a.telemetry(); const auto& y=b.telemetry();
   require(x.totalEventCount==y.totalEventCount&&x.totalCaptureCount==y.totalCaptureCount&&x.totalOutputCount==y.totalOutputCount&&
    x.eventsThisHostSample==y.eventsThisHostSample&&x.capturesThisHostSample==y.capturesThisHostSample&&x.outputsThisHostSample==y.outputsThisHostSample&&
    x.lastCaptureTimeSeconds==y.lastCaptureTimeSeconds&&x.lastOutputTimeSeconds==y.lastOutputTimeSeconds&&x.accumulatedClockPhase==y.accumulatedClockPhase&&
    x.nextEdgeCaptures==y.nextEdgeCaptures&&x.requestedDelaySeconds==y.requestedDelaySeconds&&x.effectiveDelaySeconds==y.effectiveDelaySeconds&&
    x.effectiveClockHz==y.effectiveClockHz&&x.wasClamped==y.wasClamped,"core event telemetry differs");
   if(i%256==0) { require(a.finiteState()&&b.finiteState(),"core finite state"); for(std::size_t j=0;j<stages/2;++j) require(std::abs(a.stageValue(j)-b.stageValue(j))<=1e-9,"bucket equivalence"); }
  }
  require(e.absolute<=1e-9&&h.absolute<=1e-9&&f.absolute<=1e-9&&g.absolute<=1e-12&&state.absolute<=1e-9,"core differential tolerance");
  maxError=std::max(maxError,e.absolute);
  out<<rate<<','<<stages<<','<<profile<<','<<character<<','<<trajectory<<','<<e.absolute<<','<<e.relative<<','<<e.rms()<<','<<h.absolute<<','<<f.absolute<<','<<g.absolute<<','<<state.absolute<<",1\n";
 }
}
void operations(std::ostream& out) {
 out<<"edges_per_sample,modulated,physical_events,advances,transition_builds,period_applications,arbitrary_builds,real_exp,sin_cos_pairs,old_complex_exp,host_samples\n";
 for(double edges:{.1,1.,4.,16.,127.9,128.}) for(bool modulated:{false,true}) {
  drift::ClockedBBDCore c; setup(c,48000,1024,2,0); const int n=4096;
  c.setDelaySeconds(1024/(48000*edges)); drift::asyncOperations={};
  for(int i=0;i<n;++i) { c.setDelaySeconds(1024/(48000*edges)*(modulated?1+.001*std::sin(i*.001):1)); c.process(.1); }
  const auto k=drift::asyncOperations; const auto events=c.telemetry().totalEventCount;
  require(k.builds<=static_cast<unsigned>(n)*(modulated?6u:4u),"transition builds not bounded");
  require(k.realExp<=static_cast<unsigned>(n)*(modulated?18u:12u),"exp count not bounded");
  require(k.advances<=events+2*n,"irrelevant filter event advance");
  out<<edges<<','<<modulated<<','<<events<<','<<k.advances<<','<<k.builds<<','<<k.periodApplications<<','<<k.arbitraryBuilds<<','<<k.realExp<<','<<k.sinCosPairs<<','<<5*(2*events+2*n)<<','<<n<<'\n';
 }
 drift::ClockedBBDCore c; setup(c,48000,1024,2,3); c.setDelaySeconds(.01);
 const auto before=allocations.load();
 for(int i=0;i<10000;++i) { c.setDelaySeconds(.01+.001*std::sin(i*.01)); c.process(.1); }
 require(before==allocations.load(),"realtime allocation");
 drift::BBDDeviceCharacter neutral; drift::BBDCharacterConfig neutralConfig; neutralConfig.mode=drift::BBDCharacterMode::FullLinearCharacter; neutral.configure(neutralConfig); drift::asyncOperations={}; neutral.prepare(1024); neutral.update(51200);
 require(drift::asyncOperations.characterExp==0&&drift::asyncOperations.characterPow==0,"neutral character transcendental work");
 drift::asyncOperations={}; for(int i=0;i<100;++i) c.setDelaySeconds(.01);
 // First change may build two transitions; the other 99 must reuse identity.
 require(drift::asyncOperations.builds<=2,"same clock rebuilt");
}
constexpr int benchmarkSamples=12000;
constexpr double benchmarkAudioSeconds=benchmarkSamples/48000.;
volatile double benchmarkChecksum=0;
struct Timing { double seconds=0; std::uint64_t events=0; drift::AsyncOperationCounts operations; };
template<class Core> Timing benchmark(std::size_t stages,double delay,int profile,int character,bool modulated,int voices) {
 std::vector<Core> c(static_cast<std::size_t>(voices)); for(auto& core:c) { setup(core,48000,stages,profile,character); core.setDelaySeconds(delay); }
 constexpr int n=benchmarkSamples; // Median of five identical 250 ms audio trials.
 std::array<double,5> times{}; Timing result;
 for(auto& time:times) {
  for(auto& core:c) core.reset(); drift::asyncOperations={}; double checksum=0;
  const auto start=Clock::now();
  for(int i=0;i<n;++i) {
   const double d=delay*(modulated?1+.15*std::sin(6.283185307179586*i/48000.):1);
   for(auto& core:c) { core.setDelaySeconds(d); checksum+=core.process(.1); }
  }
  time=std::chrono::duration<double>(Clock::now()-start).count(); benchmarkChecksum=checksum;
  result.operations=drift::asyncOperations; result.events=0; for(const auto& core:c) result.events+=core.telemetry().totalEventCount;
 }
 std::sort(times.begin(),times.end()); result.seconds=times[2]; return result;
}
void timingRow(std::ostream& out,std::size_t stages,double delay,int profile,int character,bool modulated,int voices,const Timing& old,const Timing& now,double stereoSeconds=0) {
 const auto& k=now.operations;
 const std::uint64_t oldExp=profile?5*(2*old.events+2*benchmarkSamples*voices):0;
 out<<stages<<','<<delay<<','<<profile<<','<<character<<','<<modulated<<','<<voices<<','<<benchmarkAudioSeconds<<','<<old.seconds<<','<<now.seconds<<','<<now.seconds/benchmarkAudioSeconds<<','<<now.events/benchmarkAudioSeconds<<','<<benchmarkSamples*voices/benchmarkAudioSeconds<<','<<k.builds/benchmarkAudioSeconds<<','<<(k.realExp+2*k.sinCosPairs+k.characterExp+k.characterPow)/benchmarkAudioSeconds<<','<<oldExp/benchmarkAudioSeconds<<','<<old.seconds/now.seconds<<','<<k.characterExp/benchmarkAudioSeconds<<','<<((character==1||character==3)?3*benchmarkSamples*voices/benchmarkAudioSeconds:0)<<','<<stereoSeconds/benchmarkAudioSeconds<<','<<(stereoSeconds?now.seconds/stereoSeconds:0)<<'\n';
}
void benchmarks(std::ostream& fixed,std::ostream& modulation,std::ostream& multi) {
 const char* header="stages,delay_seconds,profile,character,modulated,cores,audio_seconds,old_wall_seconds,new_wall_seconds,new_realtime_factor,events_per_audio_second,host_samples_per_audio_second,transition_builds_per_audio_second,new_transcendental_calls_per_audio_second,old_complex_exp_per_audio_second,speedup,new_character_exp_per_audio_second,old_character_transcendental_calls_per_audio_second,stereo_realtime_factor,cpu_scaling_vs_stereo\n";
 fixed<<header; modulation<<header; multi<<header;
 for(std::size_t stages:{256u,512u,1024u,2048u,4096u}) for(double delay:{.003,.01,.03}) for(int profile=0;profile<4;++profile) for(bool modulated:{false,true}) {
  int character=profile<2?0:profile==2?1:3; int filter=profile==0?0:2;
  auto old=benchmark<drift_reference::ClockedBBDCore>(stages,delay,filter,character,modulated,2);
  auto now=benchmark<drift::ClockedBBDCore>(stages,delay,filter,character,modulated,2);
  timingRow(modulated?modulation:fixed,stages,delay,filter,character,modulated,2,old,now);
 }
 for(std::size_t stages:{512u,1024u,2048u,4096u}) for(double delay:{.01,.03}) for(int character:{0,3}) {
  auto old=benchmark<drift_reference::ClockedBBDCore>(stages,delay,2,character,false,8);
  auto now=benchmark<drift::ClockedBBDCore>(stages,delay,2,character,false,8);
  auto stereo=benchmark<drift::ClockedBBDCore>(stages,delay,2,character,false,2);
  timingRow(multi,stages,delay,2,character,false,8,old,now,stereo.seconds);
 }
}
int main(int argc,char** argv) {
 try {
  const std::filesystem::path dir=argc>1?argv[1]:"bbd_performance_qualification"; std::error_code ec; std::filesystem::create_directories(dir,ec);
  if(ec||!std::filesystem::is_directory(dir,ec)||ec) return 1;
  const std::array<const char*,7> names{{"async_filter_reference_error.csv","bbd_core_equivalence.csv","transition_operation_counts.csv","bbd_performance.csv","bbd_multivoice_performance.csv","bbd_clock_modulation_performance.csv","README.txt"}};
  std::array<std::ofstream,7> out;
  for(std::size_t i=0;i<out.size();++i) { out[i].open(dir/names[i]); out[i]<<std::setprecision(17); if(!out[i]) return 1; }
  // Reject an already-failing destination (e.g. /dev/full) before expensive work.
  out[6]<<"DriftBrigade-M2.3-BBD-Performance-Qualification\n"; out[6].flush();
  require(bool(out[6]),"artifact initial write/flush failed");
  double filterError=0,coreError=0,stressError=0;
  filters(out[0],filterError,stressError); cores(out[1],coreError); operations(out[2]);
  // Tests/sanitizers can skip machine-dependent timing, never numerical work.
  if(argc<3||std::string(argv[2])!="--numerical-only") benchmarks(out[3],out[5],out[4]);
  out[6]<<"Numerical acceptance: filter max absolute <=1e-11 at unit scale; core <=1e-9. Maximum filter error="<<filterError<<"; core="<<coreError<<".\n"
   <<"Alternating maximum finite audio input uses float max (the core input clamp). Absolute roundoff scales with amplitude; normalized absolute error="<<stressError<<". All states must remain finite. No reduced precision.\n"
   <<"Profiles: 0 transport,1 prototype,2 Table1. Character: 0 ideal,1 loss,2 noise,3 full. Filter stimulus: 0 impulse,1 step,2 sine,3 random,4 alternating float max. Intervals: 0 host,1 nominal period,2 max-clock period,3 fractional,4 tiny,5 long,6 random.\n"
   <<"1120 core cases,8192 samples each (1024 at maximum edge rate); four host rates,five stage counts,two filters,four characters,five trajectories plus maximum-edge and low-edge probes. Identical seeds; exact scheduling telemetry, bucket history, held value, filter outputs and device gain checked.\n"
   <<"Structural acceptance: instrumented builds <=4 per fixed-clock host interval, <=6 on a clock change; real exp <=12/18. Counters compiled only in this executable. Old complex exp counts derived exactly from the frozen loop: five poles times two filters times (events+host samples). New transcendental calls count real exp plus two calls per sin/cos pair plus character exp/pow. These units differ; compare structural scaling, not just call totals.\n"
   <<"Machine-dependent timing: medians of five 250ms audio trials,48kHz,identical .1 input,same executable/machine. Realtime factor is wall/audio (lower is better), including per-sample delay calls. No wall-time CI gate. Instrumentation adds counter overhead to the new path; timings are conservative.\n"
   <<"8 independent cores are measured directly; divide their wall time by the matching stereo result to obtain CPU scaling. Extrapolation: a complete four-band stereo engine would add crossover/modulation/mixing overhead to this measured eight-core cost. No stage count selected.\n"
   <<"All existing analytical,allocation,artifact and sanitizer tests remain separate requirements. Production remains DigitalFractionalDelay; no approximate math or new sonic behavior.\n";
  bool ok=true; for(auto& stream:out) { stream.flush(); const bool written=bool(stream); stream.close(); ok=ok&&written&&!stream.fail(); }
  require(ok,"artifact write/flush/close failed");
  std::cout<<"Filter max error "<<filterError<<", core max error "<<coreError<<"; wrote "<<dir<<'\n';
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

