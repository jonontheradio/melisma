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
