#pragma once
#include "AsyncOperationCounts.h"
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
namespace drift_m24 {
struct RealPoleSection { double pole=-1,residue=1,residueOverPole=-1,state=0; };
struct ConjugatePolePairSection { double a=0,b=0,c=0,d=0,qReal=0,qImag=0,real=0,imag=0; };
struct AsyncAnalogTransition {
 struct Pair { double real=0,imag=0,inputReal=0,inputImag=0; };
 double decay=1,input=0;
 std::array<Pair,2> pairs{};
};
// H(s)=sum r/(s-p). Exact impulse / piecewise-constant continuous-time model.
class AsyncAnalogFilter {
public:
 using Complex=std::complex<double>;
 void prototype(double cutoffHz) noexcept {
  count=1; poles[0]=-2.0*3.14159265358979323846*cutoffHz; residues[0]=-poles[0]; compile();
 }
 void paperReference(bool output) noexcept {
  count=5;
  // Holters/Parker DAFx-18 Table 1, unchanged poles and residues.
  poles=output ? std::array<Complex,5>{{-176261.,{-51468.,21437.},{-51468.,-21437.},{-26276.,-59699.},{-26276.,59699.}}}
               : std::array<Complex,5>{{-46580.,{-55482.,25082.},{-55482.,-25082.},{-26292.,-59437.},{-26292.,59437.}}};
  residues=output ? std::array<Complex,5>{{5092.,{11256.,-99566.},{11256.,99566.},{-13802.,-24606.},{-13802.,24606.}}}
                  : std::array<Complex,5>{{251589.,{-130428.,-4165.},{-130428.,4165.},{4634.,-22873.},{4634.,22873.}}};
  compile();
 }
 void reset() noexcept { real.state=0; for(auto& p:pairs) p.real=p.imag=0; }
 void injectImpulse(double area) noexcept {
  real.state+=real.residue*area;
  for(std::size_t i=0;i<pairCount();++i) { pairs[i].real+=pairs[i].c*area; pairs[i].imag+=pairs[i].d*area; }
 }
 AsyncAnalogTransition makeTransition(double seconds) const noexcept {
  DRIFT_M24_ASYNC_COUNT(builds,1); DRIFT_M24_ASYNC_COUNT(realExp,1+pairCount()); DRIFT_M24_ASYNC_COUNT(sinCosPairs,pairCount());
  AsyncAnalogTransition t;
  t.decay=std::exp(real.pole*seconds); t.input=real.residueOverPole*(t.decay-1);
  for(std::size_t i=0;i<pairCount();++i) {
   const auto& p=pairs[i]; auto& e=t.pairs[i]; const double decay=std::exp(p.a*seconds);
   e.real=decay*std::cos(p.b*seconds); e.imag=decay*std::sin(p.b*seconds);
   e.inputReal=p.qReal*(e.real-1)-p.qImag*e.imag;
   e.inputImag=p.qReal*e.imag+p.qImag*(e.real-1);
  }
  return t;
 }
 double advanceWithTransition(const AsyncAnalogTransition& t,double heldInput=0) noexcept {
  DRIFT_M24_ASYNC_COUNT(advances,1);
  real.state=t.decay*real.state+t.input*heldInput;
  for(std::size_t i=0;i<pairCount();++i) {
   auto& p=pairs[i]; const auto& e=t.pairs[i];
   const double next=e.real*p.real-e.imag*p.imag+e.inputReal*heldInput;
   p.imag=e.real*p.imag+e.imag*p.real+e.inputImag*heldInput; p.real=next;
  }
  return value();
 }
 double advance(double seconds,double heldInput=0) noexcept {
  DRIFT_M24_ASYNC_COUNT(arbitraryBuilds,1);
  return advanceWithTransition(makeTransition(seconds),heldInput);
 }
 double value() const noexcept {
  double sum=real.state; for(std::size_t i=0;i<pairCount();++i) sum+=2*pairs[i].real; return sum;
 }
 Complex response(double hz) const noexcept {
  Complex sum=0;
  for(std::size_t i=0;i<count;++i) sum+=residues[i]/(Complex(0,2*3.14159265358979323846*hz)-poles[i]);
  return sum;
 }
 bool finiteState() const noexcept {
  if(!std::isfinite(real.state)) return false;
  for(const auto& p:pairs) if(!std::isfinite(p.real)||!std::isfinite(p.imag)) return false;
  return true;
 }
#ifdef DRIFT_BBD_INSTRUMENT
 // Qualification-only expanded state; absent from the production interface.
 std::array<Complex,5> stateSnapshot() const noexcept {
  std::array<Complex,5> result{}; result[0]=real.state;
  for(std::size_t i=0;i<pairCount();++i) { result[1+2*i]={pairs[i].real,pairs[i].imag}; result[2+2*i]=std::conj(result[1+2*i]); }
  return result;
 }
#endif
private:
 std::size_t pairCount() const noexcept { return (count-1)/2; }
 void compile() noexcept {
  real.pole=poles[0].real(); real.residue=residues[0].real(); real.residueOverPole=real.residue/real.pole;
  for(std::size_t i=0;i<pairCount();++i) {
   auto& p=pairs[i]; const auto pole=poles[1+2*i],r=residues[1+2*i],q=r/pole;
   p.a=pole.real(); p.b=pole.imag(); p.c=r.real(); p.d=r.imag(); p.qReal=q.real(); p.qImag=q.imag();
  }
 }
 std::size_t count=1;
 std::array<Complex,5> poles{{-1}},residues{{1}};
 RealPoleSection real;
 std::array<ConjugatePolePairSection,2> pairs{};
};
}
