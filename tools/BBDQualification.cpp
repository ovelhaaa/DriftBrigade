#include "dsp/ClockedBBDCore.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

using drift::ClockedBBDCore;
int main(int argc,char** argv) {
    const std::filesystem::path directory=argc>1?argv[1]:"bbd_qualification";
    std::error_code error;
    std::filesystem::create_directories(directory,error);
    if(error || !std::filesystem::is_directory(directory,error) || error) {
        std::cerr<<"Cannot create output directory: "<<(error?error.message():"path is not a directory")<<'\n';
        return 1;
    }
    std::ofstream constant(directory/"bbd_constant_clock.csv"), sweep(directory/"bbd_delay_sweep.csv"),
                  timing(directory/"bbd_event_timing.csv"), matrix(directory/"bbd_stage_matrix.csv"), readme(directory/"README.txt");
    if(!constant || !sweep || !timing || !matrix || !readme) {
        std::cerr<<"Cannot open all BBD qualification artifacts\n";
        return 1;
    }
    constant<<"sample_rate,stages,requested_delay_s,effective_delay_s,clock_hz,event_rate_hz,event_error,seconds,events_per_second,relative_cpu\n";
    double baseline=0;
    for(std::size_t stages:{256u,512u,1024u,2048u,4096u}) {
        ClockedBBDCore left,right;left.prepare(48000,stages);right.prepare(48000,stages);
        left.setDelaySeconds(.01);right.setDelaySeconds(.01);
        const auto start=std::chrono::steady_clock::now();
        constexpr int count=480000;
        for(int n=0;n<count;++n) { const double x=std::sin(2*drift::pi*440*n/48000.0);left.process(x);right.process(x); }
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if(!baseline)baseline=seconds;
        const double expected=count*2*left.telemetry().effectiveClockHz/48000.0;
        const auto stereoEvents=left.telemetry().totalEventCount+right.telemetry().totalEventCount;
        constant<<48000<<','<<stages<<','<<left.telemetry().requestedDelaySeconds<<','<<left.telemetry().effectiveDelaySeconds<<','<<left.telemetry().effectiveClockHz<<','<<2*left.telemetry().effectiveClockHz<<','<<(static_cast<double>(left.telemetry().totalEventCount)-expected)<<','<<seconds<<','<<stereoEvents/seconds<<','<<seconds/baseline<<'\n';
    }
    sweep<<"time_s,requested_delay_s,instantaneous_nominal_delay_s,clock_hz,event_count,output\n";
    timing<<"host_sample,time_s,phase,events_this_sample,total_events\n";
    ClockedBBDCore core;core.prepare(48000,512);
    for(int n=0;n<96000;++n){const double d=.003+.027*n/95999.0;core.setDelaySeconds(d);const double y=core.process(.5*std::sin(2*drift::pi*440*n/48000.0));
        if(n%48==0)sweep<<n/48000.0<<','<<core.telemetry().requestedDelaySeconds<<','<<core.telemetry().effectiveDelaySeconds<<','<<core.telemetry().effectiveClockHz<<','<<core.telemetry().totalEventCount<<','<<y<<'\n';
        if(n<4096)timing<<n<<','<<n/48000.0<<','<<core.telemetry().accumulatedClockPhase<<','<<core.telemetry().eventsThisHostSample<<','<<core.telemetry().totalEventCount<<'\n';}
    matrix<<"logical_stage,value\n";for(std::size_t i=0;i<512;++i)matrix<<i<<','<<core.stageValue(i)<<'\n';
    readme<<"DriftBrigade M2.0 objective BBD qualification\n\nThe CSV files contain constant-clock CPU/event metrics, a variable-clock sine experiment, per-host-sample scheduler timing, and a final fixed-stage memory snapshot. In bbd_delay_sweep.csv, instantaneous_nominal_delay_s is derived from the current clock; it is not a measurement of historical transit time during the sweep. Output reconstruction is temporary zero-order hold; these artifacts make no subjective sound-quality claim.\n";
    auto finish=[](std::ofstream& stream) {
        stream.flush();
        const bool written=static_cast<bool>(stream);
        stream.close();
        return written && !stream.fail();
    };
    const bool constantComplete=finish(constant);
    const bool sweepComplete=finish(sweep);
    const bool timingComplete=finish(timing);
    const bool matrixComplete=finish(matrix);
    const bool readmeComplete=finish(readme);
    const bool complete=constantComplete && sweepComplete && timingComplete && matrixComplete && readmeComplete;
    if(!complete) {
        std::cerr<<"Failed to finalize BBD qualification artifacts\n";
        return 1;
    }
    std::cout<<"Wrote "<<directory<<"\n";
    return 0;
}
