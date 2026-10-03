# DRIFT BRIGADE — Multiscale Organic Modulator

M1: four complementary bands, one digital modulated delay per band/channel, seeded smooth random motion, periodic/organic morph, band coherence, envelope interaction and bounded feedback. No BBD emulation is included.

## Build

Requires CMake 3.22+, C++17 and a native compiler. The core/tests require no JUCE, network or plugin host:

```sh
cmake -S . -B build-dsp -DDRIFT_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --config Release --parallel 2
ctest --test-dir build-dsp -C Release --output-on-failure
```

Windows with MinGW: add `-G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`. Use a complete MinGW installation on PATH (including runtime DLLs). For the plugin, MSVC/Visual Studio 2022 or newer is recommended:

```sh
cmake -S . -B build-plugin -A x64
cmake --build build-plugin --config Release --parallel 2
ctest --test-dir build-plugin -C Release --output-on-failure
```

macOS: use Xcode command-line tools. Linux plugin builds also require JUCE's platform development packages (ALSA, X11/Xrandr/Xinerama/Xcursor, FreeType, OpenGL). The plugin build fetches JUCE 7.0.12 at a pinned commit, retaining support for this workspace's MinGW toolchain (JUCE 8 rejects MinGW). For an existing checkout add `-DFETCHCONTENT_SOURCE_DIR_JUCE=/absolute/path/to/JUCE`. MinGW plugin builds use the same compiler options as the core example. VST3 and Standalone artifacts are under `build-plugin/DriftBrigade_artefacts/`; automatic system installation is disabled. Product name is centralized in `DRIFT_PRODUCT_NAME` in CMake and a generated header. JUCE's commercial/GPL licensing applies; this repository does not grant a JUCE commercial license.

## Offline inspection

```sh
mkdir output
build-dsp/drift_analysis output/organic.csv 0.45 1 1
build-dsp/drift_analysis output/locked.csv 1 0.7 0 flange
build-dsp/drift_analysis output/diffuse.csv 0 0.7 0 flange
build-dsp/drift_analysis output/periodic.csv 0.45 0 0
build-dsp/drift_analysis output/dynamics.csv 0.45 0.3 1
build-dsp/drift_analysis output/raw.csv 0.45 1 1 raw-depth
```

On Windows use `build-dsp\drift_analysis.exe`. Each run exports eight seconds of CSV telemetry at 1 kHz and stereo 16-bit WAV at 48 kHz. The input has quiet/loud/quiet sections at 2 and 5 s; WAV output uses a fixed 0.4 gain for headroom, without auto-normalization. CSV audio columns are snapshots, not an audio-rate capture. Use `flange` for coherence A/B with zero Width, equal settings and identical seed. `raw-depth` selects the internal comparison mapping. An isolated DSP timing loop excludes file I/O.

Optional offline plotting: install `matplotlib` in your analysis Python environment and run `python tools/plot_analysis.py output/organic.csv`. This is not a runtime dependency. On Linux/Clang or GCC, use `-DDRIFT_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug` for AddressSanitizer/UBSan; the compiler must include sanitizer runtimes.

See [research notes](docs/research_notes.md), [architecture](docs/architecture.md) and [qualification report](docs/m1_report.md). Hosted CI builds the core on Windows/macOS/Linux, runs sanitizers on Linux and builds/tests the plugin on Windows. Listening and host qualification remain distinct from numerical tests.
