#pragma once

#include "PianoRoll.h"
#include "PluginProcessor.h"

#include <memory>

namespace melisma::plugin
{
/** Main window: toolbar (open, transport, snap, undo/redo, fit) and the piano roll. */
class MelismaEditor final : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit MelismaEditor (MelismaProcessor&);
    ~MelismaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void openFile (const juce::File& file);
    void chooseFile();
    void togglePlay();
    void updateButtons();

    MelismaProcessor& melismaProcessor;
    ui::PianoRoll pianoRoll;

    juce::TextButton openButton{"Open..."};
    juce::TextButton playButton{"Play"};
    juce::ToggleButton loopButton{"Loop"};
    juce::ComboBox snapBox;
    juce::TextButton undoButton{"Undo"};
    juce::TextButton redoButton{"Redo"};
    juce::TextButton fitButton{"Fit"};
    juce::Label status;
    double progress = -1.0;
    juce::ProgressBar progressBar{progress};

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelismaEditor)
};
} // namespace melisma::plugin
