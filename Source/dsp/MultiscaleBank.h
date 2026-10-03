#pragma once
#include "DspMath.h"
#include <array>
namespace drift {
inline constexpr std::array<double, 3> crossoversHz {250.0, 1000.0, 4000.0};
class MultiscaleBank {
public:
    void prepare(double sr) noexcept {
        for (std::size_t i = 0; i < 3; ++i) {
            const double g = std::tan(pi * crossoversHz[i] / sr);
            coefficients[i] = g / (1.0 + g);
        }
        reset();
    }
    void reset() noexcept { state.fill(0); }
    std::array<double, 4> process(double x) noexcept {
        std::array<double, 3> low {};
        for (std::size_t i = 0; i < 3; ++i) {
            const double v = (x - state[i]) * coefficients[i];
            low[i] = v + state[i]; state[i] = low[i] + v;
        }
        return {low[0], low[1]-low[0], low[2]-low[1], x-low[2]};
    }
private: std::array<double, 3> coefficients {}, state {};
};
}
