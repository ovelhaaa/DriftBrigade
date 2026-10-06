#pragma once
// Offline fixtures only. No product parameters, coefficients or topology changes.
#include "dsp/DriftEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace listening {
using namespace drift;
constexpr int listeningRate = 48000;
constexpr double duration = 10, onset = .5, excitationEnd = 7.5;
constexpr std::uint32_t bakeoffSeed = 0x4d333042u;
constexpr std::uint32_t engineSeed = 77; // Preserve M2.9 band-seed policy.
using Audio = std::vector<std::array<double, 2>>;
inline void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
inline double db(double x) { return 20 * std::log10(std::max(x, 1e-300)); }
inline std::string number(double x) {
  std::ostringstream s; s << std::setprecision(17) << x; return s.str();
}
inline std::uint32_t scramble(std::uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
  x *= 0x846ca68bu; return x ^ (x >> 16);
}
inline double productClock(std::size_t stages, double sr) {
  return stages == 512 ? std::min(192000., 4 * sr) : std::min(384000., 8 * sr);
}
inline double productFloor(std::size_t stages, double sr) {
  return stages / (2 * productClock(stages, sr));
}
inline double safeDepth(double motion, double center, double sr, OrganicVariant v) {
  // Explicit 15% reserve above the PRODUCT floor, in addition to engine admission.
  const double allowed = .85 * (center - productFloor(1024, sr)) /
                         DriftEngine::combinedModulationBound(v);
  double lo = 0, hi = 1;
  for (int i = 0; i < 60; ++i) {
    const double mid = (lo + hi) / 2;
    if (depthSeconds(motion, mid) <= allowed) lo = mid; else hi = mid;
  }
  return lo;
}
struct Scene { std::string id; EngineParameters p; };
inline Scene scene(const std::string &id) {
  Scene s{id, {}};
  auto &p = s.p;
  p[Dynamics] = .15;
  if (id == "subtle") {
    p[Motion]=.35; p[Depth]=.22; p[Center]=8; p[Chaos]=.3;
    p[Coherence]=.65; p[Feedback]=.05; p[Mix]=.35; p[Width]=.65;
  } else if (id == "wide" || id == "wide_high_chaos") {
    p[Motion]=.7; p[Depth]=.25; p[Center]=12; p[Chaos]=id=="wide"?.5:1;
    p[Coherence]=.3; p[Feedback]=.1; p[Mix]=.5; p[Width]=.95;
  } else if (id == "slow") {
    p[Motion]=.15; p[Depth]=.55; p[Center]=18; p[Chaos]=.8;
    p[Coherence]=.35; p[Feedback]=.12; p[Mix]=.5; p[Width]=.85;
  } else if (id == "short") {
    p[Motion]=.9; p[Center]=2.5; p[Chaos]=.5; p[Coherence]=.45;
    p[Feedback]=.12; p[Mix]=.5; p[Width]=.8;
    // Freeze one setting valid at all three qualification rates. Do not adapt DSP.
    p[Depth]=safeDepth(p[Motion], .0025, 44100, OrganicVariant::Wander);
  } else if (id == "feedback") {
    p[Motion]=.55; p[Depth]=.28; p[Center]=10; p[Chaos]=.5;
    p[Coherence]=.45; p[Feedback]=.45; p[Mix]=.5; p[Width]=.8;
    p[Dynamics]=.1;
  } else throw std::runtime_error("Unknown scene");
  return s;
}
struct Candidate {
  std::string id;
  DelayBackend backend = DelayBackend::ExperimentalBBD;
  std::size_t stages = 1024;
  BBDHeadroomProfile gain = BBDHeadroomProfile::Nominal;
  OrganicVariant organic = OrganicVariant::Wander;
  BankMode bank = BankMode::Gentle;
};
inline const char *backendName(DelayBackend b) {
  return b==DelayBackend::DigitalFractional?"DigitalFractional":"ExperimentalBBD";
}
inline const char *gainName(BBDHeadroomProfile g) {
  return g==BBDHeadroomProfile::Conservative?"Conservative":"Nominal";
}
inline const char *organicName(OrganicVariant v) {
  return v==OrganicVariant::Wander?"Wander":v==OrganicVariant::PaperNarrowband?
         "PaperNarrowband":"PhaseDrift";
}
inline const char *bankName(BankMode b) { return b==BankMode::Gentle?"Gentle":"Selective"; }
struct Group {
  std::string set, id; int source; Scene settings;
  std::vector<Candidate> candidates; double sourceGain=1;
};
inline std::vector<Group> groups() {
  Candidate b{"BalancedListeningCandidate"}, d=b, e=b, c=b, p=b, f=b, s=b;
  d.id="DigitalReference"; d.backend=DelayBackend::DigitalFractional;
  e.id="EconomyListeningCandidate"; e.stages=512;
  c.id="ConservativeListeningCandidate"; c.gain=BBDHeadroomProfile::Conservative;
  p.id="PaperNarrowbandCandidate"; p.organic=OrganicVariant::PaperNarrowband;
  f.id="PhaseDriftCandidate"; f.organic=OrganicVariant::PhaseDrift;
  s.id="SelectiveCandidate"; s.bank=BankMode::Selective;
  return {
    {"A","A01",0,scene("subtle"),{d,b}},
    {"A","A02",0,scene("wide"),{d,b}},
    {"A","A03",1,scene("feedback"),{d,b}},
    {"A","A04",2,scene("slow"),{d,b}},
    {"B","B01",0,scene("wide"),{e,b}},
    {"B","B02",1,scene("short"),{e,b}},
    {"C","C01",0,scene("subtle"),{c,b},.125},
    {"C","C02",1,scene("feedback"),{c,b}},
    {"D","D01",0,scene("wide"),{b,p,f}},
    {"D","D02",2,scene("wide_high_chaos"),{b,p,f}},
    {"E","E01",0,scene("wide"),{b,s}},
    {"E","E02",1,scene("subtle"),{b,s}}
  };
}
inline char sourceLabel(int kind) { return char('A'+kind); }
inline std::uint32_t sourceSeed(int kind) { return scramble(bakeoffSeed+std::uint32_t(kind)); }
inline double phase(int kind, int partial) {
  return 2*pi*double(scramble(sourceSeed(kind)+std::uint32_t(partial)))/4294967296.;
}
inline Audio source(int kind, int sr, double seconds=duration) {
  require(kind>=0 && kind<3, "Invalid source");
  Audio a(std::size_t(std::llround(sr*seconds)));
  const double chords[2][4]={{130.81278265,164.81377846,195.99771799,246.94165063},
                            {146.83238396,174.61411572,220.,261.62556530}};
  const double notes[]={146.83238396,195.99771799,220.,174.61411572,
                        146.83238396,246.94165063,220.,195.99771799};
  for (std::size_t n=0;n<a.size();++n) {
    const double t=double(n)/sr-onset;
    if (t<0 || t>=excitationEnd-onset) continue;
    if (kind==0) {
      // Overlapping two polyphonic chords, smooth attacks/releases, 10 partials.
      for (int chord=0;chord<2;++chord) {
        const double local=t-3.25*chord;
        if (local<0 || local>3.75) continue;
        const double env=std::sin(pi*std::min(1.,local/.7)/2)*
                         std::sin(pi*std::min(1.,(3.75-local)/.9)/2);
        for (int ch=0;ch<2;++ch) for (int note=0;note<4;++note)
          for (int h=1;h<=10;++h)
            a[n][ch]+=.026*env*std::exp(-.15*h)/std::pow(h,1.35)*
              std::sin(2*pi*chords[chord][note]*h*t+phase(kind,note*10+h)+ch*.19*h);
      }
    } else if (kind==1) {
      const int event=int(t/.4375);
      const double local=t-event*.4375;
      const double freq=notes[event%8];
      const double attack=1-std::exp(-local/.0012);
      for (int h=1;h<=14;++h) {
        const double value=.15*attack*std::exp(-(5.+1.2*h)*local)/std::pow(h,1.2)*
          std::cos(2*pi*freq*h*local+phase(kind,h));
        a[n][0]+=value; a[n][1]+=value*(.92+.08*std::cos(.7*h));
      }
    } else {
      // Harmonic lead with formant-shaped partial amplitudes and moderate dynamics.
      const int note=int(t/.875);
      const double local=t-note*.875, freq=notes[(note+2)%8];
      const double env=(1-std::exp(-local/.035))*std::min(1.,(.875-local)/.08)*
                       (.7+.25*std::sin(2*pi*.35*t));
      double value=0;
      for (int h=1;h<=18;++h) {
        const double hz=freq*h;
        const double formant=.25+std::exp(-std::pow((hz-700)/320,2))+
                             .6*std::exp(-std::pow((hz-1700)/500,2));
        value+=.075*env*formant/h*std::sin(2*pi*hz*local+phase(kind,h));
      }
      a[n]={value,value}; // Mono material supplied on a stereo host input.
    }
  }
  for (const auto &frame:a) for (double x:frame)
    require(std::isfinite(x) && std::abs(x)<.75, "Dry source not peak safe");
  return a;
}
struct Meter {
  double sum=0, squares=0, peak=0, left=0, right=0, ll=0, rr=0, lr=0;
  std::size_t frames=0;
  void add(const std::array<double,2> &y) {
    ++frames;
    for (double x:y) { sum+=x; squares+=x*x; peak=std::max(peak,std::abs(x)); }
    left+=y[0]; right+=y[1]; ll+=y[0]*y[0]; rr+=y[1]*y[1]; lr+=y[0]*y[1];
  }
  double rms() const { return std::sqrt(squares/(2*std::max(std::size_t(1),frames))); }
  double dc() const { return sum/(2*std::max(std::size_t(1),frames)); }
  double correlation() const {
    const double count=double(std::max(std::size_t(1),frames));
    return (lr-left*right/count)/std::max(1e-300,std::sqrt(std::max(0.,(ll-left*left/count)*(rr-right*right/count))));
  }
};
inline Meter measure(const Audio &audio, std::size_t begin, std::size_t end, double gain=1) {
  Meter m;
  for (std::size_t n=begin;n<end;++n) m.add({audio[n][0]*gain,audio[n][1]*gain});
  return m;
}
inline std::ofstream output(const std::filesystem::path &p, bool binary=false) {
  std::ofstream f;
  f.exceptions(std::ios::badbit|std::ios::failbit);
  f.open(p,binary?std::ios::out|std::ios::binary:std::ios::out);
  f << std::setprecision(17);
  return f;
}
inline void word(std::ofstream &f, std::uint32_t x, int bytes) {
  for (int i=0;i<bytes;++i) f.put(char((x>>(8*i))&255));
}
inline void wav(const std::filesystem::path &p, const Audio &a, int sr, double gain=1) {
  require(a.size()<(UINT32_MAX-36)/6,"WAV too large");
  auto f=output(p,true);
  const auto size=std::uint32_t(a.size()*6);
  f.write("RIFF",4); word(f,size+36,4); f.write("WAVEfmt ",8); word(f,16,4);
  word(f,1,2); word(f,2,2); word(f,std::uint32_t(sr),4); word(f,std::uint32_t(sr*6),4);
  word(f,6,2); word(f,24,2); f.write("data",4); word(f,size,4);
  for (const auto &frame:a) for (double x:frame) {
    x*=gain;
    require(std::isfinite(x) && std::abs(x)<1-1./8388608.,"PCM encoding would clip");
    const auto sample=std::int32_t(std::llround(x*8388608.));
    word(f,std::uint32_t(sample),3);
  }
  f.flush(); f.close();
}
inline std::vector<std::size_t> permutation(std::size_t count, int groupIndex) {
  // Seeded cyclic Latin rotation. Counts 2/3 have balanced positions within each set.
  std::vector<std::size_t> order(count);
  const auto offset=(scramble(bakeoffSeed)%count+std::size_t(groupIndex))%count;
  for (std::size_t i=0;i<count;++i) order[i]=(i+offset)%count;
  return order;
}
} // namespace listening
