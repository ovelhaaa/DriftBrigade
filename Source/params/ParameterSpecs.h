#pragma once
#include <array>
#include <cstddef>
namespace drift {
enum Param : std::size_t { Motion, Depth, Center, Chaos, Coherence, Dynamics, Feedback, Mix, Width, Count };
struct ParameterSpec { const char* id; const char* name; const char* unit; float minimum, maximum, initial; bool logarithmic; };
inline constexpr std::array<ParameterSpec, Count> parameterSpecs {{
    {"motion", "Motion", "Hz", 0.05f, 10.f, 0.7f, true},
    {"depth", "Depth", "%", 0.f, 1.f, 0.5f, false},
    {"center", "Center", "ms", 0.3f, 30.f, 8.f, true},
    {"chaos", "Chaos", "%", 0.f, 1.f, 0.55f, false},
    {"coherence", "Coherence", "%", 0.f, 1.f, 0.45f, false},
    {"dynamics", "Dynamics", "%", 0.f, 1.f, 0.25f, false},
    {"feedback", "Feedback", "%", 0.f, 0.65f, 0.12f, false},
    {"mix", "Mix", "%", 0.f, 1.f, 0.5f, false},
    {"width", "Width", "%", 0.f, 1.f, 0.6f, false}
}};
struct EngineParameters {
    std::array<double, Count> values {};
    EngineParameters() noexcept { for (std::size_t i=0; i<Count; ++i) values[i] = parameterSpecs[i].initial; }
    double& operator[](Param p) noexcept { return values[p]; }
    double operator[](Param p) const noexcept { return values[p]; }
};
}
