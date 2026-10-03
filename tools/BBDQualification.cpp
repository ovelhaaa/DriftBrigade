#include "dsp/ClockedBBDCore.h"
#include "BBDMeasurements.h"
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
using drift::ClockedBBDCore;
int main(int argc,char** argv) {
    const std::filesystem::path directory=argc>1?argv[1]:"bbd_qualification";
    std::error_code error;
    std::filesystem::create_directories(directory,error);
    if(error || !std::filesystem::is_directory(directory,error) || error) {
        std::cerr<<"Cannot create output directory: "<<(error?error.message():"path is not a directory")<<'\n'; return 1;
    }
    const std::array<const char*,8> names{{"bbd_phase_semantics.csv","bbd_constant_clock.csv","bbd_frequency_response.csv",
        "bbd_aliasing_sweep.csv","bbd_hold_response.csv","bbd_delay_sweep.csv","bbd_event_timing.csv","README.txt"}};
    std::array<std::ofstream,8> streams;
    for(std::size_t i=0;i<streams.size();++i) {
        streams[i].open(directory/names[i]); streams[i]<<std::setprecision(17);
        if(!streams[i]) { std::cerr<<"Cannot open all BBD qualification artifacts\n"; return 1; }
    }
    auto& phase=streams[0]; auto& constant=streams[1]; auto& response=streams[2]; auto& alias=streams[3];
    auto& hold=streams[4]; auto& sweep=streams[5]; auto& timing=streams[6]; auto& readme=streams[7];
    phase<<"edge_id,time_s,phase,captures,output_updates,logical_buckets,held_output\n";
    ClockedBBDCore p; p.prepare(48000,8); p.setDelaySeconds(8.0/48000);
    for(int n=0;n<24;++n) {
        p.process(n+1.); const auto& t=p.telemetry();
        phase<<n+1<<','<<(n+1)/48000.0<<','<<(t.capturesThisHostSample?"capture":"output")<<','
             <<t.capturesThisHostSample<<','<<t.outputsThisHostSample<<','<<t.logicalSignalBuckets<<','<<p.heldOutput()<<'\n';
    }
    constant<<"sample_rate,physical_stages,requested_delay_s,effective_delay_s,clock_hz,signal_sampling_rate_hz,signal_nyquist_hz,per_channel_transfer_edge_rate_hz,per_channel_edge_count_error,seconds,stereo_measured_edges_per_wall_second,relative_cpu,observed_step_delay_s,observation_error_s\n";
    double baseline=0;
    for(double sr:{44100.,48000.,88200.,96000.}) for(std::size_t stages:{256u,512u,1024u,2048u,4096u}) {
        ClockedBBDCore l,r; l.prepare(sr,stages); r.prepare(sr,stages); l.setDelaySeconds(.01); r.setDelaySeconds(.01);
        const auto start=std::chrono::steady_clock::now(); const int count=static_cast<int>(sr);
        for(int n=0;n<count;++n) { const double x=std::sin(2*drift::pi*440*n/sr); l.process(x); r.process(x); }
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if(!baseline)baseline=seconds;
        const auto t=l.telemetry(); const double expected=count*2*t.effectiveClockHz/sr;
        l.reset(); double observed=0;
        for(int n=0;n<count;++n) if(l.process(1.0)==1.0) { observed=(n+1)/sr; break; }
        constant<<sr<<','<<stages<<','<<t.requestedDelaySeconds<<','<<t.effectiveDelaySeconds<<','<<t.effectiveClockHz<<','
                <<t.signalSamplingRateHz<<','<<t.signalNyquistHz<<','<<2*t.effectiveClockHz<<','
                <<(static_cast<double>(t.totalEventCount)-expected)<<','<<seconds<<','
                <<(t.totalEventCount+r.telemetry().totalEventCount)/seconds<<','<<seconds/baseline<<','<<observed<<','<<observed-.01<<'\n';
    }
    response<<"profile,host_rate_hz,clock_hz,input_hz,measured_magnitude,analytical_baseband_magnitude,absolute_error,measured_phase_rad,analytical_phase_rad,wrapped_phase_error_rad\n";
    alias<<"profile,host_rate_hz,clock_hz,signal_nyquist_hz,input_hz,desired_hz,desired_magnitude,folded_hz,folded_magnitude,image_lower_hz,image_lower_magnitude,image_upper_hz,image_upper_magnitude,folded_attenuation_db,image_upper_attenuation_db\n";
    hold<<"host_rate_hz,clock_hz,input_hz,folded_hz,measured_magnitude,zoh_sinc,absolute_error,tolerance\n";
    for(double ratio:{.1,.25,.45,.55,.75}) {
        const auto raw=qualification::measure(ratio,drift::BBDMode::TransportOnly);
        const double folded=std::min(ratio,1-ratio), hz=ratio*qualification::clockHz;
        const double theory=qualification::sinc(folded);
        hold<<qualification::sampleRate<<','<<qualification::clockHz<<','<<hz<<','<<folded*qualification::clockHz<<','<<raw.alias<<','<<theory<<','<<std::abs(raw.alias-theory)<<",0.002\n";
        for(int profile=0;profile<3;++profile) {
            const auto mode=profile==0?drift::BBDMode::TransportOnly:drift::BBDMode::AsyncLinearReference;
            const auto coefficients=profile==2?drift::BBDFilterProfile::HoltersParkerTable1:drift::BBDFilterProfile::ValidationPrototype;
            const auto m=profile==0?raw:qualification::measure(ratio,mode,coefficients);
            const char* label=profile==0?"TransportOnly":profile==1?"AsyncPrototype2kHz":"HoltersParkerTable1";
            alias<<label<<','<<qualification::sampleRate<<','<<qualification::clockHz<<','<<qualification::clockHz/2<<','<<hz<<','<<hz<<','<<m.desired<<','
                 <<folded*qualification::clockHz<<','<<m.alias<<','<<(1-folded)*qualification::clockHz<<','<<m.imageLower<<','<<(1+folded)*qualification::clockHz<<','<<m.imageUpper<<','
                 <<20*std::log10(m.alias/raw.alias)<<','<<20*std::log10(m.imageUpper/raw.imageUpper)<<'\n';
            if(ratio<.5) {
                drift::AsyncAnalogFilter in,out;
                if(profile==2) { in.paperReference(false); out.paperReference(true); } else { in.prototype(2000); out.prototype(2000); }
                const auto gain=profile==0?std::complex<double>(1.0):in.response(hz)*out.response(hz);
                const double expected=qualification::sinc(ratio)*std::abs(gain);
                const double expectedPhase=-2*drift::pi*hz*256/(2*qualification::clockHz)+std::arg(gain);
                const double phaseError=std::remainder(m.phase-expectedPhase,2*drift::pi);
                response<<label<<','<<qualification::sampleRate<<','<<qualification::clockHz<<','<<hz<<','<<m.desired<<','<<expected<<','<<std::abs(m.desired-expected)<<','
                        <<m.phase<<','<<std::remainder(expectedPhase,2*drift::pi)<<','<<phaseError<<'\n';
            }
        }
    }
    sweep<<"output_host_time_s,requested_delay_s,instantaneous_nominal_delay_s,clock_hz,marker_id,capture_s,output_onset_s,historical_onset_transit_s,hold_center_transit_s\n";
    timing<<"host_sample,host_observation_time_s,phase,physical_edges,captures,output_updates,total_edges,total_captures,total_outputs,last_capture_s,last_output_s\n";
    ClockedBBDCore c; c.prepare(48000,512); std::vector<double> captures(1,0.0); double lastMarker=0;
    // Qualification-only IDs travel as scalar test signals; production memory
    // remains doubles and has no metadata. Clock step after 10 ms retains IDs.
    for(int n=0;n<96000;++n) {
        const double d=n<480?.012:.03; c.setDelaySeconds(d);
        const auto id=c.telemetry().totalCaptureCount+1;
        const double y=c.process(static_cast<double>(id)); const auto& t=c.telemetry();
        if(t.capturesThisHostSample) captures.push_back(t.lastCaptureTimeSeconds);
        if(y && y!=lastMarker) {
            const double entered=captures[static_cast<std::size_t>(y)];
            sweep<<(n+1)/48000.0<<','<<d<<','<<t.effectiveDelaySeconds<<','<<t.effectiveClockHz<<','<<y<<','<<entered<<','<<t.lastOutputTimeSeconds<<','
                 <<t.lastOutputTimeSeconds-entered<<','<<t.lastOutputTimeSeconds-entered+1/(2*t.effectiveClockHz)<<'\n'; lastMarker=y;
        }
        if(n<4096) timing<<n<<','<<(n+1)/48000.0<<','<<t.accumulatedClockPhase<<','<<t.eventsThisHostSample<<','<<t.capturesThisHostSample<<','<<t.outputsThisHostSample<<','
                         <<t.totalEventCount<<','<<t.totalCaptureCount<<','<<t.totalOutputCount<<','<<t.lastCaptureTimeSeconds<<','<<t.lastOutputTimeSeconds<<'\n';
    }
    readme<<"DriftBrigade-M2.1-BBD-Linear-Qualification\n\n"
        <<"Physical edges=2*f_BBD; captures=f_BBD; output updates=f_BBD; Nyquist=f_BBD/2. N is physical, storage=N/2 logical samples.\n"
        <<"Paper Eq.1: output onset is N-1 edges after capture; rectangular hold spans two edges. At constant clock its centre delay is N/(2*f_BBD). Host process consumes sample at interval start and observes output at interval end.\n"
        <<"Hold response tolerance: 0.002 absolute linear magnitude at 384 kHz host, 8 kHz BBD. Coherent one-second DFT after 100 ms settling. All measured components are peak amplitudes for a unit input sine.\n"
        <<"For below-Nyquist tones desired and folded columns coincide; above Nyquist the desired frequency is itself an image. Lower image coincides with desired above Nyquist. Components are not independent energy bins in those cases.\n"
        <<"TransportOnly holds host input between captures. AsyncPrototype2kHz uses H(s)=w/(s+w), w=2*pi*2000, at input and output for machinery validation only. HoltersParkerTable1 uses documented fifth-order circuit coefficients without additional device gain, high-pass or coloration.\n"
        <<"Async input is a host weighted impulse train, not nearest-sample interpolation; exact pole-residue advance evaluates capture instants. Output integrates actual rectangular holds between edges, then samples at host times.\n"
        <<"Analytical baseband predictions ignore host impulse-train images and folded BBD images; measured residuals include these effects. Finite aliasing is expected.\n"
        <<"Delay sweep IDs show historical onset transit across a 12-to-30 ms nominal clock step at 10 ms. The later clock controls existing history; instantaneous nominal delay is not historical delay. No metadata is added to real-time storage.\n"
        <<"Constant-clock theoretical edge rate is per channel; timed throughput explicitly combines stereo edges per wall second. CPU depends on machine.\n"
        <<"See docs/m2_1_bbd_linear_resampling.md for references, limits, tests and deferred work. Production still uses DigitalFractionalDelay.\n";
    bool complete=true;
    for(auto& stream:streams) {
        stream.flush(); const bool written=static_cast<bool>(stream); stream.close();
        const bool closed=!stream.fail(); complete=written && closed && complete;
    }
    if(!complete) { std::cerr<<"Failed to finalize BBD qualification artifacts\n"; return 1; }
    std::cout<<"Wrote "<<directory<<'\n'; return 0;
}
