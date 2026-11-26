# Melisma plugin: VST3 / AU / Standalone, built from one AudioProcessor.

option(MELISMA_COPY_PLUGIN_AFTER_BUILD
    "Copy the VST3/AU bundles to ~/Library/Audio/Plug-Ins after building" OFF)

juce_add_plugin(Melisma
    PRODUCT_NAME              "Melisma"
    VERSION                   "${PROJECT_VERSION}"
    COMPANY_NAME              "Melisma"
    COMPANY_WEBSITE           "https://github.com/jonontheradio/melisma"
    BUNDLE_ID                 "io.github.jonontheradio.melisma"
    PLUGIN_MANUFACTURER_CODE  Mlsm      # 4 chars, at least one uppercase
    PLUGIN_CODE               Mls1      # 4 chars, unique per manufacturer
    FORMATS                   VST3 AU Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES           Fx Pitch Shift
    AU_MAIN_TYPE              kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Melisma needs audio input to capture and edit vocals."
    COPY_PLUGIN_AFTER_BUILD   ${MELISMA_COPY_PLUGIN_AFTER_BUILD})

target_sources(Melisma
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        ${MELISMA_UI_SOURCES})

target_include_directories(Melisma PRIVATE "${MELISMA_UI_INCLUDE_DIR}")

target_compile_definitions(Melisma
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Melisma
    PRIVATE
        melisma::juce_adapters # brings melisma::core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

melisma_set_warnings(Melisma)
