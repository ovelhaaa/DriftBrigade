#pragma once
#include "DspMath.h"
namespace drift {
inline double depthSeconds(double rate, double depth, bool perceptual = true) noexcept {
    depth = unit(depth);
    if (!perceptual) return 0.005*depth;
    const double r = std::clamp(rate, 4.0, 9.0);
    const double lower = (814.0/r-66.0)*1e-6;
    const double upper = (4800.0/r-350.0)*1e-6;
    double fitted;
    if (depth < 0.25) fitted = lower*depth/0.25;
    else if (depth <= 0.75) fitted = lower*std::pow(upper/lower, (depth-0.25)/0.5);
    else fitted = upper*(1+(depth-0.75)/0.25);
    // Independent slow-rate extension, not extrapolation of the upper fit.
    const double slowBlend = unit((rate-0.05)/(4.0-0.05));
    return rate < 4.0 ? (1-slowBlend)*0.005*depth + slowBlend*fitted : fitted;
}
}
