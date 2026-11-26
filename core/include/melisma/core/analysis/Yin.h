#pragma once

#include "melisma/core/analysis/PitchTrack.h"
#include "melisma/core/audio/MonoAudio.h"

#include <functional>

namespace melisma::analysis
{
/** Settings for the YIN pitch detector. */
struct YinSettings
{
    double minHz = 70.0;        // lowest pitch searched
    double maxHz = 1000.0;      // highest pitch searched
    double threshold = 0.15;    // CMNDF absolute threshold (lower = stricter)
    double hopSeconds = 0.005;  // time between frames
    double silenceRms = 1.0e-4; // frames quieter than this (~ -80 dBFS) get no pitch
};

/** YIN fundamental frequency estimator (de Cheveigné & Kawahara, 2002):
    difference function (computed with FFTs), cumulative mean normalisation,
    absolute threshold and parabolic interpolation.

    Outputs one frame every hopSeconds. Frames where no lag falls below the
    threshold, or that are silent, have f0Hz = 0. */
/** Called with progress in [0, 1]. Return false to cancel (the track is then
    returned incomplete). */
using ProgressCallback = std::function<bool (double)>;

PitchTrack detectPitchYin (const audio::MonoAudio& audio, const YinSettings& settings = {},
                           const ProgressCallback& progress = {});
} // namespace melisma::analysis
