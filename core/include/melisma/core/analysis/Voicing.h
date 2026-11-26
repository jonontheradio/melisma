#pragma once

#include "melisma/core/analysis/PitchTrack.h"
#include "melisma/core/audio/MonoAudio.h"

#include <cstdint>
#include <vector>

namespace melisma::analysis
{
/** What a pitch-track frame contains. */
enum class FrameClass : std::uint8_t
{
    silence,  // too quiet to matter
    unvoiced, // audible but without a stable pitch: consonants, breaths, noise
    voiced,   // pitched: vowels, sustained notes
};

struct VoicingSettings
{
    double minConfidence = 0.8;              // pitch-detector confidence needed to count as voiced
    double silenceDbBelowPeak = 50.0;        // frames this far under the loudest frame are silence
    double absoluteSilenceDb = -70.0;        // ...and anything under this level, whatever the peak
    double maxVoicedZeroCrossingHz = 3000.0; // fricatives and noise cross zero much more often
    double fillGapsSeconds = 0.02;           // short dropouts inside a voiced region are bridged
    double minVoicedSeconds = 0.03;          // shorter voiced blips become unvoiced
};

/** Classifies every frame of `track` (computed on `audio`) as silence, unvoiced
    or voiced, then smooths the result so regions do not flicker. */
std::vector<FrameClass> classifyFrames (const audio::MonoAudio& audio, const PitchTrack& track,
                                        const VoicingSettings& settings = {});
} // namespace melisma::analysis
