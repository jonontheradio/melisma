#pragma once

#include <melisma/core/audio/MonoAudio.h>

#include <juce_core/juce_core.h>

#include <optional>

namespace melisma::adapters
{
/** Information about an audio file as stored on disk. */
struct AudioFileInfo
{
    double sampleRate = 0.0;
    int numChannels = 0;
    juce::int64 lengthInSamples = 0;
    juce::String formatName;
};

/** Result of loading a file: the analysis-ready mono audio plus the original
    file properties. */
struct LoadedAudio
{
    audio::MonoAudio audio;
    AudioFileInfo original;
};

/** Reads WAV / AIFF / FLAC with JUCE and converts it to mono at
    audio::analysisSampleRate. Returns nullopt and fills `error` on failure. */
std::optional<LoadedAudio> loadAudioFile (const juce::File& file, juce::String& error);
} // namespace melisma::adapters
