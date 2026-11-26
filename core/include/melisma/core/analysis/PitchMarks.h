#pragma once

#include "melisma/core/analysis/PitchTrack.h"
#include "melisma/core/analysis/Voicing.h"
#include "melisma/core/audio/MonoAudio.h"

#include <cstddef>
#include <vector>

namespace melisma::analysis
{
/** Pitch marks (epochs): one sample position per glottal cycle in voiced
    regions, always at the same point of the waveform (the main peak of each
    cycle). TD-PSOLA cuts and re-places grains around these marks. */
struct PitchMarks
{
    std::vector<std::size_t> positions; // sorted sample indices into the source audio

    bool empty() const { return positions.empty(); }
    std::size_t size() const { return positions.size(); }
};

/** Places pitch marks in voiced regions, guided by the f0 track:
    each mark is searched around the previous one plus one period, on a
    low-passed copy of the signal, with a fixed polarity per region. */
PitchMarks detectPitchMarks (const audio::MonoAudio& audio, const PitchTrack& track,
                             const std::vector<FrameClass>& classes);
} // namespace melisma::analysis
