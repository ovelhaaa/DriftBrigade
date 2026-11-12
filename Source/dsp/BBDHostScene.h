#pragma once
#include "BBDProductAdmission.h"

namespace drift {
inline BBDAdmissionRequest bbdHostScene() noexcept {
    BBDAdmissionRequest request;
    for (std::size_t i = 0; i < Count; ++i) {
        request.automation.minimum.values[i] = parameterSpecs[i].minimum;
        request.automation.maximum.values[i] = parameterSpecs[i].maximum;
    }
    request.initial[Center] = 24;
    request.automation.minimum[Center] = 22;
    request.automation.maximum[Depth] = 0.85f;
    return request;
}

inline bool withinHostScene(const EngineParameters& p, const BBDAutomationEnvelope& box) noexcept {
    for (std::size_t i = 0; i < Count; ++i)
        if (!(p.values[i] >= box.minimum.values[i] && p.values[i] <= box.maximum.values[i]))
            return false;
    return true;
}
} // namespace drift
