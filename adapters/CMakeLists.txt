# melisma_juce_adapters: thin glue between JUCE and the JUCE-free core.
#
# It is an INTERFACE library on purpose: like JUCE's own modules, its sources
# compile inside each consumer (plugin, tools) with that consumer's JUCE
# configuration, so there is exactly one copy of JUCE per binary.
add_library(melisma_juce_adapters INTERFACE)
add_library(melisma::juce_adapters ALIAS melisma_juce_adapters)

target_sources(melisma_juce_adapters
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/juce/AudioFileLoader.cpp")

target_include_directories(melisma_juce_adapters
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/juce")

target_link_libraries(melisma_juce_adapters
    INTERFACE
        melisma::core
        juce::juce_audio_formats)
