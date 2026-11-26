#include <melisma/core/Version.h>

#include <juce_core/juce_core.h>

#include <iostream>

int main()
{
    std::cout << "Melisma " << melisma::libraryVersionString() << '\n'
              << juce::SystemStats::getJUCEVersion() << '\n'
              << juce::SystemStats::getOperatingSystemName() << '\n';

    return melisma::libraryVersionString() == melisma::versionString ? 0 : 1;
}
