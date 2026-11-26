#pragma once

#include "TestSignals.h"

#include <melisma/core/analysis/PitchTrack.h>

#include <cstddef>

namespace melisma::testsupport
{
struct Accuracy
{
    std::size_t framesCompared = 0;
    std::size_t framesWithoutPitch = 0;
    std::size_t grossErrors = 0; // |error| > 50 cents (octave errors, wrong harmonic...)
    double medianCents = 0.0;
    double p95Cents = 0.0;
    double maxCents = 0.0;
};

/** Compares detected f0 with ground truth on frames that are fully voiced
    (truth > 0 for `margin` seconds around the frame). */
Accuracy measure (const analysis::PitchTrack& track, const testsignals::Signal& truth, double margin = 0.03);
} // namespace melisma::testsupport
