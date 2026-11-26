#include "PluginEditor.h"

#include "Theme.h"

#include <melisma/core/Version.h>

#include <algorithm>

namespace melisma::plugin
{
namespace
{
constexpr int toolbarHeight = 40;
} // namespace

MelismaEditor::MelismaEditor (MelismaProcessor& p)
    : AudioProcessorEditor (p)
    , melismaProcessor (p)
{
    addAndMakeVisible (pianoRoll);
    for (auto* c : std::initializer_list<juce::Component*>{&openButton, &playButton, &loopButton, &snapBox, &undoButton,
                                                           &redoButton, &fitButton, &status})
    {
        addAndMakeVisible (c);
    }
    addChildComponent (progressBar);

    openButton.onClick = [this] { chooseFile(); };
    playButton.onClick = [this] { togglePlay(); };
    loopButton.setToggleState (melismaProcessor.isLooping(), juce::dontSendNotification);
    loopButton.onClick = [this] { melismaProcessor.setLooping (loopButton.getToggleState()); };
    undoButton.onClick = [this] { pianoRoll.undo(); };
    redoButton.onClick = [this] { pianoRoll.redo(); };
    fitButton.onClick = [this] { pianoRoll.fitToContent(); };

    snapBox.addItem ("Snap: semitone", 1);
    snapBox.addItem ("Snap: free", 2);
    snapBox.setSelectedId (1, juce::dontSendNotification);
    snapBox.onChange = [this]
    { pianoRoll.setPitchSnap (snapBox.getSelectedId() == 1 ? editor::PitchSnap::semitone : editor::PitchSnap::free); };

    status.setColour (juce::Label::textColourId, ui::theme::textDim);
    status.setText (juce::String ("Melisma v") + juce::String (std::string (libraryVersionString())),
                    juce::dontSendNotification);

    pianoRoll.onEdit = [this] { updateButtons(); };
    pianoRoll.onSeek = [this] (double seconds) { melismaProcessor.seek (seconds); };
    pianoRoll.setTake (melismaProcessor.currentTake());
    if (melismaProcessor.currentTake() != nullptr)
    {
        status.setText (melismaProcessor.loadedFileName(), juce::dontSendNotification);
    }

    setResizable (true, true);
    setResizeLimits (640, 360, 3200, 2000);
    setSize (1100, 640);
    updateButtons();
    startTimerHz (30);
}

MelismaEditor::~MelismaEditor()
{
    stopTimer();
}

void MelismaEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::theme::panel);
}

void MelismaEditor::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (toolbarHeight).reduced (8, 6);
    openButton.setBounds (bar.removeFromLeft (80));
    bar.removeFromLeft (12);
    playButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (6);
    loopButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (12);
    snapBox.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (12);
    undoButton.setBounds (bar.removeFromLeft (56));
    bar.removeFromLeft (4);
    redoButton.setBounds (bar.removeFromLeft (56));
    bar.removeFromLeft (12);
    fitButton.setBounds (bar.removeFromLeft (48));
    bar.removeFromLeft (12);
    progressBar.setBounds (bar.removeFromRight (180));
    status.setBounds (bar);
    pianoRoll.setBounds (area);
}

bool MelismaEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlay();
        return true;
    }
    return false;
}

bool MelismaEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::ranges::any_of (files,
                                [] (const juce::String& f)
                                {
                                    return f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif")
                                           || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac");
                                });
}

void MelismaEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (!files.isEmpty())
    {
        openFile (juce::File (files[0]));
    }
}

void MelismaEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open audio", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                              {
                                  openFile (fc.getResult());
                              }
                          });
}

void MelismaEditor::openFile (const juce::File& file)
{
    pianoRoll.setTake (nullptr);
    const auto error = melismaProcessor.loadFile (file);
    if (error.isNotEmpty())
    {
        status.setText (error, juce::dontSendNotification);
        return;
    }
    status.setText ("Analysing " + file.getFileName() + "...", juce::dontSendNotification);
    updateButtons();
}

void MelismaEditor::togglePlay()
{
    melismaProcessor.setPlaying (!melismaProcessor.isPlaying());
    updateButtons();
}

void MelismaEditor::updateButtons()
{
    playButton.setButtonText (melismaProcessor.isPlaying() ? "Stop" : "Play");
    undoButton.setEnabled (pianoRoll.history().canUndo());
    redoButton.setEnabled (pianoRoll.history().canRedo());
    undoButton.setTooltip (juce::String (pianoRoll.history().undoName()));
    redoButton.setTooltip (juce::String (pianoRoll.history().redoName()));
}

void MelismaEditor::timerCallback()
{
    if (melismaProcessor.pollAnalysis())
    {
        auto* take = melismaProcessor.currentTake();
        pianoRoll.setTake (take);
        status.setText (melismaProcessor.loadedFileName() + "  -  " + juce::String (take->notes().size()) + " notes",
                        juce::dontSendNotification);
        updateButtons();
    }

    progress = melismaProcessor.analysisProgress();
    progressBar.setVisible (progress >= 0.0);

    pianoRoll.setPlayheadSeconds (melismaProcessor.playheadSeconds());
    if (playButton.getButtonText() == "Stop" && !melismaProcessor.isPlaying())
    {
        updateButtons(); // playback reached the end
    }
}
} // namespace melisma::plugin
