#include "PitchAccuracy.h"

#include <melisma/core/Pitch.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace melisma::testsupport
{
/** Compares detected f0 with ground truth on frames that are fully voiced
    (truth > 0 for `margin` seconds around the frame). */
Accuracy measure (const analysis::PitchTrack& track, const testsignals::Signal& truth, double margin)
{
    Accuracy acc;
    std::vector<double> errors;
    for (const auto& frame : track.frames)
    {
        const double t = frame.timeSeconds;
        if (truth.f0At (t - margin) <= 0.0 || truth.f0At (t) <= 0.0 || truth.f0At (t + margin) <= 0.0)
        {
            continue;
        }
        ++acc.framesCompared;
        if (!frame.hasPitch())
        {
            ++acc.framesWithoutPitch;
            continue;
        }
        const double cents = std::abs (pitch::centsBetween (truth.f0At (t), frame.f0Hz));
        if (cents > 50.0)
        {
            ++acc.grossErrors;
        }
        errors.push_back (cents);
    }

    if (!errors.empty())
    {
        std::ranges::sort (errors);
        acc.medianCents = errors[errors.size() / 2];
        acc.p95Cents = errors[static_cast<std::size_t> (0.95 * static_cast<double> (errors.size() - 1))];
        acc.maxCents = errors.back();
    }
    return acc;
}

} // namespace melisma::testsupport
