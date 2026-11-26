#include "AudioFileLoader.h"

#include <melisma/core/Version.h>

#include <juce_core/juce_core.h>

#include <iostream>

int main (int argc, char* argv[])
{
    std::cout << "Melisma " << melisma::libraryVersionString() << '\n'
              << juce::SystemStats::getJUCEVersion() << '\n'
              << juce::SystemStats::getOperatingSystemName() << '\n';

    if (melisma::libraryVersionString() != melisma::versionString)
    {
        std::cerr << "Library/header version mismatch\n";
        return 1;
    }

    if (argc < 2)
    {
        return 0;
    }

    const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    juce::String error;
    const auto loaded = melisma::adapters::loadAudioFile (file, error);
    if (!loaded)
    {
        std::cerr << error << '\n';
        return 1;
    }

    const auto& o = loaded->original;
    std::cout << '\n'
              << file.getFileName() << " (" << o.formatName << ")\n"
              << "  original: " << o.sampleRate << " Hz, " << o.numChannels << " ch, " << o.lengthInSamples
              << " samples\n"
              << "  imported: " << loaded->audio.sampleRate << " Hz mono, " << loaded->audio.size() << " samples ("
              << loaded->audio.durationSeconds() << " s)\n";
    return 0;
}
