# Unit tests: Catch2 v3 + CTest.
#
#   cmake --build build && ctest --test-dir build --output-on-failure
#
# Layout mirrors the source tree: tests/core/... tests melisma_core.

include(Catch)

add_executable(melisma_tests
    core/VersionTests.cpp)

target_link_libraries(melisma_tests
    PRIVATE
        melisma::core
        Catch2::Catch2WithMain)

melisma_set_warnings(melisma_tests)

# One CTest entry per TEST_CASE, so failures are reported individually.
catch_discover_tests(melisma_tests
    TEST_PREFIX "core."
    PROPERTIES TIMEOUT 60)
