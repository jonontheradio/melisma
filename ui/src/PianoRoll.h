#pragma once

#include "NoteEnvelopes.h"

#include <melisma/core/editor/EditHistory.h>
#include <melisma/core/editor/Selection.h>
#include <melisma/core/editor/Snapping.h>
#include <melisma/core/editor/Viewport.h>
#include <melisma/core/model/Take.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <optional>

namespace melisma::ui
{
/** The note editor: piano keyboard on the left, time ruler on top, and the
    note area with grid, blobs, pitch-drift curves and the playhead.

    Mouse:
      click a note           select it (shift: add/remove)
      drag on empty space    rubber-band selection (shift: add)
      drag a note up/down    change pitch, snapped to semitones (alt: free)
      cmd/ctrl + drag        move notes in time
      drag a note's edge     stretch it in time
      double-click empty     clear selection
      wheel / trackpad       scroll; cmd/ctrl + wheel zooms time, alt + wheel zooms pitch
      click the ruler        move the playhead
    Keys: cmd+Z / shift+cmd+Z undo/redo, cmd+A select all, Escape cancels a drag
    or clears the selection, arrow up/down nudge pitch by a semitone (shift: 10 cents). */
class PianoRoll final : public juce::Component
{
public:
    PianoRoll();

    /** The take to edit (nullptr = nothing loaded). Not owned. */
    void setTake (model::Take* take);
    void fitToContent();

    editor::EditHistory& history() { return editHistory; }
    editor::Selection& selection() { return noteSelection; }

    void setPitchSnap (editor::PitchSnap snap) { pitchSnap = snap; }
    editor::PitchSnap getPitchSnap() const { return pitchSnap; }

    /** Playhead in source seconds (drawn; set by the owner from playback). */
    void setPlayheadSeconds (double seconds);

    void undo();
    void redo();

    /** Called after any edit or undo/redo. */
    std::function<void()> onEdit;
    /** Called when the user clicks the ruler to move the playhead. */
    std::function<void (double seconds)> onSeek;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int keyboardWidth = 56;
    static constexpr int rulerHeight = 24;

private:
    enum class DragMode : std::uint8_t
    {
        none,
        rubberBand,
        pitch,
        time,
        stretchStart,
        stretchEnd,
        seek,
    };

    juce::Rectangle<int> noteArea() const;
    juce::Point<double> toArea (juce::Point<float> p) const;
    const model::Note* noteAtPoint (juce::Point<float> p) const;
    juce::Rectangle<float> noteBounds (const model::Note&) const;
    DragMode modeForPress (const juce::MouseEvent&, const model::Note&) const;
    void editsChanged();
    void nudgeSelection (double cents);

    void paintGrid (juce::Graphics&) const;
    void paintNotes (juce::Graphics&) const;
    void paintKeyboard (juce::Graphics&) const;
    void paintRuler (juce::Graphics&) const;

    model::Take* take = nullptr;
    NoteEnvelopes envelopes;
    editor::Viewport viewport;
    editor::Selection noteSelection;
    editor::EditHistory editHistory;
    editor::PitchSnap pitchSnap = editor::PitchSnap::semitone;
    double playheadSeconds = 0.0;

    // Drag state
    DragMode dragMode = DragMode::none;
    editor::EditSnapshot dragSnapshot;
    std::optional<model::NoteId> dragNote;
    model::NoteEdits dragNoteStartEdits;
    double dragStartMidi = 0.0;
    double dragStartSeconds = 0.0;
    juce::Rectangle<float> rubberBand;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
} // namespace melisma::ui
