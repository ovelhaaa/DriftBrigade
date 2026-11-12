#pragma once
#include "BBDModulatedDelayVoice.h"
#include "params/ParameterSpecs.h"

namespace drift {
enum class BBDProductProfile { Balanced1024 };
enum class BBDAdmissionStatus {
    NotRequested, Admitted, UnsupportedSampleRate, UnsupportedProfile,
    InvalidConfiguration, CenterBelowFloor, ExcursionOutOfRange,
    ClockLimitExceeded, PreparationFailed
};
enum class BBDPreparationOutcome { Unavailable, BBD, FallbackToDigital, DigitalPreparationFailed };
enum class BBDAutomationStatus { Inactive, Accepted, InvalidParameters, OutsideEnvelope };

struct BBDAutomationEnvelope {
    EngineParameters minimum, maximum;
};
struct BBDAdmissionRequest {
    BBDProductProfile profile = BBDProductProfile::Balanced1024;
    std::size_t physicalStages = 1024;
    BBDHeadroomProfile gainProfile = BBDHeadroomProfile::Nominal;
    double clockCeilingHz = 384000;
    EngineParameters initial;
    BBDAutomationEnvelope automation;
};
struct BBDAdmissionResult {
    DelayBackend requestedBackend = DelayBackend::ExperimentalBBD;
    DelayBackend preparedBackend = DelayBackend::Unavailable;
    BBDAdmissionRequest request;
    BBDAdmissionStatus status = BBDAdmissionStatus::NotRequested;
    BBDPreparationOutcome outcome = BBDPreparationOutcome::Unavailable;
    double sampleRate = 0, preparedSampleRate = 0;
    double maximumClockHz = 0, minimumDelaySeconds = 0;
    double maximumDelaySeconds = .055, maximumExcursionSeconds = 0;
};

// The result describes eligibility. prepareProduct completes the lifecycle fields.
BBDAdmissionResult evaluateBBDAdmission(double sampleRate, const BBDAdmissionRequest&) noexcept;
} // namespace drift
