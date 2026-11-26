# Unit tests: Catch2 v3 + CTest.
#
#   cmake --build build && ctest --test-dir build --output-on-failure
#
# Layout mirrors the source tree: tests/core/... tests melisma_core.
# tests/support/ holds helpers shared by the tests (synthetic signals...).

include(Catch)

# ---- Shared test helpers -------------------------------------------------------
add_library(melisma_test_support STATIC
    support/TestSignals.cpp)
target_include_directories(melisma_test_support PUBLIC support)
target_link_libraries(melisma_test_support PUBLIC melisma::core)
melisma_set_warnings(melisma_test_support)

# ---- Test executable -----------------------------------------------------------
add_executable(melisma_tests
    core/PitchTests.cpp
    core/VersionTests.cpp
    support/TestSignalsTests.cpp)

target_link_libraries(melisma_tests
    PRIVATE
        melisma_test_support # brings melisma::core
        Catch2::Catch2WithMain)

melisma_set_warnings(melisma_tests)

# One CTest entry per TEST_CASE, so failures are reported individually.
catch_discover_tests(melisma_tests
    TEST_PREFIX "core."
    PROPERTIES TIMEOUT 60)
