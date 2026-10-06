#pragma once
#include "DspMath.h"
namespace drift {
class EnvelopeFollower {
public:
    void prepare(double sr) noexcept { attack = std::exp(-1.0/(sr*0.015)); release = std::exp(-1.0/(sr*0.250)); reset(); }
    void clearSubnormalState() noexcept { envelope = flushSubnormal(envelope); }
  void reset() noexcept { envelope = 0; }
    double process(double input) noexcept {
        const auto level = std::abs(input);
        const auto c = level > envelope ? attack : release;
        envelope = level + c * (envelope - level);
        return envelope;
    }
#ifdef DRIFT_BBD_REALTIME_QUALIFY
  void qualificationInjectTail(double value) noexcept { envelope = value; }
  double qualificationState() const noexcept { return envelope; }
#endif
private:
  double attack = 0, release = 0, envelope = 0;
};
} // namespace drift
