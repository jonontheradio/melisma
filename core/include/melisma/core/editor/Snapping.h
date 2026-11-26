#pragma once

#include <cstdint>

namespace melisma::editor
{
/** How dragged pitches are snapped. Scale snapping arrives with M4. */
enum class PitchSnap : std::uint8_t
{
    free,     // any value
    semitone, // the note's centre lands exactly on an equal-tempered note
};

/** Pitch offset (cents) to apply to a note whose detected centre is
    `detectedMidi` so that it ends up at `wantedMidi`, after snapping. */
double snappedPitchOffsetCents (double detectedMidi, double wantedMidi, PitchSnap snap);
} // namespace melisma::editor
