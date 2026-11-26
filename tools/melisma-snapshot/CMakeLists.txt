# melisma-snapshot: analyses an audio file and renders the piano roll to a PNG,
# without opening a window. Used to check the editor's drawing.
juce_add_console_app(melisma-snapshot PRODUCT_NAME "melisma-snapshot")

target_sources(melisma-snapshot PRIVATE Main.cpp ${MELISMA_UI_SOURCES})
target_include_directories(melisma-snapshot PRIVATE "${MELISMA_UI_INCLUDE_DIR}")

target_compile_definitions(melisma-snapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(melisma-snapshot
    PRIVATE
        melisma::juce_adapters # brings melisma::core
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

melisma_set_warnings(melisma-snapshot)
