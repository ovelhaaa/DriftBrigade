#pragma once
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
namespace drift_reference {
// H(s)=sum r/(s-p). Exact evolution with impulses or piecewise-constant input.
class AsyncAnalogFilter {
public:
    using Complex=std::complex<double>;
    void prototype(double cutoffHz) noexcept {
        count=1; poles[0]=-2.0*3.14159265358979323846*cutoffHz; residues[0]=-poles[0];
    }
    void paperReference(bool output) noexcept {
        count=5;
        // Holters/Parker DAFx-18 Table 1: poles in rad/s, matching residues.
        poles=output ? std::array<Complex,5>{{-176261.,{-51468.,21437.},{-51468.,-21437.},{-26276.,-59699.},{-26276.,59699.}}}
                     : std::array<Complex,5>{{-46580.,{-55482.,25082.},{-55482.,-25082.},{-26292.,-59437.},{-26292.,59437.}}};
        residues=output ? std::array<Complex,5>{{5092.,{11256.,-99566.},{11256.,99566.},{-13802.,-24606.},{-13802.,24606.}}}
                        : std::array<Complex,5>{{251589.,{-130428.,-4165.},{-130428.,4165.},{4634.,-22873.},{4634.,22873.}}};
    }
    void reset() noexcept { state.fill(0.0); }
    void injectImpulse(double area) noexcept {
        for(std::size_t i=0;i<count;++i) state[i]+=residues[i]*area;
    }
    double advance(double seconds,double heldInput=0.0) noexcept {
        for(std::size_t i=0;i<count;++i) {
            const Complex e=std::exp(poles[i]*seconds);
            state[i]=e*state[i]+residues[i]/poles[i]*(e-Complex(1.0))*heldInput;
        }
        return value();
    }
    double value() const noexcept {
        Complex sum=0.0; for(std::size_t i=0;i<count;++i) sum+=state[i]; return sum.real();
    }
    Complex response(double hz) const noexcept {
        Complex sum=0.0;
        for(std::size_t i=0;i<count;++i) sum+=residues[i]/(Complex(0.0,2.0*3.14159265358979323846*hz)-poles[i]);
        return sum;
    }
    bool finiteState() const noexcept {
        for(const auto& s:state) if(!std::isfinite(s.real()) || !std::isfinite(s.imag())) return false;
        return true;
    }
#ifdef DRIFT_BBD_INSTRUMENT
    // Read-only test observation; frozen equations and operation order unchanged.
    std::array<Complex,5> stateSnapshot() const noexcept { return state; }
#endif
private:
    std::size_t count=1;
    std::array<Complex,5> poles{{-1.0}},residues{{1.0}},state{};
};
}


