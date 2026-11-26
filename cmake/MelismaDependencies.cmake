# Third-party dependencies, fetched at configure time and pinned to an exact
# version + SHA-256 so builds are reproducible.

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---- JUCE (AGPLv3) -------------------------------------------------------------
set(MELISMA_JUCE_VERSION "9.0.6")
FetchContent_Declare(JUCE
    URL      https://github.com/juce-framework/JUCE/archive/refs/tags/${MELISMA_JUCE_VERSION}.tar.gz
    URL_HASH SHA256=103e6c9d5f17f19bc7b5485a890573609d326513aad4db6b045181f4936ef424
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM            # its headers are not subject to our warnings
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(JUCE)

# ---- Catch2 (BSL-1.0), tests only ---------------------------------------------
if(MELISMA_BUILD_TESTS)
    FetchContent_Declare(Catch2
        URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz
        URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SYSTEM
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()
