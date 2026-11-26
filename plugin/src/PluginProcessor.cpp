#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "AudioFileLoader.h"

#include <utility>

namespace melisma::plugin
{
MelismaProcessor::MelismaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

juce::String MelismaProcessor::loadFile (const juce::File& file)
{
    juce::String error;
    auto loaded = adapters::loadAudioFile (file, error);
    if (!loaded)
    {
        return error;
    }

    setPlaying (false);
    seek (0.0);
    {
        auto audio = std::make_shared<const audio::MonoAudio> (loaded->audio);
        const juce::SpinLock::ScopedLockType lock (playbackLock);
        std::swap (playbackAudio, audio);
        // The previous buffer (now in `audio`) is released here, on the message thread.
    }

    take.reset();
    job = std::make_unique<analysis::AnalysisJob> (std::move (loaded->audio), analysis::AnalysisSettings{});
    fileName = file.getFileName();
    return {};
}

bool MelismaProcessor::pollAnalysis()
{
    if (job == nullptr || !job->isFinished())
    {
        return false;
    }
    auto result = job->takeResult();
    job.reset();
    if (!result)
    {
        return false;
    }
    take = std::make_unique<model::Take> (std::move (*result));
    return true;
}

double MelismaProcessor::analysisProgress() const
{
    return job != nullptr ? job->progress() : -1.0;
}

void MelismaProcessor::setPlaying (bool shouldPlay)
{
    playing.store (shouldPlay);
}

void MelismaProcessor::seek (double seconds)
{
    pendingSeek.store (std::max (0.0, seconds));
    playhead.store (std::max (0.0, seconds));
}

void MelismaProcessor::prepareToPlay (double sampleRate, int)
{
    deviceRate = sampleRate;
}

void MelismaProcessor::releaseResources() {}

bool MelismaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    // Mono or stereo, and the same layout in and out (vocals are usually mono tracks).
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
    {
        return false;
    }

    return layouts.getMainInputChannelSet() == out;
}

void MelismaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
    {
        buffer.clear (ch, 0, numSamples);
    }

    const juce::SpinLock::ScopedTryLockType lock (playbackLock);
    if (!lock.isLocked() || playbackAudio == nullptr)
    {
        return; // no file: pass the input through
    }

    const auto& source = *playbackAudio;
    const double sourceRate = source.sampleRate;
    if (const double seekTo = pendingSeek.exchange (-1.0); seekTo >= 0.0)
    {
        readPosition = seekTo * sourceRate;
    }

    // With a file loaded, Melisma outputs the file instead of its input.
    buffer.clear();
    if (!playing.load() || source.empty())
    {
        return;
    }

    // Linear interpolation from the source rate to the device rate (preview quality).
    const double step = sourceRate / deviceRate;
    const auto length = static_cast<double> (source.size());
    auto* out = buffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        if (readPosition >= length - 1.0)
        {
            if (!looping.load())
            {
                playing.store (false);
                readPosition = 0.0;
                break;
            }
            readPosition = 0.0;
        }
        const auto index = static_cast<std::size_t> (readPosition);
        const auto frac = static_cast<float> (readPosition - static_cast<double> (index));
        out[i] = source.samples[index] + frac * (source.samples[index + 1] - source.samples[index]);
        readPosition += step;
    }
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }
    playhead.store (readPosition / sourceRate);
}

juce::AudioProcessorEditor* MelismaProcessor::createEditor()
{
    return new MelismaEditor (*this);
}

void MelismaProcessor::getStateInformation (juce::MemoryBlock&) {}

void MelismaProcessor::setStateInformation (const void*, int) {}
} // namespace melisma::plugin

// Entry point JUCE uses to create the plugin instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new melisma::plugin::MelismaProcessor();
}
