# melisma-eval: runs the analysis on an audio file and writes the pitch track
# and notes as CSV, for inspection and for accuracy evaluation
# (see tools/eval/compare_f0.py).
juce_add_console_app(melisma-eval PRODUCT_NAME "melisma-eval")

target_sources(melisma-eval PRIVATE Main.cpp)

target_compile_definitions(melisma-eval PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(melisma-eval
    PRIVATE
        melisma::juce_adapters # brings melisma::core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

melisma_set_warnings(melisma-eval)
