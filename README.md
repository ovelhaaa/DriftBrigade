# DriftBrigade

DriftBrigade is a four-band organic modulated delay for mono and stereo, available as a VST3 and a standalone application. The public build uses DigitalFractional, Wander modulation and the Gentle bank. It offers Motion, Depth, Center, Chaos, Coherence, Dynamics, Feedback, Mix and Width. Balanced1024 BBD is an internal experiment with bounded-clock admission and Digital fallback.

## Build and test

Use CMake 3.22+ and a C++17 compiler.

```sh
cmake -S . -B build-dsp -DDRIFT_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --config Release --parallel 2
ctest --test-dir build-dsp -C Release --output-on-failure
```

For VST3 and Standalone, configure with `cmake -S . -B build-plugin`, then run the same build and CTest commands with `build-plugin`. Windows MSVC users can add `-A x64`. MinGW users can add `-G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++` and put the toolchain runtime on PATH. macOS needs Xcode command-line tools. Linux needs JUCE's ALSA, X11/Xrandr/Xinerama/Xcursor, FreeType and OpenGL development libraries. CMake fetches a pinned JUCE 7.0.12 checkout. An existing checkout can be supplied through `FETCHCONTENT_SOURCE_DIR_JUCE`. JUCE's commercial/GPL licensing applies. The plugin bundles are written under `build-plugin/DriftBrigade_artefacts/`.

See the [documentation and validation index](docs/README.md) for milestones, regression checks, listening packages and performance tools. Base changes on the latest integrated milestone, run the relevant local checks and submit a PR. Keep measurements and blind listening outputs in build or output directories, and use the GitHub Actions artifacts for sharing.
