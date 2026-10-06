cmake_minimum_required(VERSION 3.22)
project(DriftBrigade VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(DRIFT_PRODUCT_NAME "DRIFT BRIGADE" CACHE STRING "Provisional product name")
configure_file(Source/ProductConfig.h.in generated/ProductConfig.h @ONLY)
option(DRIFT_BUILD_PLUGIN "Build JUCE VST3 and Standalone" ON)
option(DRIFT_SANITIZE "Enable address/undefined sanitizers (Clang/GCC)" OFF)
add_library(drift_dsp Source/dsp/DriftEngine.cpp Source/dsp/OrganicModulator.cpp Source/dsp/ClockedBBDCore.cpp)
target_include_directories(drift_dsp PUBLIC Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
  target_compile_options(drift_dsp PRIVATE /W4)
else()
  target_compile_options(drift_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(DRIFT_SANITIZE AND NOT MSVC)
  target_compile_options(drift_dsp PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(drift_dsp PUBLIC -fsanitize=address,undefined)
endif()
enable_testing()
add_executable(drift_tests tests/DspTests.cpp)
target_link_libraries(drift_tests PRIVATE drift_dsp)
add_test(NAME dsp_qualification COMMAND drift_tests)
add_executable(drift_bbd_tests tests/BBDTests.cpp)
target_link_libraries(drift_bbd_tests PRIVATE drift_dsp)
add_test(NAME bbd_transport_qualification COMMAND drift_bbd_tests transport)
add_test(NAME bbd_async_filter_qualification COMMAND drift_bbd_tests async)
add_test(NAME bbd_aliasing_and_hold_qualification COMMAND drift_bbd_tests spectral)
add_executable(drift_analysis tools/Analysis.cpp)
target_link_libraries(drift_analysis PRIVATE drift_dsp)
add_executable(drift_bakeoff tools/Bakeoff.cpp)
target_link_libraries(drift_bakeoff PRIVATE drift_dsp)
add_executable(drift_bbd_qualification tools/BBDQualification.cpp)
target_link_libraries(drift_bbd_qualification PRIVATE drift_dsp)
add_test(NAME bbd_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-stream-tests
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)
add_executable(drift_bbd_character_tests tests/BBDCharacterTests.cpp)
target_link_libraries(drift_bbd_character_tests PRIVATE drift_dsp)
add_test(NAME bbd_device_character_qualification COMMAND drift_bbd_character_tests)
add_executable(drift_bbd_character_qualification tools/BBDCharacterQualification.cpp)
target_link_libraries(drift_bbd_character_qualification PRIVATE drift_dsp)
add_test(NAME bbd_device_character_measurements COMMAND drift_bbd_character_qualification
  ${CMAKE_CURRENT_BINARY_DIR}/bbd-character-measurements)
add_test(NAME bbd_character_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_character_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-character-stream-tests
  -DBLOCKED_ARTIFACT=bbd_device_gain.csv
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)
if(DRIFT_BUILD_PLUGIN)
  include(FetchContent)
  # JUCE 7.0.12 retains MinGW support for this workspace's native toolchain.
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 4f43011b96eb0636104cb3e433894cda98243626)
  FetchContent_MakeAvailable(JUCE)
  juce_add_plugin(DriftBrigade COMPANY_NAME "Drift Audio" BUNDLE_ID "audio.driftbrigade.m1"
    PLUGIN_MANUFACTURER_CODE Drft PLUGIN_CODE DrBg FORMATS VST3 Standalone
    PRODUCT_NAME "${DRIFT_PRODUCT_NAME}" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(DriftBrigade PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/params/Parameters.cpp)
  target_include_directories(DriftBrigade PUBLIC Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_compile_definitions(DriftBrigade PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(DriftBrigade PRIVATE drift_dsp juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
  add_executable(drift_plugin_tests tests/PluginTests.cpp)
  target_link_libraries(drift_plugin_tests PRIVATE DriftBrigade juce::juce_audio_utils)
  add_test(NAME plugin_state_and_routing COMMAND drift_plugin_tests)
endif()

# Separate instrumented DSP compilation: no reference or counters in drift_dsp.
add_executable(drift_bbd_performance_qualification tools/BBDPerformanceQualification.cpp
  Source/dsp/ClockedBBDCore.cpp tests/reference/ClockedBBDCore.cpp)
target_include_directories(drift_bbd_performance_qualification PRIVATE Source)
target_compile_definitions(drift_bbd_performance_qualification PRIVATE DRIFT_BBD_INSTRUMENT=1)
if(DRIFT_SANITIZE AND NOT MSVC)
  target_compile_options(drift_bbd_performance_qualification PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(drift_bbd_performance_qualification PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME bbd_performance_numerical COMMAND drift_bbd_performance_qualification
  ${CMAKE_CURRENT_BINARY_DIR}/bbd-performance-numerical --numerical-only)
add_test(NAME bbd_performance_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_performance_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-performance-stream-tests
  -DBLOCKED_ARTIFACT=async_filter_reference_error.csv
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)

# MinGW Clang native TLS for inline thread_local counters fails with this GNU runtime.
# Confine emulated TLS to the allocation-test executables.
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  foreach(test_target drift_tests drift_bbd_tests drift_bbd_character_tests drift_bbd_performance_qualification drift_plugin_tests)
    if(TARGET ${test_target})
      target_compile_options(${test_target} PRIVATE -femulated-tls)
    endif()
  endforeach()
endif()

add_executable(drift_bbd_nonlinear_qualification tools/BBDNonlinearQualification.cpp
  tests/reference_m23/ClockedBBDCore.cpp)
target_link_libraries(drift_bbd_nonlinear_qualification PRIVATE drift_dsp)
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(drift_bbd_nonlinear_qualification PRIVATE -femulated-tls)
endif()
add_test(NAME bbd_nonlinear_numerical COMMAND drift_bbd_nonlinear_qualification --tests-only)
add_test(NAME bbd_nonlinear_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_nonlinear_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-nonlinear-stream-tests
  -DBLOCKED_ARTIFACT=bbd_nonlinear_transfer_curve.csv
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)

# Instrumentation and frozen M2.4 oracle are confined to qualification.
add_executable(drift_bbd_compander_qualification tools/BBDCompanderQualification.cpp
  Source/dsp/ClockedBBDCore.cpp tests/reference_m24/ClockedBBDCore.cpp)
target_include_directories(drift_bbd_compander_qualification PRIVATE Source)
target_compile_definitions(drift_bbd_compander_qualification PRIVATE DRIFT_BBD_INSTRUMENT=1)
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(drift_bbd_compander_qualification PRIVATE -femulated-tls)
endif()
if(DRIFT_SANITIZE AND NOT MSVC)
  target_compile_options(drift_bbd_compander_qualification PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(drift_bbd_compander_qualification PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME bbd_compander_numerical COMMAND drift_bbd_compander_qualification --tests-only)
add_test(NAME bbd_compander_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_compander_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-compander-stream-tests
  -DBLOCKED_ARTIFACT=bbd_compander_static_ratio.csv
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)

# M2.6 internal operating-domain qualification and immutable merged M2.5 oracle.
add_executable(drift_bbd_headroom_qualification tools/BBDHeadroomQualification.cpp
  Source/dsp/ClockedBBDCore.cpp tests/reference_m25/ClockedBBDCore.cpp)
target_include_directories(drift_bbd_headroom_qualification PRIVATE Source)
target_compile_definitions(drift_bbd_headroom_qualification PRIVATE DRIFT_BBD_INSTRUMENT=1)
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(drift_bbd_headroom_qualification PRIVATE -femulated-tls)
endif()
if(DRIFT_SANITIZE AND NOT MSVC)
  target_compile_options(drift_bbd_headroom_qualification PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(drift_bbd_headroom_qualification PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME bbd_headroom_numerical COMMAND drift_bbd_headroom_qualification --tests-only)
add_test(NAME bbd_headroom_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_headroom_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-headroom-stream-tests
  -DBLOCKED_ARTIFACT=bbd_gain_profiles.csv
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)

# M2.7 actual eight-voice engine, separate measurement-only DSP build.
add_executable(drift_bbd_engine_qualification tools/BBDEngineQualification.cpp
  Source/dsp/DriftEngine.cpp Source/dsp/OrganicModulator.cpp Source/dsp/ClockedBBDCore.cpp
  tests/reference_m26/DriftEngine.cpp)
target_include_directories(drift_bbd_engine_qualification PRIVATE Source)
target_compile_definitions(drift_bbd_engine_qualification PRIVATE DRIFT_BBD_INSTRUMENT=1)
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(drift_bbd_engine_qualification PRIVATE -femulated-tls)
endif()
if(DRIFT_SANITIZE AND NOT MSVC)
  target_compile_options(drift_bbd_engine_qualification PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(drift_bbd_engine_qualification PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME bbd_engine_numerical COMMAND drift_bbd_engine_qualification --tests-only)
add_test(NAME bbd_engine_artifact_stream_safety COMMAND ${CMAKE_COMMAND}
  -DTOOL=$<TARGET_FILE:drift_bbd_engine_qualification>
  -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bbd-engine-stream-tests
  -DBLOCKED_ARTIFACT=bbd_engine_backend_regression.csv
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BBDFailSafe.cmake)
