# melisma UI: JUCE components (piano roll, blobs, tools).
#
# The sources are compiled directly into the plugin target (see
# plugin/CMakeLists.txt) so they share its JUCE configuration and get the
# project's strict warnings.
set(MELISMA_UI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NoteEnvelopes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PianoRoll.cpp"
    CACHE INTERNAL "Melisma UI sources")
set(MELISMA_UI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src" CACHE INTERNAL "Melisma UI headers")
