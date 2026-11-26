#pragma once

#include "PluginProcessor.h"

namespace melisma::plugin
{
/** Placeholder editor: shows the product name and version.
    The piano roll arrives in milestone M2. */
class MelismaEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MelismaEditor (MelismaProcessor&);
    ~MelismaEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelismaEditor)
};
} // namespace melisma::plugin
