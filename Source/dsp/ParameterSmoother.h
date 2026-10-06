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
  void clearSubnormalState() noexcept { current = flushSubnormal(current); }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  void qualificationInjectTail(double value) noexcept { current = value; }
#endif
  double value() const noexcept { return current; }

private:
  double coefficient = 0, current = 0, target = 0;
};
} // namespace drift
