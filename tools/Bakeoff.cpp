#include "dsp/DriftEngine.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int sampleRate=48000;
constexpr std::uint32_t seed=0x4d313142u;
void word(std::ofstream& f,std::uint32_t x,int bytes){for(int i=0;i<bytes;++i)f.put(static_cast<char>((x>>(8*i))&255));}
struct Wav {
    std::ofstream f; std::uint32_t frames=0;
    explicit Wav(const std::filesystem::path& p):f(p,std::ios::binary){f.seekp(44);}
    void add(double l,double r){for(double x:{l,r}){auto s=static_cast<std::int16_t>(std::clamp(x,-1.,1.)*32767);word(f,static_cast<std::uint16_t>(s),2);}++frames;}
    ~Wav(){if(!f)return;const auto bytes=frames*4;f.seekp(0);f.write("RIFF",4);word(f,36+bytes,4);f.write("WAVEfmt ",8);word(f,16,4);word(f,1,2);word(f,2,2);word(f,sampleRate,4);word(f,sampleRate*4,4);word(f,4,2);word(f,16,2);f.write("data",4);word(f,bytes,4);}
};
enum class Source { Comparison, Broadband, Dynamic };
double source(Source kind,int n) {
    const double t=n/static_cast<double>(sampleRate);
    if(kind==Source::Comparison) {
        const int section=n/(4*sampleRate); const double local=std::fmod(t,4.0);
        if(section==0) return 0.22*(std::sin(2*drift::pi*110*t)+.5*std::sin(2*drift::pi*220*t)+.25*std::sin(2*drift::pi*330*t));
        if(section==1){const double decay=std::exp(-5*std::fmod(local,.5));return .42*decay*(std::sin(2*drift::pi*147*t)+.45*std::sin(2*drift::pi*294*t));}
    }
    // Deterministic broadband pseudo-noise, gently low-passed without state.
    std::uint32_t x=static_cast<std::uint32_t>(n)*747796405u+2891336453u;x^=x>>16;x*=2246822519u;x^=x>>13;
    const double noise=(static_cast<double>(x)/4294967295.0*2-1);
    if(kind==Source::Dynamic){const double amp=t<3?.025:t<7?.6:.025;return amp*(.75*std::sin(2*drift::pi*196*t)+.25*std::sin(2*drift::pi*784*t));}
    return .28*noise;
}
drift::EngineParameters settings(double chaos=.55) {
    drift::EngineParameters p;p[drift::Motion]=.7;p[drift::Depth]=.7;p[drift::Center]=7;p[drift::Chaos]=chaos;p[drift::Coherence]=.45;p[drift::Dynamics]=.25;p[drift::Feedback]=.2;p[drift::Mix]=.65;p[drift::Width]=.45;return p;
}
void render(const std::filesystem::path& dir,const std::string& name,drift::OrganicVariant variant,double chaos,Source sourceType,
            double coherence=.45,drift::BankMode bank=drift::BankMode::Gentle,drift::DynamicsMode dynamics=drift::DynamicsMode::Current,bool telemetry=false) {
    auto p=settings(chaos);p[drift::Coherence]=coherence;if(name.find("Coherence")!=std::string::npos){p[drift::Width]=0;p[drift::Center]=1.5;p[drift::Feedback]=.4;}
    if(name.find("Bank_")!=std::string::npos){p[drift::Width]=0;p[drift::Center]=3;p[drift::Feedback]=.3;}
    drift::DriftEngine e;e.setParameters(p);e.setOrganicVariant(variant);e.setBankMode(bank);e.setDynamicsMode(dynamics);e.prepare(sampleRate,seed);
    Wav wav(dir/(name+".wav"));std::ofstream csv;if(telemetry){csv.open(dir/(name+".csv"));csv<<"time,organic_control,mod_l0,mod_l1,mod_l2,mod_l3,delay_l0,delay_l1,delay_l2,delay_l3,envelope,effective_chaos,effective_feedback,carrier,random_envelope,instantaneous_rate,phase_derivative\n"<<std::setprecision(12);}
    const int frames=12*sampleRate;
    for(int n=0;n<frames;++n){double x=source(sourceType,n);auto y=e.processSample(x,x);wav.add(.55*y[0],.55*y[1]);if(telemetry&&n%48==0){const auto&t=e.telemetry();csv<<n/double(sampleRate)<<','<<t.organic;for(double v:t.modulation[0])csv<<','<<v;for(double v:t.delaySeconds[0])csv<<','<<v;csv<<','<<t.envelope<<','<<t.effectiveChaos<<','<<t.effectiveFeedback<<','<<t.organicDiagnostics.carrier<<','<<t.organicDiagnostics.randomEnvelope<<','<<t.organicDiagnostics.instantaneousRate<<','<<t.organicDiagnostics.phaseDerivative<<'\n';}}
}
const char* variantName(drift::OrganicVariant v){return v==drift::OrganicVariant::Wander?"A":v==drift::OrganicVariant::PaperNarrowband?"B":"C";}
void statistics(){
    std::cout<<"CONTROL_STATISTICS variant,motion,chaos,mean,rms,peak,max_step,max_step_near_transition\n";
    for(auto v:{drift::OrganicVariant::Wander,drift::OrganicVariant::PaperNarrowband,drift::OrganicVariant::PhaseDrift})for(double rate:{.2,.7,2.,6.})for(double chaos:{0.,.25,.5,.75,1.}){
        drift::OrganicModulator m;m.prepare(sampleRate);m.setVariant(v);m.reset(seed);double sum=0,sq=0,peak=0,step=0,transition=0,prev=m.process(rate,chaos),prevPhase=m.segmentPhase();const int count=20*sampleRate;
        for(int i=1;i<count;++i){double phase=m.segmentPhase(),x=m.process(rate,chaos),d=std::abs(x-prev);sum+=x;sq+=x*x;peak=std::max(peak,std::abs(x));step=std::max(step,d);if(phase<prevPhase||phase<.002||phase>.998)transition=std::max(transition,d);prev=x;prevPhase=phase;}
        double mean=sum/(count-1);std::cout<<variantName(v)<<','<<rate<<','<<chaos<<','<<mean<<','<<std::sqrt(sq/(count-1)-mean*mean)<<','<<peak<<','<<step<<','<<transition<<'\n';
    }
    for(auto v:{drift::OrganicVariant::Wander,drift::OrganicVariant::PaperNarrowband,drift::OrganicVariant::PhaseDrift}){drift::DriftEngine e;auto p=settings();e.setParameters(p);e.setOrganicVariant(v);e.prepare(sampleRate,seed);auto begin=std::chrono::steady_clock::now();double check=0;for(int i=0;i<480000;++i)check+=e.processSample(.1,.1)[0];double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();std::cout<<"CPU "<<variantName(v)<<' '<<s<<" seconds checksum "<<check<<'\n';}
}
}
int main(int argc,char**argv){
    const std::filesystem::path dir=argc>1?argv[1]:"bakeoff";std::filesystem::create_directories(dir);
    render(dir,"01_A_Wander",drift::OrganicVariant::Wander,.5,Source::Comparison,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current,true);
    render(dir,"02_B_PaperNarrowband",drift::OrganicVariant::PaperNarrowband,.5,Source::Comparison,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current,true);
    render(dir,"03_C_PhaseDrift",drift::OrganicVariant::PhaseDrift,.5,Source::Comparison,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current,true);
    render(dir,"04_A_Wander_HighChaos",drift::OrganicVariant::Wander,1,Source::Comparison,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current,true);
    render(dir,"05_B_PaperNarrowband_HighChaos",drift::OrganicVariant::PaperNarrowband,1,Source::Comparison,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current,true);
    render(dir,"06_C_PhaseDrift_HighChaos",drift::OrganicVariant::PhaseDrift,1,Source::Comparison,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current,true);
    render(dir,"07_Coherence_0",drift::OrganicVariant::Wander,.6,Source::Broadband,0);render(dir,"08_Coherence_50",drift::OrganicVariant::Wander,.6,Source::Broadband,.5);render(dir,"09_Coherence_100",drift::OrganicVariant::Wander,.6,Source::Broadband,1);
    render(dir,"10_Bank_Gentle_Coh0",drift::OrganicVariant::Wander,.65,Source::Broadband,0);render(dir,"11_Bank_Selective_Coh0",drift::OrganicVariant::Wander,.65,Source::Broadband,0,drift::BankMode::Selective);render(dir,"12_Bank_Gentle_Coh100",drift::OrganicVariant::Wander,.65,Source::Broadband,1);render(dir,"13_Bank_Selective_Coh100",drift::OrganicVariant::Wander,.65,Source::Broadband,1,drift::BankMode::Selective);
    render(dir,"14_Dynamics_Current",drift::OrganicVariant::Wander,.45,Source::Dynamic,.45,drift::BankMode::Gentle,drift::DynamicsMode::Current);render(dir,"15_Dynamics_MotionOnly",drift::OrganicVariant::Wander,.45,Source::Dynamic,.45,drift::BankMode::Gentle,drift::DynamicsMode::MotionOnly);
    std::ofstream guide(dir/"README.txt");guide<<"DRIFT BRIGADE M1.1 deterministic bake-off (48 kHz stereo).\nNo file is independently normalized. Compare matched numbered groups; see docs/m1_1_listening_guide.md in the repository.\nSections in files 01-06: harmonic, plucked/transient, deterministic broadband. Seed: 0x4d313142.\n";
    statistics();std::cout<<"Rendered bake-off to "<<dir<<'\n';
}
