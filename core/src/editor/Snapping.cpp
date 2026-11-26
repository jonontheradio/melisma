#include "melisma/core/editor/Snapping.h"

#include <cmath>

namespace melisma::editor
{
double snappedPitchOffsetCents (double detectedMidi, double wantedMidi, PitchSnap snap)
{
    const double target = snap == PitchSnap::semitone ? std::round (wantedMidi) : wantedMidi;
    return 100.0 * (target - detectedMidi);
}
} // namespace melisma::editor
