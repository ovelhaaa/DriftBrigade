#pragma once
#include "DspMath.h"
namespace drift {
// One-pole smoothing is independent of callback segmentation.
class ParameterSmoother {
public:
    void prepare(double sr, double seconds = 0.05) noexcept { coefficient = std::exp(-1.0 / (sr * seconds)); }
    void reset(double x) noexcept { current = target = x; }
    void setTarget(double x) noexcept { target = x; }
    double next() noexcept { current = target + coefficient * (current - target); return current; }
    double value() const noexcept { return current; }
private: double coefficient = 0, current = 0, target = 0;
};
}
