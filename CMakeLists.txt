cmake_minimum_required(VERSION 3.25)

project(Melisma
    VERSION 0.1.0
    DESCRIPTION "Open-source note-based pitch & time editor"
    HOMEPAGE_URL "https://github.com/jonontheradio/melisma"
    LANGUAGES C CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use: cmake -B build")
endif()

# ---- Global settings ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# macOS: minimum deployment target (must be set before any target is created)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# ---- Options -------------------------------------------------------------------
option(MELISMA_BUILD_PLUGIN "Build the VST3/AU/Standalone plugin" ON)
option(MELISMA_BUILD_TESTS  "Build the unit tests"                ON)
option(MELISMA_BUILD_TOOLS  "Build command-line tools"            ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(MelismaWarnings)
include(MelismaDependencies)

# ---- Subprojects -------------------------------------------------------------
add_subdirectory(core)        # DSP + model library, no GUI       (#12)
add_subdirectory(ui)          # JUCE components: piano roll, blobs (M2)

if(MELISMA_BUILD_PLUGIN)
    add_subdirectory(plugin)  # VST3 / AU / Standalone            (#13)
    add_subdirectory(app)
endif()

if(MELISMA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)   # Catch2 + CTest                    (#14)
endif()

if(MELISMA_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
