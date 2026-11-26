#include "PluginEditor.h"

#include <melisma/core/Version.h>

namespace melisma::plugin
{
MelismaEditor::MelismaEditor (MelismaProcessor& p)
    : AudioProcessorEditor (p)
{
    setResizable (true, true);
    setResizeLimits (480, 320, 2400, 1600);
    setSize (900, 560);
}

void MelismaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    auto area = getLocalBounds().toFloat();

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
    g.drawText ("Melisma", area.removeFromTop (area.getHeight() * 0.55f), juce::Justification::centredBottom);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::FontOptions (15.0f));
    const auto version = juce::String ("v") + juce::String (std::string (melisma::libraryVersionString()));
    g.drawText (version + "  -  pass-through", area.removeFromTop (30.0f), juce::Justification::centred);
}
} // namespace melisma::plugin
