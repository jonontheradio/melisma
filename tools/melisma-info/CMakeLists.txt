# melisma-info: tiny CLI that prints Melisma and JUCE versions.
# It also acts as a smoke test that JUCE is fetched and builds correctly.
juce_add_console_app(melisma-info PRODUCT_NAME "melisma-info")

target_sources(melisma-info PRIVATE Main.cpp)

target_compile_definitions(melisma-info PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    MELISMA_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(melisma-info
    PRIVATE
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

melisma_set_warnings(melisma-info)
