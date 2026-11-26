# melisma-info: prints Melisma and JUCE versions, and loads an audio file
# given as argument (melisma-info voice.wav) to show how it is imported.
# It also acts as a smoke test that JUCE is fetched and builds correctly.
juce_add_console_app(melisma-info PRODUCT_NAME "melisma-info")

target_sources(melisma-info PRIVATE Main.cpp)

target_compile_definitions(melisma-info PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(melisma-info
    PRIVATE
        melisma::juce_adapters # brings melisma::core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

melisma_set_warnings(melisma-info)
