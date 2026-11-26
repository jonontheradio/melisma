#pragma once

#include <melisma/core/analysis/Analysis.h>
#include <melisma/core/audio/MonoAudio.h>
#include <melisma/core/model/Take.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

namespace melisma::plugin
{
/** Melisma's audio processor.

    For now (milestone M2) it can load an audio file, analyse it in the
    background and play the ORIGINAL audio with its own transport, so the
    editor can be tried. Without a file it is a transparent pass-through.
    Playback of the edited take arrives with resynthesis (M3) and capture
    inside the DAW with M5. */
class MelismaProcessor final : public juce::AudioProcessor
{
public:
    MelismaProcessor();
    ~MelismaProcessor() override = default;

    // ---- Message-thread API used by the editor ----
    /** Loads an audio file and starts analysing it. Returns an error message
        on failure (empty on success). */
    juce::String loadFile (const juce::File& file);
    juce::String loadedFileName() const { return fileName; }

    /** Call regularly from the GUI. Returns true once, when a new take is ready. */
    bool pollAnalysis();
    /** 0..1 while analysing, < 0 when idle. */
    double analysisProgress() const;
    model::Take* currentTake() { return take.get(); }

    void setPlaying (bool shouldPlay);
    bool isPlaying() const { return playing.load(); }
    void setLooping (bool shouldLoop) { looping.store (shouldLoop); }
    bool isLooping() const { return looping.load(); }
    void seek (double seconds);
    double playheadSeconds() const { return playhead.load(); }

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    // Owned by the message thread.
    std::unique_ptr<model::Take> take;
    std::unique_ptr<analysis::AnalysisJob> job;
    juce::String fileName;

    // Shared with the audio thread: the buffer is swapped under the lock and
    // the audio thread only try-locks (outputs silence for a block if busy).
    juce::SpinLock playbackLock;
    std::shared_ptr<const audio::MonoAudio> playbackAudio;
    std::atomic<bool> playing{false};
    std::atomic<bool> looping{false};
    std::atomic<double> playhead{0.0};
    std::atomic<double> pendingSeek{-1.0};

    // Audio thread only.
    double readPosition = 0.0; // in source samples
    double deviceRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelismaProcessor)
};
} // namespace melisma::plugin
