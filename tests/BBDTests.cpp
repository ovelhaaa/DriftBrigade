#include "dsp/ClockedBBDCore.h"
#include "AllocationTracker.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using drift::ClockedBBDCore;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void constantDelay(double sr, std::size_t stages, double delay) {
    ClockedBBDCore core; core.prepare(sr, stages); core.setDelaySeconds(delay);
    std::size_t enteredHost = 0, exited = 0; bool inserted=false;
    for (std::size_t n=0; n<static_cast<std::size_t>(sr*2.0) && !exited; ++n) {
        // A step avoids hiding a sub-host-sample event pulse behind the
        // temporary host-rate output hold; its leading edge is the marker.
        const double output = core.process(1.0);
        if(!inserted && core.telemetry().eventsThisHostSample) { inserted=true; enteredHost=n; }
        if (output == 1.0) exited = n;
    }
    require(inserted && exited != 0, "fixed-stage transit event count");
    const double measured = static_cast<double>(exited-enteredHost)/sr;
    require(std::abs(measured-delay) <= 1.0/sr, "N/(2*fclock) delay accuracy");
}

std::vector<double> render(std::size_t block) {
    constexpr std::size_t count=30000; std::vector<double> output(count);
    ClockedBBDCore core; core.prepare(48000, 512);
    std::size_t position=0;
    while(position<count) {
        const auto end=std::min(count,position+block);
        for(;position<end;++position) {
            core.setDelaySeconds(0.002+0.018*static_cast<double>(position)/(count-1));
            output[position]=core.process(std::sin(2.0*drift::pi*317.0*position/48000.0));
        }
    }
    return output;
}

void segmentation() {
    const auto reference=render(1);
    for(std::size_t block:{17,64,127,256,511,1024}) require(reference==render(block),"BBD block segmentation");
}

void eventCounts() {
    for(double clock:{137.25,24000.0,73123.75,1000000.0}) {
        ClockedBBDCore core; core.prepare(48000,256); core.setDelaySeconds(256.0/(2.0*clock));
        constexpr std::uint64_t samples=1000000;
        for(std::uint64_t n=0;n<samples;++n) core.process(0);
        const auto expected=static_cast<std::uint64_t>(std::floor(samples*(2.0*core.telemetry().effectiveClockHz/48000.0)));
        const auto actual=core.telemetry().totalEventCount;
        require(actual==expected || actual+1==expected || expected+1==actual,"long-run event drift");
    }
}

void variableAndStep() {
    ClockedBBDCore core; core.prepare(48000,256); core.setDelaySeconds(.01);
    bool inserted=false; std::uint64_t previous=0; double previousPhase=0; bool phaseMoved=false;
    for(int n=0;n<30000;++n) {
        const double d=.001+.029*n/29999.0; core.setDelaySeconds(d);
        const double out=core.process(inserted?0.0:.75);
        if(!inserted && core.telemetry().eventsThisHostSample) inserted=true;
        require(core.telemetry().totalEventCount>=previous,"event count reset during sweep");
        if(core.telemetry().accumulatedClockPhase!=previousPhase) phaseMoved=true;
        previous=core.telemetry().totalEventCount; previousPhase=core.telemetry().accumulatedClockPhase;
        require(std::isfinite(out) && core.finiteState(),"sweep finite state");
    }
    require(phaseMoved,"scheduler phase frozen");
    // A unique value remains in stage memory across an abrupt clock change.
    core.reset(); core.setDelaySeconds(.005); while(!core.telemetry().eventsThisHostSample) core.process(.625);
    core.setDelaySeconds(.025); bool found=false;
    for(std::size_t i=0;i<256;++i) found |= core.stageValue(i)==.625;
    require(found,"delay step cleared stage history");
}

void safetyAndAllocation() {
    ClockedBBDCore core; core.prepare(48000,4096); core.setDelaySeconds(4096.0/(2.0*1000000.0));
    const auto before=allocations.load();
    for(int n=0;n<10000;++n) {
        const double signals[]{0.0,n==0?1.0:0.0,std::sin(2*drift::pi*1000*n/48000.0),
            (n*1103515245u+12345u)/2147483648.0-1.0,std::numeric_limits<float>::max()};
        for(double x:signals) require(std::isfinite(core.process(x)) && core.finiteState(),"numerical safety");
    }
    require(allocations.load()==before,"BBD process allocation");
}
}
int main(){try{
    for(double sr:{44100.,48000.,88200.,96000.}) for(std::size_t stages:{256u,512u,1024u,2048u,4096u})
        for(double delay:{.003,.01,.03}) constantDelay(sr,stages,delay);
    segmentation(); eventCounts(); variableAndStep(); safetyAndAllocation();
    std::cout<<"PASS: M2.0 fixed-stage BBD transport; all rates/stages, exact transit events, block invariant\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
