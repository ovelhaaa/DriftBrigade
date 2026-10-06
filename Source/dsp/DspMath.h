#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
namespace drift {
inline constexpr double pi = 3.14159265358979323846;
inline double bounded(double x, double lo, double hi) noexcept { return std::isfinite(x) ? std::clamp(x, lo, hi) : lo;
}
// Only the IEEE underflow tail is cleared; normal values and signed zero stay
// unchanged.
inline double flushSubnormal(double x) noexcept {
  return x != 0 && std::abs(x) < std::numeric_limits<double>::min()
             ? std::copysign(0.0, x)
             : x; }
inline double unit(double x) noexcept { return bounded(x, 0.0, 1.0); }
class Random {
public:
    void reset(std::uint32_t seed) noexcept { state = seed == 0 ? 0x9e3779b9u : seed; }
    double next() noexcept {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return static_cast<double>(state) / 4294967296.0;
    }
private: std::uint32_t state = 1;
};
} // namespace drift
