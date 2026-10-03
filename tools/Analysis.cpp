#include "dsp/DriftEngine.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <cstdint>
#include <filesystem>
namespace {
void writeWord(std::ofstream& out, std::uint32_t x, int bytes) {
    for(int i=0;i<bytes;++i) out.put(static_cast<char>((x>>(8*i))&255));
}
void wavHeader(std::ofstream& out) {
    constexpr std::uint32_t bytes=48000*8*2*2;
    out.write("RIFF",4); writeWord(out,36+bytes,4); out.write("WAVEfmt ",8);
    writeWord(out,16,4); writeWord(out,1,2); writeWord(out,2,2); writeWord(out,48000,4);
    writeWord(out,192000,4); writeWord(out,4,2); writeWord(out,16,2); out.write("data",4); writeWord(out,bytes,4);
}
}
int main(int argc, char** argv) {
    // All I/O is offline. Arguments: output CSV, coherence, Chaos, Dynamics, optional raw-depth/flange.
    const std::string path=argc>1 ? argv[1] : "drift-analysis.csv";
    std::ofstream csv(path);
    if(!csv) { std::cerr << "Cannot open " << path << '\n'; return 1; }
    drift::EngineParameters p;
    try { if(argc>2) p[drift::Coherence]=std::stod(argv[2]); if(argc>3) p[drift::Chaos]=std::stod(argv[3]); if(argc>4) p[drift::Dynamics]=std::stod(argv[4]); }
    catch(...) { std::cerr << "Invalid numeric argument\n"; return 1; }
    drift::DriftEngine engine; engine.setParameters(p); engine.prepare(48000,12345);
    if(argc>5 && std::string(argv[5])=="raw-depth") engine.usePerceptualDepth(false);
    if(argc>5 && std::string(argv[5])=="flange") {
        p[drift::Center]=1.5; p[drift::Feedback]=0.45; p[drift::Motion]=1.3;
        p[drift::Width]=0; p[drift::Depth]=0.8; engine.setParameters(p); engine.reset(12345);
    }
    auto wavPath=std::filesystem::path(path); wavPath.replace_extension(".wav");
    std::ofstream wav(wavPath,std::ios::binary); if(!wav) { std::cerr << "Cannot open WAV\n"; return 1; } wavHeader(wav);
    csv << "time_s,input,output_l,output_r,organic,random_control,envelope,effective_chaos,effective_feedback,wet_prominence";
    for(int ch=0;ch<2;++ch) for(int b=0;b<4;++b) csv << ",mod_" << ch << '_' << b << ",delay_s_" << ch << '_' << b;
    csv << '\n' << std::setprecision(12);
    for(int i=0;i<48000*8;++i) {
        const double t=i/48000.; const double amplitude=t<2 ? 0.02 : t<5 ? 0.8 : 0.001;
        const double input=amplitude*(0.7*std::sin(2*drift::pi*220*t)+0.3*std::sin(2*drift::pi*1760*t));
        const auto out=engine.processSample(input,input); const auto& trace=engine.telemetry();
        for(double sample : out) {
            const auto pcm=static_cast<std::int16_t>(std::clamp(sample*0.4,-1.0,1.0)*32767);
            writeWord(wav,static_cast<std::uint16_t>(pcm),2);
        }
        if(i%48==0) {
            csv << t << ',' << input << ',' << out[0] << ',' << out[1] << ',' << trace.organic << ',' << trace.randomControl << ',' << trace.envelope << ',' << trace.effectiveChaos << ',' << trace.effectiveFeedback << ',' << trace.wetProminence;
            for(int ch=0;ch<2;++ch) for(int b=0;b<4;++b) csv << ',' << trace.modulation[ch][b] << ',' << trace.delaySeconds[ch][b];
            csv << '\n';
        }
    }
    csv.close(); if(!csv) { std::cerr << "CSV write failed\n"; return 1; }
    wav.close(); if(!wav) { std::cerr << "WAV write failed\n"; return 1; }
    const auto start=std::chrono::steady_clock::now();
    double checksum=0;
    for(int i=0;i<480000;++i) checksum+=engine.processSample(0.1,0.1)[0];
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout << "Exported 8 seconds at 1 kHz telemetry to " << path << "; stereo DSP benchmark: " << seconds << " s / 10 s audio (" << 100*seconds/10 << "% of one core), checksum=" << checksum << '\n';
}
