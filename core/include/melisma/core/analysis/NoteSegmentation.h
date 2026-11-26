#pragma once

#include "melisma/core/analysis/PitchTrack.h"
#include "melisma/core/analysis/Voicing.h"
#include "melisma/core/model/Note.h"

#include <vector>

namespace melisma::analysis
{
struct SegmentationSettings
{
    /** 0..1. Higher = smaller pitch changes start a new note (more, shorter notes). */
    double sensitivity = 0.5;
    /** Notes shorter than this are merged into a neighbour (or dropped if isolated). */
    double minNoteSeconds = 0.06;

    /** Pitch change, in semitones, that starts a new note (derived from sensitivity). */
    double splitSemitones() const;
    /** How long a pitch change must last to count (derived from sensitivity). */
    double holdSeconds() const;
};

/** Splits the voiced regions of a pitch track into notes.

    A new note starts when the pitch moves away from the current note's
    reference pitch by more than splitSemitones() and stays there for at least
    holdSeconds(), so vibrato and drift stay inside one note. Each note gets an
    energy-weighted centre pitch measured on its stable middle part.

    Onsets (see detectOnsets) also start a new note when they fall inside one,
    which separates repeated notes of the same pitch.

    Returned notes are sorted, do not overlap and have id 0 (Take assigns ids). */
std::vector<model::Note> segmentNotes (const PitchTrack& track, const std::vector<FrameClass>& classes,
                                       const SegmentationSettings& settings = {},
                                       const std::vector<double>& onsetTimes = {});
} // namespace melisma::analysis
