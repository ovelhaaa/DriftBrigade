#include "BBDListeningFixtures.h"
#include "dsp/BBDHostScene.h"
#include "../tests/AllocationTracker.h"
#include <chrono>
#include <iostream>

namespace {
using namespace drift;
using listening::require;
using Clock = std::chrono::steady_clock;
using Block = std::array<std::array<float, 512>, 2>;
constexpr std::uint32_t seed = 0x44524946u;
constexpr std::array<std::size_t, 8> variable {16, 127, 32, 511, 17, 512, 63, 128};
constexpr std::array<double, 4> rates {44100, 48000, 88200, 96000};
constexpr std::array<std::size_t, 6> buffers {16, 32, 64, 128, 512, 0};
constexpr std::size_t warmup = 128;

float stimulus(std::size_t sample, std::size_t channel, double rate) {
    const auto segment = static_cast<std::size_t>(rate / 50);
    const double phase = 6.283185307179586 * sample / rate;
    const double stereo = channel ? .8 : 1.;
    switch ((sample / segment) % 9) {
        case 0: return static_cast<float>(.23 * std::sin(220 * phase) * stereo);
        case 1: return sample % segment == 0 ? .98f : 0.f;
        case 2: return static_cast<float>(.16 * (std::sin(330 * phase) + std::sin(440 * phase)) * stereo);
        case 3: return static_cast<float>(.5 * std::sin(47 * phase));
        case 4: return .5f * (static_cast<float>(listening::scramble(seed + static_cast<std::uint32_t>(sample * 2 + channel)) >> 8) / 8388608.f - 1.f);
        case 5: return static_cast<float>(.999 * std::sin(440 * phase));
        default: return 0.f;
    }
}

void fill(Block& audio, std::size_t start, std::size_t length, std::size_t channels, double rate) {
    for (std::size_t ch = 0; ch < channels; ++ch)
        for (std::size_t n = 0; n < length; ++n)
            audio[ch][n] = stimulus(start + n, ch, rate);
}

void prepare(DriftEngine& engine, double rate, bool product, const BBDAdmissionRequest& scene) {
    if (product) {
        const auto result = engine.prepareProduct(rate, scene, seed);
        const auto expected = rate == 96000 ? BBDPreparationOutcome::FallbackToDigital : BBDPreparationOutcome::BBD;
        require(result.outcome == expected && result.preparedSampleRate == rate, "host preparation outcome/rate");
    } else {
        engine.setParameters(scene.initial);
        engine.prepare(rate, seed);
        require(engine.delayBackend() == DelayBackend::DigitalFractional, "Digital reference backend");
    }
}

void lifecycle() {
    DriftEngine engine, offline;
    auto scene = bbdHostScene();
    prepare(engine, rates.front(), false, scene);
    for (double rate : {44100., 48000., 88200., 96000., 44100.}) {
        prepare(engine, rate, true, scene);
        prepare(offline, rate, rate != 96000, scene);
        for (std::size_t channels : {1u, 2u}) {
            engine.reset(seed);
            offline.reset(seed);
            Block audio{}, reference{};
            float* outputs[] {audio[0].data(), audio[1].data()};
            float* oracle[] {reference[0].data(), reference[1].data()};
            const auto length = rate == 44100 ? 127u : 511u;
            const auto before = allocations.load();
            for (std::size_t start = 0; start < 32768; start += length) {
                fill(audio, start, length, channels, rate);
                reference = audio;
                // Same audio, with an unaligned host boundary in the first render.
                engine.process(outputs, channels, 17);
                float* rest[] {outputs[0] + 17, outputs[1] + 17};
                engine.process(rest, channels, length - 17);
                offline.process(oracle, channels, length);
                require(audio == reference, "block partition, reset or fallback mismatch");
                for (std::size_t ch = 0; ch < channels; ++ch)
                    require(std::all_of(audio[ch].begin(), audio[ch].end(),
                                        [](float x) { return std::isfinite(x); }), "nonfinite host output");
            }
            require(allocations.load() == before, "host callback allocated");
        }
    }
    scene.initial[Feedback] = parameterSpecs[Feedback].maximum;
    prepare(engine, 48000, true, scene);
    Block tail{};
    float* tailChannels[] {tail[0].data(), tail[1].data()};
    const auto before = allocations.load();
    for (std::size_t block = 0; block <= 192; ++block) {
        tail = {};
        if (block == 0 || block == 192) tail[0][0] = tail[1][0] = .999f;
        engine.process(tailChannels, 2, 512);
        for (const auto& channel : tail)
            require(std::all_of(channel.begin(), channel.end(),
                                [](float x) { return std::isfinite(x); }), "nonfinite feedback tail");
    }
    require(allocations.load() == before && std::abs(tail[0][0]) > .1f,
            "allocation or no response after prolonged silence");

    scene.initial[Mix] = 0;
    prepare(engine, 48000, true, scene);
    Block dry{};
    dry[0][0] = std::numeric_limits<float>::max();
    dry[1][0] = -dry[0][0];
    const auto reference = dry;
    float* outputs[] {dry[0].data(), dry[1].data()};
    engine.process(outputs, 2, dry[0].size());
    require(dry == reference, "Mix 0 changed finite dry signal");
}

struct Callback {
    std::size_t size;
    double dspNs, clockNs, deadlineNs;
};

void distribution(std::ostream& out, std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    for (double p : {.5, .95, .99, .999})
        out << ',' << samples[static_cast<std::size_t>(std::ceil(samples.size() * p)) - 1];
    out << ',' << samples.back();
}

void measure(std::ostream& summary, std::ostream& raw, double rate, std::size_t size,
             std::size_t channels, bool product, std::size_t callbacks) {
    const auto scene = bbdHostScene();
    DriftEngine engine;
    prepare(engine, rate, product, scene);
    const bool admitted = product && rate != 96000;
    const char* backend = admitted ? "BBD" : product ? "DigitalFallback" : "Digital";
    const std::string label = size ? std::to_string(size) : "variable";
    std::vector<Callback> records(callbacks);
    Block audio{};
    float* outputs[] {audio[0].data(), audio[1].data()};
    std::size_t start = 0;
    using Ns = std::chrono::duration<double, std::nano>;
    const auto wallStart = Clock::now();
    for (std::size_t i = 0; i < callbacks + warmup; ++i) {
        const auto length = size ? size : variable[i % variable.size()];
        fill(audio, start, length, channels, rate);
        const auto& parameters = (start / static_cast<std::size_t>(rate / 25)) % 2
            ? scene.automation.maximum : scene.automation.minimum;
        const auto before = allocations.load();
        const auto clockStart = Clock::now();
        const auto dspStart = Clock::now();
        engine.setParameters(parameters);
        engine.process(outputs, channels, length);
        const auto end = Clock::now();
        require(allocations.load() == before, "automated callback allocated");
        require(!admitted || engine.bbdAutomationStatus() == BBDAutomationStatus::Accepted,
                "legal automation endpoint rejected");
        for (std::size_t ch = 0; ch < channels; ++ch)
            for (std::size_t n = 0; n < length; ++n)
                require(std::isfinite(audio[ch][n]), "nonfinite automated output");
        if (i >= warmup)
            records[i - warmup] = {length, Ns(end - dspStart).count(), Ns(dspStart - clockStart).count(),
                                   1e9 * length / rate};
        start += length;
    }
    const auto wallSeconds = std::chrono::duration<double>(Clock::now() - wallStart).count();
    std::vector<double> us, percentages, overhead;
    std::size_t misses = 0, samples = 0;
    for (std::size_t i = 0; i < callbacks; ++i) {
        const auto& r = records[i];
        us.push_back(r.dspNs / 1000);
        percentages.push_back(100 * r.dspNs / r.deadlineNs);
        overhead.push_back(r.clockNs);
        misses += r.dspNs > r.deadlineNs;
        samples += r.size;
        raw << rate << ',' << label << ',' << channels << ',' << backend << ',' << i << ','
            << r.size << ',' << r.dspNs << ',' << r.deadlineNs << ',' << percentages.back() << ',' << r.clockNs << '\n';
    }
    summary << rate << ',' << label << ',' << channels << ',' << backend << ',' << callbacks
            << ',' << samples / rate << ',' << wallSeconds << ',' << misses << ',' << 100. * misses / callbacks;
    distribution(summary, us);
    distribution(summary, percentages);
    distribution(summary, overhead);
    summary << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc >= 2 && argc <= 3, "usage: drift_bbd_host_qualification --tests-only | OUTPUT_DIR [callbacks-per-case]");
        lifecycle();
        if (std::string(argv[1]) == "--tests-only") {
            std::cout << "PASS: host lifecycle, partition, reset, fallback, Mix 0 and allocations\n";
            return 0;
        }
        std::size_t used = 0;
        const auto callbacks = argc == 3 ? std::stoul(argv[2], &used) : 2000;
        require((argc == 2 || used == std::string(argv[2]).size()) && callbacks > 0 && callbacks <= 1000000,
                "callbacks-per-case must be in [1, 1000000]");
        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        std::ofstream summary(dir / "host_summary.csv"), raw(dir / "host_callbacks.csv");
        summary.exceptions(std::ios::failbit | std::ios::badbit);
        raw.exceptions(std::ios::failbit | std::ios::badbit);
        summary << "rate,buffer,channels,backend,callbacks,audio_s,wall_s_with_warmup,misses,miss_pct";
        for (auto unit : {"us", "pct", "clock_ns"})
            for (auto percentile : {"p50", "p95", "p99", "p999", "max"}) summary << ',' << percentile << '_' << unit;
        summary << '\n';
        raw << "rate,buffer,channels,backend,callback,block,dsp_ns,deadline_ns,util_pct,clock_ns\n";
        for (double rate : rates)
            for (auto buffer : buffers)
                for (std::size_t channels : {1u, 2u})
                    for (bool product : {false, true})
                        measure(summary, raw, rate, buffer, channels, product, callbacks);
        raw.close();
        summary.close();
        std::cout << "PASS: host timing matrix and callback checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
