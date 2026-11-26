#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace melisma::plugin
{
MelismaProcessor::MelismaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MelismaProcessor::prepareToPlay (double, int) {}

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

    // Pass-through: the input already sits in the buffer. Only clear output
    // channels that have no matching input, so they never carry garbage.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
    {
        buffer.clear (ch, 0, buffer.getNumSamples());
    }
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
