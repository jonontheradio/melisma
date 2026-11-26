#include <juce_core/juce_core.h>

#include <iostream>

int main()
{
    std::cout << "Melisma " << MELISMA_VERSION_STRING << '\n'
              << juce::SystemStats::getJUCEVersion() << '\n'
              << juce::SystemStats::getOperatingSystemName() << '\n';
    return 0;
}
