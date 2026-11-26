#include "PianoRoll.h"

#include "Theme.h"

#include <melisma/core/Pitch.h>

#include <algorithm>
#include <cmath>

namespace melisma::ui
{
namespace
{
bool isBlackKey (int midi)
{
    const int pc = ((midi % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

/** A "nice" grid step in seconds giving at least minPixels between lines. */
double timeGridStep (double pixelsPerSecond, double minPixels)
{
    for (const double step :
         {0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0})
    {
        if (step * pixelsPerSecond >= minPixels)
        {
            return step;
        }
    }
    return 600.0;
}

juce::String timeLabel (double seconds, double step)
{
    if (seconds >= 60.0)
    {
        const int minutes = static_cast<int> (seconds / 60.0);
        return juce::String (minutes) + ":"
               + juce::String (seconds - minutes * 60.0, step < 1.0 ? 1 : 0).paddedLeft ('0', step < 1.0 ? 4 : 2);
    }
    int decimals = 0;
    if (step < 0.1)
    {
        decimals = 2;
    }
    else if (step < 1.0)
    {
        decimals = 1;
    }
    return juce::String (seconds, decimals) + "s";
}

constexpr float edgeGrabPixels = 6.0f;
constexpr double minStretch = 0.25;
constexpr double maxStretch = 4.0;
} // namespace

PianoRoll::PianoRoll()
{
    setWantsKeyboardFocus (true);
    setOpaque (true);
}

void PianoRoll::setTake (model::Take* newTake)
{
    take = newTake;
    noteSelection.clear();
    editHistory.clear();
    if (take != nullptr)
    {
        envelopes.build (*take);
        viewport.setContentDuration (take->source().durationSeconds());
        fitToContent();
    }
    else
    {
        envelopes.clear();
    }
    repaint();
}

void PianoRoll::fitToContent()
{
    if (take == nullptr || take->notes().empty())
    {
        viewport.setVisibleRange (0.0, take != nullptr ? take->source().durationSeconds() : 10.0, 48.0, 72.0);
        repaint();
        return;
    }
    double low = 127.0;
    double high = 0.0;
    for (const auto& n : take->notes())
    {
        low = std::min (low, n.targetMidi());
        high = std::max (high, n.targetMidi());
    }
    const double centre = 0.5 * (low + high);
    const double span = std::max (12.0, high - low + 6.0);
    viewport.setVisibleRange (0.0, take->source().durationSeconds(), centre - 0.5 * span, centre + 0.5 * span);
    repaint();
}

void PianoRoll::setPlayheadSeconds (double seconds)
{
    if (std::abs (seconds - playheadSeconds) * viewport.pixelsPerSecond() < 0.5)
    {
        return;
    }
    playheadSeconds = seconds;
    repaint (noteArea().withY (0).withHeight (getHeight()));
}

void PianoRoll::undo()
{
    if (take != nullptr && editHistory.undo (*take))
    {
        editsChanged();
    }
}

void PianoRoll::redo()
{
    if (take != nullptr && editHistory.redo (*take))
    {
        editsChanged();
    }
}

void PianoRoll::editsChanged()
{
    repaint();
    if (onEdit)
    {
        onEdit();
    }
}

// ---- Geometry ---------------------------------------------------------------

juce::Rectangle<int> PianoRoll::noteArea() const
{
    return getLocalBounds().withTrimmedLeft (keyboardWidth).withTrimmedTop (rulerHeight);
}

void PianoRoll::resized()
{
    const auto area = noteArea();
    viewport.setSize (area.getWidth(), area.getHeight());
}

juce::Point<double> PianoRoll::toArea (juce::Point<float> p) const
{
    const auto area = noteArea();
    return {static_cast<double> (p.x) - area.getX(), static_cast<double> (p.y) - area.getY()};
}

juce::Rectangle<float> PianoRoll::noteBounds (const model::Note& n) const
{
    const auto area = noteArea().toFloat();
    const auto x0 = static_cast<float> (viewport.timeToX (n.editedStartSeconds())) + area.getX();
    const auto x1 = static_cast<float> (viewport.timeToX (n.editedEndSeconds())) + area.getX();
    const auto halfRow = static_cast<float> (0.5 * viewport.pixelsPerSemitone());
    const auto y = static_cast<float> (viewport.midiToY (n.targetMidi())) + area.getY();
    return {x0, y - halfRow, std::max (2.0f, x1 - x0), 2.0f * halfRow};
}

const model::Note* PianoRoll::noteAtPoint (juce::Point<float> p) const
{
    if (take == nullptr)
    {
        return nullptr;
    }
    // Topmost = last drawn; selected notes are drawn last.
    const model::Note* hit = nullptr;
    for (const auto& n : take->notes())
    {
        if (noteBounds (n).expanded (2.0f, 0.0f).contains (p) && (hit == nullptr || noteSelection.contains (n.id)))
        {
            hit = &n;
        }
    }
    return hit;
}

PianoRoll::DragMode PianoRoll::modeForPress (const juce::MouseEvent& e, const model::Note& n) const
{
    const auto b = noteBounds (n);
    if (b.getWidth() > 3.0f * edgeGrabPixels)
    {
        if (e.position.x - b.getX() < edgeGrabPixels)
        {
            return DragMode::stretchStart;
        }
        if (b.getRight() - e.position.x < edgeGrabPixels)
        {
            return DragMode::stretchEnd;
        }
    }
    return e.mods.isCommandDown() ? DragMode::time : DragMode::pitch;
}

// ---- Painting -----------------------------------------------------------------

void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (noteArea());
        paintGrid (g);
        paintNotes (g);

        if (!rubberBand.isEmpty())
        {
            g.setColour (theme::rubberBand);
            g.fillRect (rubberBand);
            g.setColour (theme::blob);
            g.drawRect (rubberBand, 1.0f);
        }

        const auto area = noteArea();
        const auto x = static_cast<float> (viewport.timeToX (playheadSeconds)) + static_cast<float> (area.getX());
        g.setColour (theme::playhead);
        g.drawLine (x, static_cast<float> (area.getY()), x, static_cast<float> (area.getBottom()), 1.5f);
    }
    paintKeyboard (g);
    paintRuler (g);

    if (take == nullptr)
    {
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (16.0f));
        g.drawText ("Open or drop an audio file to start", noteArea(), juce::Justification::centred);
    }
}

void PianoRoll::paintGrid (juce::Graphics& g) const
{
    const auto area = noteArea().toFloat();
    const int lo = static_cast<int> (std::floor (viewport.lowMidi()));
    const int hi = static_cast<int> (std::ceil (viewport.highMidi()));

    for (int m = lo; m <= hi; ++m)
    {
        const auto yTop = static_cast<float> (viewport.midiToY (m + 0.5)) + area.getY();
        const auto yBottom = static_cast<float> (viewport.midiToY (m - 0.5)) + area.getY();
        if (isBlackKey (m))
        {
            g.setColour (theme::blackKeyRow);
            g.fillRect (juce::Rectangle<float> (area.getX(), yTop, area.getWidth(), yBottom - yTop));
        }
        g.setColour (m % 12 == 0 ? theme::gridOctave : theme::gridLine);
        g.drawHorizontalLine (static_cast<int> (yBottom), area.getX(), area.getRight());
    }

    const double step = timeGridStep (viewport.pixelsPerSecond(), 80.0);
    g.setColour (theme::gridLine);
    const auto firstLine = static_cast<long> (std::ceil (viewport.startSeconds() / step));
    for (auto k = firstLine; static_cast<double> (k) * step <= viewport.endSeconds(); ++k)
    {
        const double t = static_cast<double> (k) * step;
        const auto x = static_cast<float> (viewport.timeToX (t)) + area.getX();
        g.drawVerticalLine (static_cast<int> (x), area.getY(), area.getBottom());
    }
}

void PianoRoll::paintNotes (juce::Graphics& g) const
{
    if (take == nullptr)
    {
        return;
    }
    const auto area = noteArea().toFloat();
    const double pps = viewport.pixelsPerSemitone();
    const auto maxHalf = static_cast<float> (std::clamp (0.48 * pps, 3.0, 40.0));
    const auto& track = take->pitch();

    const auto drawNote = [&] (const model::Note& n)
    {
        const auto b = noteBounds (n);
        if (b.getRight() < area.getX() || b.getX() > area.getRight())
        {
            return;
        }
        const bool selected = noteSelection.contains (n.id);
        auto colour = n.edits.isNeutral() ? theme::blob : theme::blobEdited;
        if (selected)
        {
            colour = theme::blobSelected;
        }
        const float gain =
            n.edits.muted ? 0.0f : std::sqrt (std::pow (10.0f, static_cast<float> (n.edits.gainDb) / 20.0f));
        const auto centreY = static_cast<float> (viewport.midiToY (n.targetMidi())) + area.getY();
        const auto toX = [&] (double sourceSeconds)
        {
            const double edited = n.editedStartSeconds() + (sourceSeconds - n.startSeconds) * n.edits.timeStretch;
            return static_cast<float> (viewport.timeToX (edited)) + area.getX();
        };

        // Blob: the amplitude envelope mirrored around the note's pitch line.
        const auto& env = envelopes.of (n.id);
        juce::Path blob;
        if (env.size() >= 2)
        {
            const auto pointAt = [&] (std::size_t i, float sign)
            {
                const double t = n.startSeconds + static_cast<double> (i) * NoteEnvelopes::blockSeconds;
                const float half = std::max (1.0f, maxHalf * std::min (1.0f, env[i] * gain));
                return juce::Point<float> (toX (t), centreY + sign * half);
            };
            blob.startNewSubPath (pointAt (0, -1.0f));
            for (std::size_t i = 1; i < env.size(); ++i)
            {
                blob.lineTo (pointAt (i, -1.0f));
            }
            for (std::size_t i = env.size(); i-- > 0;)
            {
                blob.lineTo (pointAt (i, 1.0f));
            }
            blob.closeSubPath();
        }
        else
        {
            blob.addRoundedRectangle (b.reduced (0.0f, b.getHeight() * 0.3f), 2.0f);
        }

        const float fillAlpha = selected ? 0.75f : 0.55f;
        g.setColour (colour.withAlpha (n.edits.muted ? 0.15f : fillAlpha));
        g.fillPath (blob);
        g.setColour (colour);
        g.strokePath (blob, juce::PathStrokeType (1.0f));

        // Pitch drift: the real f0 inside the note, moved with the note.
        if (!track.empty())
        {
            juce::Path curve;
            bool drawing = false;
            const auto first = track.frameIndexAt (n.startSeconds);
            const auto last = track.frameIndexAt (n.endSeconds);
            for (std::size_t i = first; i <= last && i < track.size(); ++i)
            {
                const auto& f = track.frames[i];
                if (!f.hasPitch())
                {
                    drawing = false;
                    continue;
                }
                const double midi = n.targetMidi() + (pitch::hzToMidi (f.f0Hz) - n.detectedMidi);
                const juce::Point<float> p (toX (f.timeSeconds),
                                            static_cast<float> (viewport.midiToY (midi)) + area.getY());
                if (drawing)
                {
                    curve.lineTo (p);
                }
                else
                {
                    curve.startNewSubPath (p);
                    drawing = true;
                }
            }
            g.setColour (theme::drift.withAlpha (selected ? 0.9f : 0.6f));
            g.strokePath (curve, juce::PathStrokeType (1.2f));
        }

        // Name of the note when there is room.
        if (b.getWidth() > 36.0f && pps > 12.0)
        {
            g.setColour (theme::text.withAlpha (0.8f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (juce::String (pitch::noteName (n.targetMidi())), b.translated (2.0f, -b.getHeight()),
                        juce::Justification::bottomLeft, false);
        }
    };

    for (const auto& n : take->notes())
    {
        if (!noteSelection.contains (n.id))
        {
            drawNote (n);
        }
    }
    for (const auto& n : take->notes())
    {
        if (noteSelection.contains (n.id))
        {
            drawNote (n);
        }
    }
}

void PianoRoll::paintKeyboard (juce::Graphics& g) const
{
    const auto area = noteArea();
    const juce::Rectangle<int> keys (0, area.getY(), keyboardWidth, area.getHeight());
    const juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (keys);
    g.setColour (theme::panel);
    g.fillRect (keys);

    const int lo = static_cast<int> (std::floor (viewport.lowMidi()));
    const int hi = static_cast<int> (std::ceil (viewport.highMidi()));
    g.setFont (juce::FontOptions (9.5f));
    for (int m = lo; m <= hi; ++m)
    {
        const auto yTop = static_cast<float> (viewport.midiToY (m + 0.5)) + static_cast<float> (area.getY());
        const auto yBottom = static_cast<float> (viewport.midiToY (m - 0.5)) + static_cast<float> (area.getY());
        const bool black = isBlackKey (m);
        const juce::Rectangle<float> key (
            0.0f, yTop, black ? keyboardWidth * 0.62f : static_cast<float> (keyboardWidth), yBottom - yTop);
        g.setColour (black ? theme::blackKey : theme::whiteKey);
        g.fillRect (key.reduced (0.0f, 0.5f));
        if (m % 12 == 0 && yBottom - yTop > 7.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.drawText (juce::String (pitch::noteName (m)), key.withTrimmedRight (4.0f),
                        juce::Justification::centredRight);
        }
    }
    g.setColour (theme::gridOctave);
    g.drawVerticalLine (keyboardWidth - 1, static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
}

void PianoRoll::paintRuler (juce::Graphics& g) const
{
    const auto area = noteArea();
    const juce::Rectangle<int> ruler (area.getX(), 0, area.getWidth(), rulerHeight);
    g.setColour (theme::panel);
    g.fillRect (getLocalBounds().withHeight (rulerHeight));

    const juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (ruler);
    const double step = timeGridStep (viewport.pixelsPerSecond(), 80.0);
    g.setFont (juce::FontOptions (10.0f));
    const auto firstLine = static_cast<long> (std::ceil (viewport.startSeconds() / step));
    for (auto k = firstLine; static_cast<double> (k) * step <= viewport.endSeconds(); ++k)
    {
        const double t = static_cast<double> (k) * step;
        const auto x = static_cast<int> (viewport.timeToX (t)) + area.getX();
        g.setColour (theme::textDim);
        g.drawVerticalLine (x, static_cast<float> (rulerHeight - 6), static_cast<float> (rulerHeight));
        g.setColour (theme::text);
        g.drawText (timeLabel (t, step), x + 3, 2, 70, rulerHeight - 6, juce::Justification::left);
    }
    const auto px = static_cast<float> (viewport.timeToX (playheadSeconds)) + static_cast<float> (area.getX());
    juce::Path marker;
    marker.addTriangle (px - 5.0f, 0.0f, px + 5.0f, 0.0f, px, 8.0f);
    g.setColour (theme::playhead);
    g.fillPath (marker);
}

// ---- Mouse ----------------------------------------------------------------------

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto area = noteArea();
    if (e.position.y < static_cast<float> (rulerHeight) && e.position.x >= static_cast<float> (area.getX()))
    {
        dragMode = DragMode::seek;
        if (onSeek)
        {
            onSeek (std::max (0.0, viewport.xToTime (toArea (e.position).x)));
        }
        return;
    }
    if (take == nullptr || !area.contains (e.getPosition()))
    {
        return;
    }

    const auto p = toArea (e.position);
    dragStartMidi = viewport.yToMidi (p.y);
    dragStartSeconds = viewport.xToTime (p.x);

    if (const auto* n = noteAtPoint (e.position))
    {
        if (e.mods.isShiftDown())
        {
            noteSelection.toggle (n->id);
        }
        else if (!noteSelection.contains (n->id))
        {
            noteSelection.selectOnly (n->id);
        }

        if (!noteSelection.contains (n->id))
        {
            dragMode = DragMode::none; // shift-click removed it
            repaint();
            return;
        }
        dragMode = modeForPress (e, *n);
        dragNote = n->id;
        dragNoteStartEdits = n->edits;
        dragSnapshot = editor::EditSnapshot (*take, noteSelection.toVector());
    }
    else
    {
        if (!e.mods.isShiftDown())
        {
            noteSelection.clear();
        }
        dragMode = DragMode::rubberBand;
        dragSnapshot = editor::EditSnapshot (*take, noteSelection.toVector()); // remembers the base selection
        rubberBand = {};
    }
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = toArea (e.position);
    const double nowMidi = viewport.yToMidi (p.y);
    const double nowSeconds = viewport.xToTime (p.x);

    if (dragMode == DragMode::seek)
    {
        if (onSeek)
        {
            onSeek (std::max (0.0, nowSeconds));
        }
        return;
    }
    if (take == nullptr)
    {
        return;
    }

    if (dragMode == DragMode::rubberBand)
    {
        rubberBand = juce::Rectangle<float> (e.mouseDownPosition, e.position);
        noteSelection.clear();
        for (const auto id : dragSnapshot.ids())
        {
            noteSelection.add (id);
        }
        noteSelection.selectInRegion (*take, dragStartSeconds, nowSeconds, dragStartMidi, nowMidi, true);
        repaint();
        return;
    }

    const auto* clicked = dragNote ? take->findNote (*dragNote) : nullptr;
    if (clicked == nullptr)
    {
        return;
    }

    if (dragMode == DragMode::pitch)
    {
        const auto snap = e.mods.isAltDown() ? editor::PitchSnap::free : pitchSnap;
        const double wanted =
            clicked->detectedMidi + dragNoteStartEdits.pitchOffsetCents / 100.0 + (nowMidi - dragStartMidi);
        const double newOffset = editor::snappedPitchOffsetCents (clicked->detectedMidi, wanted, snap);
        const double delta = newOffset - dragNoteStartEdits.pitchOffsetCents;
        for (const auto id : dragSnapshot.ids())
        {
            if (auto* n = take->findNote (id))
            {
                n->edits.pitchOffsetCents = dragSnapshot.find (id)->pitchOffsetCents + delta;
            }
        }
    }
    else if (dragMode == DragMode::time)
    {
        double delta = nowSeconds - dragStartSeconds;
        for (const auto id : dragSnapshot.ids()) // never move a note before 0 s
        {
            if (const auto* n = take->findNote (id))
            {
                delta = std::max (delta, -(n->startSeconds + dragSnapshot.find (id)->timeOffsetSeconds));
            }
        }
        for (const auto id : dragSnapshot.ids())
        {
            if (auto* n = take->findNote (id))
            {
                n->edits.timeOffsetSeconds = dragSnapshot.find (id)->timeOffsetSeconds + delta;
            }
        }
    }
    else if (dragMode == DragMode::stretchEnd || dragMode == DragMode::stretchStart)
    {
        auto& edits = take->findNote (clicked->id)->edits;
        const double duration = clicked->durationSeconds();
        const double start0 = clicked->startSeconds + dragNoteStartEdits.timeOffsetSeconds;
        const double end0 = start0 + duration * dragNoteStartEdits.timeStretch;
        if (dragMode == DragMode::stretchEnd)
        {
            edits.timeStretch = std::clamp ((nowSeconds - start0) / duration, minStretch, maxStretch);
        }
        else
        {
            const double stretch = std::clamp ((end0 - std::max (0.0, nowSeconds)) / duration, minStretch, maxStretch);
            edits.timeStretch = stretch;
            edits.timeOffsetSeconds = end0 - duration * stretch - clicked->startSeconds;
        }
    }
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (take != nullptr && dragMode != DragMode::none && dragMode != DragMode::rubberBand && dragMode != DragMode::seek)
    {
        const char* name = "Edit";
        switch (dragMode)
        {
            case DragMode::pitch:
                name = "Move pitch";
                break;
            case DragMode::time:
                name = "Move in time";
                break;
            case DragMode::stretchStart:
            case DragMode::stretchEnd:
                name = "Stretch";
                break;
            case DragMode::none:
            case DragMode::rubberBand:
            case DragMode::seek:
                break;
        }
        auto command = dragSnapshot.toCommand (name, *take);
        if (!command.empty())
        {
            editHistory.push (std::move (command));
            editsChanged();
        }
    }
    dragMode = DragMode::none;
    dragNote.reset();
    rubberBand = {};
    repaint();
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    auto cursor = juce::MouseCursor::NormalCursor;
    if (const auto* n = noteAtPoint (e.position))
    {
        const auto mode = modeForPress (e, *n);
        cursor = (mode == DragMode::stretchStart || mode == DragMode::stretchEnd)
                     ? juce::MouseCursor::LeftRightResizeCursor
                     : juce::MouseCursor::UpDownResizeCursor;
    }
    setMouseCursor (cursor);
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (noteAtPoint (e.position) == nullptr)
    {
        noteSelection.clear();
        repaint();
    }
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto p = toArea (e.position);
    if (e.mods.isCommandDown())
    {
        viewport.zoomTime (std::exp (2.0 * wheel.deltaY), p.x);
    }
    else if (e.mods.isAltDown())
    {
        viewport.zoomPitch (std::exp (2.0 * wheel.deltaY), p.y);
    }
    else
    {
        const double dx = e.mods.isShiftDown() ? wheel.deltaY : wheel.deltaX;
        const double dy = e.mods.isShiftDown() ? 0.0 : wheel.deltaY;
        viewport.scrollTime (-dx * 400.0);
        viewport.scrollPitch (dy * 400.0);
    }
    repaint();
}

void PianoRoll::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    viewport.zoomTime (scaleFactor, toArea (e.position).x);
    repaint();
}

void PianoRoll::nudgeSelection (double cents)
{
    if (take == nullptr || noteSelection.empty())
    {
        return;
    }
    const editor::EditSnapshot snapshot (*take, noteSelection.toVector());
    for (const auto id : noteSelection.noteIds())
    {
        if (auto* n = take->findNote (id))
        {
            n->edits.pitchOffsetCents += cents;
        }
    }
    editHistory.push (snapshot.toCommand ("Nudge pitch", *take));
    editsChanged();
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        undo();
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0))
    {
        redo();
        return true;
    }
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0) && take != nullptr)
    {
        noteSelection.selectAll (*take);
        repaint();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (dragMode != DragMode::none && take != nullptr && dragMode != DragMode::rubberBand)
        {
            dragSnapshot.restore (*take);
            dragMode = DragMode::none;
        }
        else
        {
            noteSelection.clear();
        }
        rubberBand = {};
        repaint();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
    {
        const double sign = key.getKeyCode() == juce::KeyPress::upKey ? 1.0 : -1.0;
        nudgeSelection (sign * (mods.isShiftDown() ? 10.0 : 100.0));
        return true;
    }
    return false;
}
} // namespace melisma::ui
