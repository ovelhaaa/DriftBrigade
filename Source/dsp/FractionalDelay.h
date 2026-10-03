#pragma once
#include "DspMath.h"
#include <vector>
namespace drift {
class DigitalFractionalDelay {
public:
    void prepare(double sr, double maximumSeconds = 0.060) {
        sampleRate = sr; buffer.assign(static_cast<std::size_t>(std::ceil(sr*maximumSeconds))+8, 0.0); writeIndex = 0;
    }
    void reset() noexcept { std::fill(buffer.begin(), buffer.end(), 0.0); writeIndex = 0; }
    double process(double input, double seconds) noexcept {
        const double delay = bounded(seconds*sampleRate, 3.0, static_cast<double>(buffer.size()-4));
        buffer[writeIndex] = input;
        double position = static_cast<double>(writeIndex) - delay;
        if (position < 0) position += static_cast<double>(buffer.size());
        const auto i = static_cast<std::size_t>(position);
        const double t = position - static_cast<double>(i);
        const double a = buffer[(i+buffer.size()-1)%buffer.size()], b = buffer[i];
        const double c = buffer[(i+1)%buffer.size()], d = buffer[(i+2)%buffer.size()];
        const double result = b + 0.5*t*(c-a+t*(2*a-5*b+4*c-d+t*(3*(b-c)+d-a)));
        if (++writeIndex == buffer.size()) writeIndex = 0;
        return result;
    }
    std::size_t capacity() const noexcept { return buffer.size(); }
private: std::vector<double> buffer; std::size_t writeIndex = 0; double sampleRate = 48000;
};
// Replace the contained engine here in M2; the bank and modulation know only seconds.
class DelayPath {
public:
    void prepare(double sr) { delay.prepare(sr); dcPole = std::exp(-2*pi*5.0/sr); reset(); }
    void reset() noexcept { delay.reset(); previousWet = previousInput = dcState = 0; }
    double process(double input, double seconds, double feedback) noexcept {
        // Hermite absolute coefficient sum <= 1.25. 0.75*1.25 < 1,
        // plus hard limiting guarantees a bounded loop even under abusive input.
        const double write = std::clamp(input + bounded(feedback, 0, 0.75)*previousWet, -16.0, 16.0);
        const double wet = delay.process(write, seconds);
        previousWet = wet;
        // DC blocker on the output, outside the feedback loop.
        dcState = wet - previousInput + dcPole*dcState; previousInput = wet;
        return dcState;
    }
    std::size_t capacity() const noexcept { return delay.capacity(); }
private: DigitalFractionalDelay delay; double previousWet = 0, previousInput = 0, dcState = 0, dcPole = 0;
};
}
