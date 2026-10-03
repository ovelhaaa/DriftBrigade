#include "dsp/ClockedBBDCore.h"
#include "AllocationTracker.h"
#include "../tools/BBDMeasurements.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using drift::ClockedBBDCore;
void require(bool v,const char* m) { if(!v) throw std::runtime_error(m); }
double maxDelayError=0,maxHoldError=0,maxFilterError=0,maxAsyncError=0,maxPaperMagnitudeError=0,maxPaperPhaseError=0;
void phases() {
    ClockedBBDCore c; c.prepare(48000,8); c.setDelaySeconds(8.0/48000); // one edge per host interval
    for(int n=0;n<24;++n) {
        const double before=c.heldOutput(); c.process(n+1.0); const auto& t=c.telemetry();
        require(t.eventsThisHostSample==1,"one physical edge");
        require(t.capturesThisHostSample==static_cast<unsigned>(n%2==0),"capture every second edge");
        require(t.outputsThisHostSample==static_cast<unsigned>(n%2==1),"opposite output phase");
        if(n%2==0) require(c.heldOutput()==before,"capture must not change hold");
        if(n>=7 && n%2==1) require(c.heldOutput()==n-6.0,"N-1 edge historical transit");
    }
    require(c.telemetry().totalCaptureCount==12 && c.telemetry().totalOutputCount==12,"signal rate fBBD");
    bool rejected=false; try { c.prepare(48000,257); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"odd physical stages rejected");
}
void constantDelay(double sr,std::size_t stages,double delay) {
    ClockedBBDCore c; c.prepare(sr,stages); c.setDelaySeconds(delay);
    double captured=0,observed=0,event=0;
    for(int n=0;n<static_cast<int>(sr*2);++n) {
        const double y=c.process(1.0);
        if(c.telemetry().totalCaptureCount && !captured) captured=c.telemetry().lastCaptureTimeSeconds;
        if(y==1.0) { observed=(n+1)/sr; event=c.telemetry().lastOutputTimeSeconds; break; }
    }
    require(observed>0,"marker exited");
    const double halfPeriod=1.0/(2*c.telemetry().effectiveClockHz);
    if(2*c.telemetry().effectiveClockHz<=sr)
        require(std::abs((event-captured)-(stages-1)*halfPeriod)<1e-10,"paper Eq.1 pulse onset");
    // From reset, first capture is at half a period and first output at D.
    // At faster-than-host clocks the snapshot may include later edges too.
    const double error=std::abs(observed-delay);
    maxDelayError=std::max(maxDelayError,error);
    require(error<=1.0/sr+1e-10,"nominal delay within host observation error");
}
void knownAndRates() {
    ClockedBBDCore c;
    for(double rate:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),0.,-48000.,1.,1e20}) {
        c.prepare(rate,1024); require(c.telemetry().hostRateWasNormalized,"invalid host rate normalized");
        c.setDelaySeconds(.01); c.process(1); require(c.finiteState(),"invalid host rate finite clock/state");
    }
    for(double rate:{8000.,384000.}) { c.prepare(rate,1024); require(!c.telemetry().hostRateWasNormalized && c.finiteState(),"inclusive rate extremes"); }
    c.prepare(48000,1024); c.setDelaySeconds(.01);
    require(c.telemetry().effectiveClockHz==51200 && c.telemetry().signalNyquistHz==25600 && c.telemetry().logicalSignalBuckets==512,"1024/10ms example");
    c.prepare(48000,4096); c.setDelaySeconds(.3);
    require(std::abs(c.telemetry().effectiveClockHz-6826.666666666667)<1e-9 && std::abs(c.telemetry().signalNyquistHz-3413.333333333333)<1e-9,"4096/300ms example");
    for(double d:{-1.,0.,1e20,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        c.setDelaySeconds(d); require(c.telemetry().wasClamped && c.finiteState(),"invalid delay safe clamp");
    }
}
std::vector<double> render(std::size_t block,drift::BBDMode mode) {
    std::vector<double> out(10000); ClockedBBDCore c; c.setQualificationMode(mode); c.prepare(48000,512);
    for(std::size_t start=0;start<out.size();start+=block)
        for(std::size_t n=start;n<std::min(out.size(),start+block);++n) {
            c.setDelaySeconds(.002+.018*n/(out.size()-1)); out[n]=c.process(std::sin(2*drift::pi*317*n/48000));
        }
    return out;
}
void segmentation() {
    for(auto mode:{drift::BBDMode::TransportOnly,drift::BBDMode::AsyncLinearReference}) {
        const auto selected=render(1,mode);
        for(std::size_t b:{17,64,127,256,511,1024}) require(selected==render(b,mode),"bit-identical blocks");
    }
}
void filterReference() {
    drift::AsyncAnalogFilter f; f.prototype(2000); const double a=2*drift::pi*2000;
    f.injectImpulse(1/a); double t=0;
    for(double dt:{.000001,.000017,.000003,.000127,.00004,.001}) {
        t+=dt; maxFilterError=std::max(maxFilterError,std::abs(f.advance(dt)-std::exp(-a*t)));
    }
    require(maxFilterError<1e-12,"arbitrary-instant impulse reference");
    f.reset(); t=0;
    for(double dt:{.000001,.000019,.00027,.000033}) {
        t+=dt; require(std::abs(f.advance(dt,1)-(1-std::exp(-a*t)))<1e-12,"analytical CT step");
    }
    // Steady-state complex impulse train: exact geometric series, including
    // magnitude and phase at arbitrary offsets between host impulses.
    for(double hz:{100.,500.,2000.,7000.}) {
        f.reset(); constexpr double sr=48000; const double w=2*drift::pi*hz;
        const std::complex<double> expectedGain=(a/sr)/(1.0-std::exp(std::complex<double>(-a/sr,-w/sr)));
        for(int n=0;n<12000;++n) {
            f.injectImpulse(std::cos(w*n/sr)/sr);
            for(double fraction:{.13,.29,.58}) f.advance(fraction/sr);
            if(n>10000) {
                const double expected=(expectedGain*std::polar(std::exp(-a/sr),w*n/sr)).real();
                maxFilterError=std::max(maxFilterError,std::abs(f.value()-expected));
            }
        }
    }
    require(maxFilterError<1e-10,"sine magnitude/phase analytical impulse train");
    // Independent real-valued Table 1 impulse kernels. Verifies signs and
    // pairing of the complex residues, not just self-consistent response().
    for(bool output:{false,true}) {
        f.paperReference(output); f.reset(); f.injectImpulse(1.0); t=0;
        for(double dt:{.000001,.000017,.000003,.000127,.00004,.001}) {
            t+=dt;
            const double expected=output
                ?5092*std::exp(-176261*t)+2*std::exp(-51468*t)*(11256*std::cos(21437*t)+99566*std::sin(21437*t))
                    +2*std::exp(-26276*t)*(-13802*std::cos(59699*t)-24606*std::sin(59699*t))
                :251589*std::exp(-46580*t)+2*std::exp(-55482*t)*(-130428*std::cos(25082*t)+4165*std::sin(25082*t))
                    +2*std::exp(-26292*t)*(4634*std::cos(59437*t)-22873*std::sin(59437*t));
            require(std::abs(f.advance(dt)-expected)<1e-8,"Table 1 independent impulse kernel");
        }
    }
}
// Independent direct convolution with analytical impulse/step kernels.
// No reuse of the DSP filter recurrence, ring storage or phase scheduler.
void asynchronousReference(bool changing) {
    constexpr double sr=48000,a=2*drift::pi*2000;
    ClockedBBDCore c; c.setQualificationMode(drift::BBDMode::AsyncLinearReference); c.prepare(sr,8);
    std::vector<double> inputs,captures(3,0.0),stepTimes,steps;
    double nextEdge=1.0/8000,held=0,phase=0; int edge=0;
    for(int n=0;n<2000;++n) {
        const double clock=changing && n>=500?2700.:4000.; c.setDelaySeconds(8/(2*clock));
        const double input=std::sin(2*drift::pi*713*n/sr); inputs.push_back(input);
        const double begin=n/sr,end=(n+1)/sr,inc=2*clock/sr;
        const int events=static_cast<int>(phase+inc);
        for(int e=0;e<events;++e) {
            nextEdge=begin+(1-phase+e)/(2*clock);
            if(edge%2==0) {
                double v=0; for(int k=0;k<=n;++k) v+=inputs[k]*a/sr*std::exp(-a*(nextEdge-k/sr));
                captures.push_back(v);
            } else {
                const double v=captures[static_cast<std::size_t>(edge/2)];
                stepTimes.push_back(nextEdge); steps.push_back(v-held); held=v;
            }
            ++edge;
        }
        phase+=inc-events;
        double expected=0;
        for(std::size_t j=0;j<steps.size();++j) expected+=steps[j]*(1-std::exp(-a*(end-stepTimes[j])));
        const double actual=c.process(input); maxAsyncError=std::max(maxAsyncError,std::abs(actual-expected));
    }
    require(maxAsyncError<1e-10,"async shell direct convolution fixed/changing clock");
}
void history() {
    ClockedBBDCore c; c.prepare(48000,256); c.setDelaySeconds(.01);
    while(!c.telemetry().totalCaptureCount) c.process(.625);
    const auto entered=c.telemetry(); c.setDelaySeconds(.025);
    require(c.telemetry().accumulatedClockPhase==entered.accumulatedClockPhase,"clock step phase retained");
    require(c.stageValue(0)==.625,"clock step memory retained");
    double exited=0;
    for(int n=0;n<2000;++n) { c.process(0); if(c.heldOutput()==.625) { exited=c.telemetry().lastOutputTimeSeconds; break; } }
    // Phase at host end accounts for already elapsed fractional edge before step.
    const double hostEnd=std::ceil(entered.lastCaptureTimeSeconds*48000-1e-9)/48000;
    require(std::abs(exited-(hostEnd+(255-entered.accumulatedClockPhase)/(2*c.telemetry().effectiveClockHz)))<1e-10,"later trajectory marker transit");
    c.setQualificationMode(drift::BBDMode::AsyncLinearReference); c.reset();
    for(int n=0;n<1000;++n)c.process(.3);
    const auto phase=c.telemetry().accumulatedClockPhase; const double in=c.inputFilterValue(),out=c.outputFilterValue(),hold=c.heldOutput();
    c.setDelaySeconds(.01);
    require(in==c.inputFilterValue() && out==c.outputFilterValue() && hold==c.heldOutput() && phase==c.telemetry().accumulatedClockPhase,"all clock-step state retained");
}
void spectral() {
    for(double ratio:{.1,.25,.45,.55,.75}) {
        const auto raw=qualification::measure(ratio,drift::BBDMode::TransportOnly);
        const double base=std::min(ratio,1-ratio);
        const double expected=qualification::sinc(base);
        maxHoldError=std::max(maxHoldError,std::abs(raw.alias-expected));
        require(std::abs(raw.alias-expected)<.002,"ZOH sinc envelope, 0.002 absolute tolerance");
        const auto filtered=qualification::measure(ratio,drift::BBDMode::AsyncLinearReference);
        require(filtered.alias<raw.alias && filtered.imageUpper<raw.imageUpper,"measurable alias/image filter attenuation");
        if(ratio>.5) require(raw.alias>.2,"alias origin at fBBD Nyquist");
        const auto paper=qualification::measure(ratio,drift::BBDMode::AsyncLinearReference,drift::BBDFilterProfile::HoltersParkerTable1);
        require(paper.alias<raw.alias && paper.imageUpper<raw.imageUpper,"paper profile alias/image attenuation");
        if(ratio<.5) {
            drift::AsyncAnalogFilter in,out; in.paperReference(false); out.paperReference(true);
            const double hz=ratio*qualification::clockHz;
            const auto gain=in.response(hz)*out.response(hz);
            const double magnitude=qualification::sinc(ratio)*std::abs(gain);
            const double phase=-2*drift::pi*hz*256/(2*qualification::clockHz)+std::arg(gain);
            maxPaperMagnitudeError=std::max(maxPaperMagnitudeError,std::abs(paper.desired-magnitude));
            maxPaperPhaseError=std::max(maxPaperPhaseError,std::abs(std::remainder(paper.phase-phase,2*drift::pi)));
            require(maxPaperMagnitudeError<2e-5 && maxPaperPhaseError<5e-5,"paper analog sine magnitude/phase reference");
        }
    }
}
void safety() {
    ClockedBBDCore transport; transport.prepare(48000,4096); transport.setDelaySeconds(1e-6);
    const auto transportBefore=allocations.load();
    for(int n=0;n<10000;++n) require(std::isfinite(transport.process(n%2?std::numeric_limits<double>::quiet_NaN():std::numeric_limits<float>::max())) && transport.finiteState(),"transport input safety");
    require(allocations.load()==transportBefore,"zero transport process allocations");
    for(auto profile:{drift::BBDFilterProfile::ValidationPrototype,drift::BBDFilterProfile::HoltersParkerTable1}) {
        ClockedBBDCore c; c.setQualificationMode(drift::BBDMode::AsyncLinearReference,profile); c.prepare(48000,4096); c.setDelaySeconds(1e-6);
        const auto before=allocations.load();
        for(int n=0;n<3000;++n) require(std::isfinite(c.process(n%2?std::numeric_limits<float>::max():-std::numeric_limits<float>::max())) && c.finiteState(),"finite maximum input/rate");
        require(allocations.load()==before,"zero process allocations");
    }
    for(double clock:{137.25,24000.,73123.75,1000000.}) {
        ClockedBBDCore c; c.prepare(48000,256); c.setDelaySeconds(256/(2*clock));
        for(int n=0;n<1000000;++n)c.process(0);
        const double expected=1000000*2*c.telemetry().effectiveClockHz/48000;
        require(std::abs(c.telemetry().totalEventCount-expected)<=1,"long-run edge drift");
    }
}
}
int main(int argc,char** argv) { try {
    const std::string selection=argc>1?argv[1]:"all";
    if(selection=="all" || selection=="transport") {
        phases(); knownAndRates();
        for(double sr:{44100.,48000.,88200.,96000.}) for(std::size_t stages:{256u,512u,1024u,2048u,4096u})
            for(double delay:{.003,.01,.03})constantDelay(sr,stages,delay);
        constantDelay(48000,4096,.3); segmentation(); history(); safety();
    }
    if(selection=="all" || selection=="async") {
        filterReference(); asynchronousReference(false); asynchronousReference(true);
    }
    if(selection=="all" || selection=="spectral") spectral();
    require(selection=="all" || selection=="transport" || selection=="async" || selection=="spectral","unknown test selection");
    std::cout<<"PASS M2.1: block invariant, finite, zero allocations\nmax nominal delay observation error s="<<maxDelayError
             <<"\nmax hold magnitude error="<<maxHoldError<<"\nmax filter analytical error="<<maxFilterError<<"\nmax async convolution error="<<maxAsyncError
             <<"\nmax paper magnitude error="<<maxPaperMagnitudeError<<"\nmax paper phase error rad="<<maxPaperPhaseError<<'\n'; return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; } }
