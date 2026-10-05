#pragma once
#include <algorithm>
#include <cmath>
namespace drift_m24 {
enum class BBDNonlinearityMode { Disabled, EngineeringPolynomial };
struct BBDNonlinearConfig {
  BBDNonlinearityMode mode = BBDNonlinearityMode::Disabled;
  double strength = 0;
};
// Aggregate memoryless output-event model; see m2_4_bbd_nonlinearity.md.
class BBDNonlinearTransfer {
public:
  void configure(BBDNonlinearConfig c) noexcept {
    strength =
        c.mode == BBDNonlinearityMode::Disabled
            ? 0
            : (std::isfinite(c.strength) ? std::clamp(c.strength, 0., 1.) : 0);
  }
  bool enabled() const noexcept { return strength != 0; }
  double process(double x) const noexcept {
    if (!enabled())
      return x;
    if (!std::isfinite(x))
      return 0;
    const double z = std::clamp(x, -1., 1.);
    const double y = z - strength * (a * z * z + b * z * z * z);
    if (x == z)
      return y;
    const double t = std::abs(x) - 1;
    return y + std::copysign(derivative(z) * (t / (1 + t)), x);
  }
  double derivative(double x) const noexcept {
    if (!enabled())
      return 1;
    if (!std::isfinite(x))
      return 0;
    const double z = std::clamp(x, -1., 1.);
    const double slope = 1 - strength * (2 * a * z + 3 * b * z * z);
    const double t = std::max(0., std::abs(x) - 1);
    return (slope / (1 + t)) / (1 + t);
  }
  static constexpr double a = 1. / 8, b = 1. / 18;

private:
  double strength = 0;
};
} // namespace drift
