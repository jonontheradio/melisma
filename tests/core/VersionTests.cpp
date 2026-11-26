#include <melisma/core/Version.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE ("Version string matches its numeric parts", "[core][version]")
{
    const auto expected = std::to_string (melisma::versionMajor) + "." + std::to_string (melisma::versionMinor) + "."
                          + std::to_string (melisma::versionPatch);

    CHECK (melisma::versionString == expected);
}

TEST_CASE ("Compiled library matches the headers", "[core][version]")
{
    CHECK (melisma::libraryVersionString() == melisma::versionString);
}
