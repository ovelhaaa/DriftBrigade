#include "dsp/DriftEngine.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>
#include "AllocationTracker.h"

namespace {
using namespace drift;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Correlation {
    double x=0,y=0,xx=0,yy=0,xy=0,n=0;
    void add(double a, double b) { x+=a; y+=b; xx+=a*a; yy+=b*b; xy+=a*b; ++n; }
    double value() const { return (xy-x*y/n)/std::sqrt((xx-x*x/n)*(yy-y*y/n)); }
};
void reconstruction(double sr) {
    for(auto mode:{BankMode::Gentle,BankMode::Selective}) {
      MultiscaleBank bank; bank.setMode(mode); bank.prepare(sr); Random rng; rng.reset(14);
      double squared=0,peak=0;
      for (int i=0; i<100000; ++i) {
          const double x=i==0 ? 1.0 : 2*rng.next()-1;
          const auto bands=bank.process(x); const double error=bands[0]+bands[1]+bands[2]+bands[3]-x;
          squared+=error*error; peak=std::max(peak,std::abs(error));
      }
      const double rms=std::sqrt(squared/100000);
      require(peak<1e-14 && rms<1e-15,"filterbank reconstruction");
      std::cout << (mode==BankMode::Gentle?"gentle":"selective") << " reconstruction " << sr << " Hz: RMS=" << rms << " peak=" << peak << '\n';
    }
}
void variantQualification() {
    constexpr int samples=960000;
    std::array<std::vector<double>,3> captures;
    int index=0;
    for(auto variant:{OrganicVariant::Wander,OrganicVariant::PaperNarrowband,OrganicVariant::PhaseDrift}) {
        OrganicModulator a,b,different;a.prepare(48000);b.prepare(48000);different.prepare(48000);
        a.setVariant(variant);b.setVariant(variant);different.setVariant(variant);a.reset(77);b.reset(77);different.reset(78);
        captures[index].reserve(48000);double seedDifference=0,sum=0,sq=0,peak=0,maxStep=0;
        double previous=a.process(2,.65);require(previous==b.process(2,.65),"variant seed determinism");different.process(2,.65);
        for(int i=1;i<samples;++i) {
            const double chaos=.65;
            const double rate=i<samples/2?2.:6.; const double x=a.process(rate,chaos);
            require(x==b.process(rate,chaos),"variant seed determinism");seedDifference+=std::abs(x-different.process(rate,chaos));
            require(std::abs(x)<=OrganicModulator::maximumMagnitude(variant)+1e-12,"variant analytical bound");
            maxStep=std::max(maxStep,std::abs(x-previous));previous=x;
            if(i>48000){sum+=x;sq+=x*x;} if(i<48000)captures[index].push_back(x);peak=std::max(peak,std::abs(x));
        }
        require(seedDifference>10,"variant seed differentiation");require(maxStep<.003,"variant parameter continuity");
        const double count=samples-48000,mean=sum/count,rms=std::sqrt(sq/count-mean*mean);
        require(rms>.5&&rms<1.0,"variant RMS sanity");
        std::cout<<"variant "<<index<<" RMS="<<rms<<" peak="<<peak<<" max step="<<maxStep<<'\n';++index;

        OrganicModulator automated;automated.prepare(48000);automated.setVariant(variant);automated.reset(77);
        double autoPrevious=automated.process(),autoStep=0;
        for(int i=1;i<240000;++i){if(i==40000)automated.setRate(10);if(i==90000)automated.setChaos(1);if(i==150000){automated.setRate(.05);automated.setChaos(.1);}const double x=automated.process();autoStep=std::max(autoStep,std::abs(x-autoPrevious));autoPrevious=x;}
        require(autoStep<.006,"variant smoothed Motion/Chaos continuity");
    }
    // All strategies use precisely the original sine when Chaos is zero.
    OrganicModulator zeroA,zeroB,zeroC;zeroA.prepare(48000);zeroB.prepare(48000);zeroC.prepare(48000);
    zeroA.setVariant(OrganicVariant::Wander);zeroB.setVariant(OrganicVariant::PaperNarrowband);zeroC.setVariant(OrganicVariant::PhaseDrift);
    zeroA.reset(91);zeroB.reset(91);zeroC.reset(91);
    for(int n=0;n<100000;++n){const double x=zeroA.process(.7,0);require(x==zeroB.process(.7,0)&&x==zeroC.process(.7,0),"Chaos-zero baseline mismatch");}

    double commonExcursion=-1;
    for(auto variant:{OrganicVariant::Wander,OrganicVariant::PaperNarrowband,OrganicVariant::PhaseDrift}) {
        DriftEngine engine;EngineParameters p;p[Motion]=.7;p[Depth]=.5;p[Center]=15;
        engine.setParameters(p);engine.setOrganicVariant(variant);engine.prepare(48000,91);engine.processSample(0,0);
        const auto& trace=engine.telemetry();
        require(trace.requestedExcursionSeconds==trace.actualExcursionSeconds,"bake-off excursion safety clamp");
        if(commonExcursion<0)commonExcursion=trace.actualExcursionSeconds;
        require(trace.actualExcursionSeconds==commonExcursion,"variant excursion mismatch");
    }
}
void modulation() {
    // Endpoint and adjacent segment derivatives, not merely small audio sample steps.
    constexpr double epsilon=1e-6;
    require(OrganicModulator::raisedCosine(0)==0 && OrganicModulator::raisedCosine(1)==1,"cosine endpoints");
    const double before=(1-OrganicModulator::raisedCosine(1-epsilon))/epsilon;
    const double after=OrganicModulator::raisedCosine(epsilon)/epsilon;
    require(std::abs(before)<3e-6 && std::abs(after)<3e-6,"cosine endpoint slope");
    OrganicModulator a,b,c; a.prepare(48000); b.prepare(48000); c.prepare(48000);
    a.reset(42); b.reset(42); c.reset(43);
    double difference=0,previous=0,maxStep=0,previousRandom=0,boundaryStep=0;
    for (int i=0; i<240000; ++i) {
        if (i==40000) { a.setRate(10); b.setRate(10); c.setRate(10); }
        if (i==90000) { a.setChaos(1); b.setChaos(1); c.setChaos(1); }
        if (i==140000) { a.setRate(0.05); b.setRate(0.05); c.setRate(0.05); }
        const auto phase=a.segmentPhase(); const double x=a.process();
        require(x==b.process(),"modulator seed determinism"); difference+=std::abs(x-c.process());
        require(std::abs(x)<=OrganicModulator::wanderMaximumMagnitude+1e-12,"morph analytic bound");
        if (i>0) maxStep=std::max(maxStep,std::abs(x-previous));
        if (a.segmentPhase()<phase) boundaryStep=std::max(boundaryStep,std::abs(a.randomValue()-previousRandom));
        previous=x; previousRandom=a.randomValue();
    }
    require(difference>100,"different seeds"); require(maxStep<0.005,"rate/chaos automation continuity");
    require(boundaryStep<5e-6,"random boundary value continuity");
    std::cout << "modulation max step=" << maxStep << " random boundary step=" << boundaryStep << '\n';
    // Expected RMS remains stable across Chaos over long, fixed-rate captures.
    double minimum=10,maximum=0;
    for (double chaos : {0.,0.25,0.5,0.75,1.}) {
        OrganicModulator m; m.prepare(48000); m.reset(912); double sum=0,sq=0;
        for (int i=0; i<960000; ++i) { const double x=m.process(9,chaos); sum+=x; sq+=x*x; }
        const double rms=std::sqrt(sq/960000-(sum/960000)*(sum/960000));
        minimum=std::min(minimum,rms); maximum=std::max(maximum,rms);
        std::cout << "Chaos=" << chaos << " centered RMS=" << rms << '\n';
    }
    require(maximum/minimum<1.18,"Chaos depth consistency");
}
void coherence() {
    for (double coherenceValue : {0.,0.5,1.}) {
        DriftEngine engine; EngineParameters p; p[Motion]=8; p[Chaos]=1; p[Coherence]=coherenceValue; p[Dynamics]=0; p[Width]=0;
        engine.setParameters(p); engine.prepare(48000,123);
        Correlation stats;
        for (int i=0; i<960000; ++i) { engine.processSample(0,0); const auto& m=engine.telemetry().modulation[0]; stats.add(m[0],m[1]); }
        const double r=stats.value();
        std::cout << "Coherence=" << coherenceValue << " correlation=" << r << '\n';
        require(coherenceValue<1 || r>0.999999,"shared modulation correlation");
        require(coherenceValue>0 || std::abs(r)<0.3,"independent modulation correlation");
        require(coherenceValue!=0.5 || (r>0.05 && r<0.55),"partial correlation");
    }
}
void delayInterpolation(double sr) {
    DigitalFractionalDelay delay; delay.prepare(sr);
    double maxError=0;
    for (int i=0; i<20000; ++i) {
        // Affine content must interpolate exactly, including repeated ring wraps.
        const double delaySamples=3.1+0.31*(i%77);
        const double x=0.001*i;
        const double y=delay.process(x,delaySamples/sr);
        if (i>100) maxError=std::max(maxError,std::abs(y-0.001*(i-delaySamples)));
    }
    require(maxError<1e-11,"Hermite polynomial accuracy or ring boundary");
    delay.reset();
    for (int i=0; i<10000; ++i) require(std::isfinite(delay.process(1, i%2 ? 1000 : -1)),"delay clamp");
}
void boundsAndSafety(double sr) {
    DriftEngine e; EngineParameters p; p[Mix]=1; p[Feedback]=0.65; p[Dynamics]=1;
    e.setParameters(p); e.prepare(sr,82);
    double peak=0,minDelay=1,maxDelay=0;
    for (int mask=0; mask<512; ++mask) {
        for (std::size_t j=0; j<Count; ++j) p.values[j]=(mask&(1<<j)) ? parameterSpecs[j].maximum : parameterSpecs[j].minimum;
        e.setParameters(p);
        // Reset reaches parameter endpoints immediately; both automation and bounds are covered elsewhere.
        e.reset(82);
        for (int i=0; i<512; ++i) {
            const auto out=e.processSample(i%2 ? -1 : 1,1);
            for (double x : out) { require(std::isfinite(x),"finite extreme output"); peak=std::max(peak,std::abs(x)); }
            for (const auto& ch : e.telemetry().delaySeconds) for (double d : ch) {
                require(d*sr>3 && d*sr<static_cast<double>(e.delayCapacity()-4),"delay safety margin");
                minDelay=std::min(minDelay,d); maxDelay=std::max(maxDelay,d);
            }
        }
    }
    p[Mix]=1; p[Feedback]=0.65; p[Dynamics]=1; p[Depth]=1; p[Chaos]=1; p[Motion]=10; p[Width]=1;
    for (double center : {0.3,30.}) for (int stimulus=0; stimulus<3; ++stimulus) {
        p[Center]=center; e.setParameters(p); e.reset(81); double tailPeak=0;
        for (int i=0; i<static_cast<int>(sr*4); ++i) {
            const double x=stimulus==0 ? 0 : stimulus==1 ? (i==0 ? 1 : 0) : (i<sr ? std::sin(2*pi*440*i/sr) : 0);
            const auto out=e.processSample(x,x);
            for (double y : out) { require(std::isfinite(y) && std::abs(y)<160,"feedback growth"); peak=std::max(peak,std::abs(y)); if(i>sr*3) tailPeak=std::max(tailPeak,std::abs(y)); }
        }
        require(tailPeak<0.005,"feedback tail does not decay");
    }
    std::cout << "safety " << sr << " Hz: peak=" << peak << " delay seconds=" << minDelay << ".." << maxDelay << '\n';
    // Full finite float range, not just nominal full scale.
    p[Mix]=0.5; e.setParameters(p); e.reset();
    for (int i=0; i<100; ++i) { const auto out=e.processSample(std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()); for(double y:out) require(std::isfinite(static_cast<float>(y)),"float overflow"); }
}
std::vector<float> render(double sr, std::size_t block, std::uint32_t seed, bool dry=false, bool mono=false, OrganicVariant variant=OrganicVariant::Wander) {
    constexpr std::size_t count=12000;
    std::vector<float> left(count),right(count);
    for(std::size_t i=0;i<count;++i) { left[i]=static_cast<float>(0.4*std::sin(2*pi*220*i/sr)); right[i]=static_cast<float>(0.3*std::sin(2*pi*333*i/sr)); }
    DriftEngine e; EngineParameters p; p[Mix]=dry ? 0 : 0.7; e.setParameters(p); e.setOrganicVariant(variant); e.prepare(sr,seed);
    std::size_t pos=0;
    while(pos<count) {
        if(pos==4096) { p[Motion]=10; p[Center]=0.3; p[Chaos]=1; p[Coherence]=0; p[Depth]=1; e.setParameters(p); }
        const std::size_t until=pos<4096 ? 4096 : count;
        const auto n=std::min(block,until-pos);
        float* channels[]{left.data()+pos,right.data()+pos};
        const auto before=allocations.load(); e.process(channels,mono ? 1 : 2,n);
        require(allocations.load()==before,"audio-thread allocation"); pos+=n;
    }
    if(!mono) left.insert(left.end(),right.begin(),right.end());
    return left;
}
void segmentation(double sr) {
    const auto reference=render(sr,1,501);
    for(std::size_t block : {17,64,127,256,511,1024,16384}) require(reference==render(sr,block,501),"block segmentation changes output");
    require(reference==render(sr,127,501),"engine determinism"); require(reference!=render(sr,127,502),"engine seed variation");
    const auto dry=render(sr,511,501,true);
    for(std::size_t i=0;i<12000;++i) {
        require(dry[i]==static_cast<float>(0.4*std::sin(2*pi*220*i/sr)),"dry left invariant");
        require(dry[i+12000]==static_cast<float>(0.3*std::sin(2*pi*333*i/sr)),"dry right invariant");
    }
    const auto mono=render(sr,17,501,false,true); require(mono==render(sr,1024,501,false,true),"mono segmentation");
    for(auto variant:{OrganicVariant::PaperNarrowband,OrganicVariant::PhaseDrift})
        require(render(sr,1,501,false,false,variant)==render(sr,511,501,false,false,variant),"variant block segmentation");
    std::cout << "block sizes 1/17/64/127/256/511/1024/16384 bit identical at " << sr << " Hz; no process allocations\n";
}
void stereoAndDynamics() {
    DriftEngine e; EngineParameters p; p[Width]=0; p[Dynamics]=0; e.setParameters(p); e.prepare(48000);
    for(int i=0;i<10000;++i) { const auto out=e.processSample(std::sin(2*pi*440*i/48000),std::sin(2*pi*440*i/48000)); require(out[0]==out[1],"width zero equality"); }
    p[Width]=1; p[Dynamics]=1; p[Chaos]=0.2; e.setParameters(p); e.reset();
    double stereoDifference=0; const double quiet=e.telemetry().envelope;
    for(int i=0;i<48000;++i) { const auto out=e.processSample(0.8*std::sin(2*pi*440*i/48000),0.8*std::sin(2*pi*440*i/48000)); stereoDifference+=std::abs(out[0]-out[1]); }
    require(stereoDifference>100,"stereo decorrelation");
    const auto loud=e.telemetry(); require(loud.envelope>quiet+0.5 && loud.effectiveChaos>p[Chaos]+0.1 && loud.effectiveFeedback>p[Feedback]+0.05,"dynamic response");
    require(loud.effectiveFeedback<=0.75,"dynamic feedback clamp");
    double maxWetStep=0,prev=loud.wetProminence;
    for(int i=0;i<240000;++i) { e.processSample(0,0); maxWetStep=std::max(maxWetStep,std::abs(e.telemetry().wetProminence-prev)); prev=e.telemetry().wetProminence; }
    require(e.telemetry().wetProminence<loud.wetProminence && maxWetStep<0.001,"dynamics smoothing");
    p[Dynamics]=0; e.setParameters(p); e.reset(); e.processSample(0,0);
    require(e.telemetry().effectiveChaos==p[Chaos] && e.telemetry().effectiveFeedback==p[Feedback] && e.telemetry().wetProminence==1,"dynamics zero invariant");
    p[Mix]=1; p[Dynamics]=0; p[Feedback]=0; e.setParameters(p); e.reset();
    double mean=0;
    for(int i=0;i<144000;++i) { const auto out=e.processSample(1,1); if(i>=96000) mean+=out[0]/48000; }
    require(std::abs(mean)<1e-8,"wet DC accumulation");
    std::cout << "dynamics loud envelope=" << loud.envelope << " Chaos=" << loud.effectiveChaos << " feedback=" << loud.effectiveFeedback << " max wet step=" << maxWetStep << '\n';
}
void lockedFullbandEquivalence() {
    DriftEngine e; EngineParameters p; p[Width]=0; p[Dynamics]=0; p[Mix]=1; p[Coherence]=1; p[Feedback]=0;
    e.setParameters(p); e.prepare(48000,52);
    DelayPath fullband; fullband.prepare(48000); Random rng; rng.reset(144);
    double peakError=0;
    for(int i=0;i<96000;++i) {
        const double input=0.3*(2*rng.next()-1); const auto out=e.processSample(input,input);
        const double reference=fullband.process(input,e.telemetry().delaySeconds[0][0],0);
        peakError=std::max(peakError,std::abs(out[0]-reference));
    }
    require(peakError<1e-12,"coherent bands do not recover fullband modulated delay");
    std::cout << "locked multiscale/fullband equivalence peak error=" << peakError << '\n';
}
void mappingAndAutomation() {
    for(double rate : {0.05,0.5,2.,3.,4.,6.,9.,10.}) {
        double previous=-1;
        for(int i=0;i<=1000;++i) {
            const double d=depthSeconds(rate,i/1000.);
            require(d>=previous && std::isfinite(d) && d<=0.005,"depth mapping monotonic/safe"); previous=d;
        }
        require(depthSeconds(rate,0)==0,"zero depth");
    }
    for(double boundary : {4.,9.})
        require(std::abs(depthSeconds(boundary-1e-8,0.5)-depthSeconds(boundary+1e-8,0.5))<1e-10,"rate mapping boundary jump");
    DriftEngine e; EngineParameters p; p[Mix]=0; e.setParameters(p); e.prepare(48000);
    float maximum=std::numeric_limits<float>::max(); float* channels[]{&maximum};
    e.process(channels,1,1); require(maximum==std::numeric_limits<float>::max(),"full float dry invariant");
    p[Mix]=1; e.setParameters(p);
    std::array<std::array<double,4>,2> previousDelay=e.telemetry().delaySeconds;
    for(int i=0;i<96000;++i) {
        if(i==10000) { p[Center]=30; p[Motion]=10; p[Depth]=1; p[Coherence]=0; e.setParameters(p); }
        if(i==50000) { p[Center]=0.3; p[Motion]=0.05; p[Chaos]=1; p[Width]=1; e.setParameters(p); }
        e.processSample(0.2,0.2);
        const auto& current=e.telemetry().delaySeconds;
        for(std::size_t ch=0;ch<2;++ch) for(std::size_t b=0;b<4;++b)
            require(std::abs(current[ch][b]-previousDelay[ch][b])<=0.250001/48000,"delay pointer automation jump");
        previousDelay=current;
    }
    std::cout << "depth mapping monotonic/continuous; Center automation slew <= 0.25 sample per sample\n";
}
}
int main() {
    try {
        const auto start=std::chrono::steady_clock::now();
        modulation(); variantQualification(); coherence(); stereoAndDynamics(); lockedFullbandEquivalence(); mappingAndAutomation();
        for(double sr : {44100.,48000.,88200.,96000.}) { reconstruction(sr); delayInterpolation(sr); boundsAndSafety(sr); segmentation(sr); }
        std::cout << "PASS: M1 DSP qualification (" << std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count() << " s)\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
