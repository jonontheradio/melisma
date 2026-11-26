# melisma_core: analysis, model, synthesis and DSP.
#
# Design rule: this library is plain C++20 and does NOT depend on JUCE.
#  - It can be unit-tested and profiled without any GUI or host.
#  - It avoids compiling JUCE module sources twice (JUCE modules are compiled
#    into each final binary: plugin, app, tools).
# Adapters that need JUCE (e.g. reading audio files into a core buffer) live in
# the binaries that already link JUCE.

add_library(melisma_core STATIC)
add_library(melisma::core ALIAS melisma_core)

configure_file(
    include/melisma/core/Version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/melisma/core/Version.h"
    @ONLY)

target_sources(melisma_core
    PRIVATE
        src/Version.cpp
        src/analysis/Analysis.cpp
        src/analysis/NoteSegmentation.cpp
        src/analysis/Onsets.cpp
        src/analysis/PitchMarks.cpp
        src/analysis/PitchTrack.cpp
        src/analysis/Pyin.cpp
        src/analysis/Voicing.cpp
        src/analysis/Yin.cpp
        src/audio/MonoAudio.cpp
        src/dsp/Fft.cpp
        src/editor/EditHistory.cpp
        src/editor/Selection.cpp
        src/editor/Snapping.cpp
        src/editor/Viewport.cpp
        src/model/Take.cpp)

target_include_directories(melisma_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/include")

target_compile_features(melisma_core PUBLIC cxx_std_20)

find_package(Threads REQUIRED)
target_link_libraries(melisma_core PUBLIC Threads::Threads)

melisma_set_warnings(melisma_core)
