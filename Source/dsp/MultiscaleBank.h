#pragma once
#include "DspMath.h"
#include <array>
namespace drift {
inline constexpr std::array<double, 3> crossoversHz {250.0, 1000.0, 4000.0};
enum class BankMode { Gentle, Selective };
class MultiscaleBank {
public:
    void prepare(double sr) noexcept {
        for (std::size_t i = 0; i < 3; ++i) {
            const double g = std::tan(pi * crossoversHz[i] / sr);
            coefficients[i] = g / (1.0 + g);
        }
        reset();
    }
    void setMode(BankMode value) noexcept { mode = value; }
    void reset() noexcept { state.fill(0); cascadeState.fill(0); }
    std::array<double, 4> process(double x) noexcept {
        if (mode == BankMode::Selective) {
            // Three sequential, two-pole complementary residual splits. Each
            // low+high pair is exact, so the four outputs telescope to x.
            std::array<double, 4> bands {};
            double residual = x;
            for (std::size_t i=0; i<3; ++i) {
                const auto offset = 2*i;
                const double v1=(residual-cascadeState[offset])*coefficients[i];
                const double l1=v1+cascadeState[offset]; cascadeState[offset]=l1+v1;
                const double v2=(l1-cascadeState[offset+1])*coefficients[i];
                const double low=v2+cascadeState[offset+1]; cascadeState[offset+1]=low+v2;
                bands[i]=low; residual-=low;
            }
            bands[3]=residual;
            return bands;
        }
        std::array<double, 3> low {};
        for (std::size_t i = 0; i < 3; ++i) {
            const double v = (x - state[i]) * coefficients[i];
            low[i] = v + state[i]; state[i] = low[i] + v;
        }
        return {low[0], low[1]-low[0], low[2]-low[1], x-low[2]};
    }
private:
    BankMode mode = BankMode::Gentle;
    std::array<double, 3> coefficients {}, state {};
    std::array<double, 6> cascadeState {};
};
}
