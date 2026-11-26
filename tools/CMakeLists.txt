add_subdirectory(melisma-info)
add_subdirectory(melisma-eval)
if(MELISMA_BUILD_PLUGIN)
    add_subdirectory(melisma-snapshot) # needs the UI sources
endif()
