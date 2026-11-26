#pragma once

#include <cstdint>

namespace melisma::model
{
/** Stable identifier of a note inside a take (survives edits, splits, undo). */
using NoteId = std::uint32_t;

/** Everything the user can change on a note. All values are relative to the
    detected note, and the defaults leave the audio untouched. */
struct NoteEdits
{
    double pitchOffsetCents = 0.0;      // moves the whole note
    double driftAmount = 1.0;           // 1 = original slow pitch deviation, 0 = flat, >1 = exaggerated
    double vibratoAmount = 1.0;         // 1 = original vibrato depth, 0 = removed
    double formantShiftSemitones = 0.0; // timbre, independent of pitch
    double gainDb = 0.0;
    double timeOffsetSeconds = 0.0; // moves the note in time
    double timeStretch = 1.0;       // > 1 = longer
    bool muted = false;

    bool isNeutral() const;
    bool operator== (const NoteEdits&) const = default;
};

/** A detected note: where it is in the source audio, its pitch, and the edits
    applied to it. Times refer to the original (unedited) source. */
struct Note
{
    NoteId id = 0;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    double detectedMidi = 0.0; // energy-weighted centre pitch, fractional MIDI
    NoteEdits edits;

    double durationSeconds() const { return endSeconds - startSeconds; }
    bool contains (double seconds) const { return seconds >= startSeconds && seconds < endSeconds; }

    /** Centre pitch after edits, fractional MIDI. */
    double targetMidi() const { return detectedMidi + edits.pitchOffsetCents / 100.0; }

    /** Start and end after the time edits. */
    double editedStartSeconds() const { return startSeconds + edits.timeOffsetSeconds; }
    double editedEndSeconds() const { return editedStartSeconds() + durationSeconds() * edits.timeStretch; }

    bool operator== (const Note&) const = default;
};
} // namespace melisma::model
