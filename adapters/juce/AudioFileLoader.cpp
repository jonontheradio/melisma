#include "AudioFileLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>
#include <span>
#include <vector>

namespace melisma::adapters
{
std::optional<LoadedAudio> loadAudioFile (const juce::File& file, juce::String& error)
{
    if (!file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return std::nullopt;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats(); // WAV, AIFF, FLAC, Ogg (+ CoreAudio formats on macOS)

    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + file.getFileName();
        return std::nullopt;
    }

    constexpr juce::int64 maxSeconds = juce::int64{60} * 60; // 1 hour; protects against huge files
    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0
        || reader->lengthInSamples > static_cast<juce::int64> (reader->sampleRate) * maxSeconds)
    {
        error = "Audio file is empty or too long: " + file.getFileName();
        return std::nullopt;
    }

    const auto numChannels = static_cast<int> (reader->numChannels);
    const auto length = static_cast<int> (reader->lengthInSamples);

    juce::AudioBuffer<float> buffer (numChannels, length);
    if (!reader->read (&buffer, 0, length, 0, true, true))
    {
        error = "Could not read audio data from: " + file.getFileName();
        return std::nullopt;
    }

    std::vector<std::span<const float>> channels;
    channels.reserve (static_cast<std::size_t> (numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
    {
        channels.emplace_back (buffer.getReadPointer (ch), static_cast<std::size_t> (length));
    }

    LoadedAudio result;
    result.original = {reader->sampleRate, numChannels, reader->lengthInSamples, reader->getFormatName()};
    result.audio = audio::prepareForAnalysis (channels, reader->sampleRate);
    return result;
}
} // namespace melisma::adapters
