#pragma once

#include "melisma/core/analysis/PitchTrack.h"
#include "melisma/core/audio/MonoAudio.h"

#include <vector>

namespace melisma::analysis
{
struct OnsetSettings
{
    /** 0..1. Higher = weaker attacks count as onsets. */
    double sensitivity = 0.5;
    /** Two onsets are never closer than this. */
    double minIntervalSeconds = 0.08;

    /** Energy rise after a dip, in dB, that marks an onset (derived from sensitivity). */
    double energyRiseDb() const;
    /** Spectral-flux peak height above its local median, relative to the
        typical flux level (derived from sensitivity). */
    double fluxThreshold() const;
};

/** Detects note onsets: the starts of new notes even when the pitch does not
    change (repeated notes, re-articulated syllables).

    Two cues are combined:
    - energy: the level rises by energyRiseDb() after a dip of the same depth;
    - spectral flux: a sharp increase of log-magnitude across the spectrum
      (consonants, attacks), picked against an adaptive threshold.

    Returns onset times in seconds, sorted. `track` provides the frame grid
    and per-frame energy. */
std::vector<double> detectOnsets (const audio::MonoAudio& audio, const PitchTrack& track,
                                  const OnsetSettings& settings = {});
} // namespace melisma::analysis
