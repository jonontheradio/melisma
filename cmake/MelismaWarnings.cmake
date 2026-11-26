# melisma_set_warnings(<target>)
# Strict warnings for OUR source files only. JUCE compiles its module sources
# inside our targets, so flags are attached per source file instead of to the
# whole target, keeping third-party code out of it.
# Call it after target_sources().
function(melisma_set_warnings target)
    if(MSVC)
        set(flags /W4 /permissive-)
    else()
        set(flags -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                  -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
    endif()

    get_target_property(sources ${target} SOURCES)
    get_target_property(source_dir ${target} SOURCE_DIR)
    foreach(src IN LISTS sources)
        cmake_path(ABSOLUTE_PATH src BASE_DIRECTORY "${source_dir}")
        cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${src}" NORMALIZE is_ours)
        if(is_ours AND NOT src MATCHES "/_deps/")
            set_property(SOURCE "${src}" APPEND PROPERTY COMPILE_OPTIONS ${flags})
        endif()
    endforeach()
endfunction()
