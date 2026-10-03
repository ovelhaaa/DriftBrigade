#pragma once
#include "dsp/ClockedBBDCore.h"
#include <algorithm>
#include <cmath>
#include <complex>
namespace qualification {
constexpr double sampleRate=384000.0, clockHz=8000.0;
inline double sinc(double x) { return x==0.0?1.0:std::sin(drift::pi*x)/(drift::pi*x); }
struct Spectrum {
    double desired=0,alias=0,imageLower=0,imageUpper=0,phase=0;
};
inline Spectrum measure(double ratio,drift::BBDMode mode,drift::BBDFilterProfile profile=drift::BBDFilterProfile::ValidationPrototype) {
    drift::ClockedBBDCore core; core.setQualificationMode(mode,profile); core.prepare(sampleRate,256);
    core.setDelaySeconds(256.0/(2.0*clockHz));
    const double tone=ratio*clockHz, alias=std::min(tone,clockHz-tone);
    const double frequencies[]{tone,alias,clockHz-alias,clockHz+alias};
    std::complex<double> sums[4]{};
    // Coherent one-second analysis, following 100 ms settling. No window leakage.
    constexpr int warmup=38400,count=384000;
    for(int n=0;n<warmup+count;++n) {
        const double y=core.process(std::sin(2*drift::pi*tone*n/sampleRate));
        if(n>=warmup) for(int i=0;i<4;++i)
            sums[i]+=y*std::polar(1.0,-2*drift::pi*frequencies[i]*(n+1)/sampleRate);
    }
    return {2*std::abs(sums[0])/count,2*std::abs(sums[1])/count,
            2*std::abs(sums[2])/count,2*std::abs(sums[3])/count,std::arg(sums[0])+drift::pi/2};
}
}
