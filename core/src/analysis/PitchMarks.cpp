#include "melisma/core/analysis/PitchMarks.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace melisma::analysis
{
namespace
{
/** Zero-phase (forward + backward) 2nd-order Butterworth low-pass. Keeps the
    fundamental and first harmonics, removes formant ringing that would make
    several peaks per cycle compete. */
std::vector<double> zeroPhaseLowPass (const std::vector<float>& x, double cutoffHz, double sampleRate)
{
    const double k = std::tan (std::numbers::pi * cutoffHz / sampleRate);
    const double q = std::numbers::sqrt2 / 2.0;
    const double norm = 1.0 / (1.0 + k / q + k * k);
    const double b0 = k * k * norm;
    const double b1 = 2.0 * b0;
    const double b2 = b0;
    const double a1 = 2.0 * (k * k - 1.0) * norm;
    const double a2 = (1.0 - k / q + k * k) * norm;

    std::vector<double> y (x.size());
    const auto pass = [&] (auto begin, auto end, auto out)
    {
        double x1 = 0.0;
        double x2 = 0.0;
        double y1 = 0.0;
        double y2 = 0.0;
        for (auto it = begin; it != end; ++it, ++out)
        {
            const double in = *it;
            const double v = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = in;
            y2 = y1;
            y1 = v;
            *out = v;
        }
    };

    std::vector<double> forward (x.size());
    pass (x.begin(), x.end(), forward.begin());
    pass (forward.rbegin(), forward.rend(), y.rbegin());
    return y;
}

/** f0 at a sample, from the nearest frame with a pitch inside [first, last). */
double f0At (const PitchTrack& track, std::size_t sample, double sampleRate, std::size_t first, std::size_t last)
{
    const auto idx = std::clamp (track.frameIndexAt (static_cast<double> (sample) / sampleRate), first, last - 1);
    for (std::size_t d = 0; d < last - first; ++d)
    {
        if (idx >= first + d && track.frames[idx - d].hasPitch())
        {
            return track.frames[idx - d].f0Hz;
        }
        if (idx + d < last && track.frames[idx + d].hasPitch())
        {
            return track.frames[idx + d].f0Hz;
        }
    }
    return 0.0;
}
} // namespace

PitchMarks detectPitchMarks (const audio::MonoAudio& audio, const PitchTrack& track,
                             const std::vector<FrameClass>& classes)
{
    PitchMarks marks;
    const std::size_t n = std::min (track.size(), classes.size());
    if (audio.empty() || n == 0)
    {
        return marks;
    }

    const double sr = audio.sampleRate;
    const auto smooth = zeroPhaseLowPass (audio.samples, 1000.0, sr);
    const auto total = smooth.size();

    std::size_t i = 0;
    while (i < n)
    {
        if (classes[i] != FrameClass::voiced || !track.frames[i].hasPitch())
        {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < n && classes[j] == FrameClass::voiced && track.frames[j].hasPitch())
        {
            ++j;
        }

        // Region in samples (frame centres +- half a hop).
        const double halfHop = 0.5 * track.hopSeconds;
        const auto regionStart =
            static_cast<std::size_t> (std::max (0.0, (track.frames[i].timeSeconds - halfHop) * sr));
        const auto regionEnd =
            std::min (total, static_cast<std::size_t> ((track.frames[j - 1].timeSeconds + halfHop) * sr));
        const std::size_t firstFrame = i;
        const std::size_t lastFrame = j;
        i = j;
        if (regionEnd <= regionStart + 2)
        {
            continue;
        }

        // Polarity: follow whichever peaks are stronger in this region.
        double maxPos = 0.0;
        double maxNeg = 0.0;
        for (std::size_t s = regionStart; s < regionEnd; ++s)
        {
            maxPos = std::max (maxPos, smooth[s]);
            maxNeg = std::max (maxNeg, -smooth[s]);
        }
        const double polarity = maxPos >= maxNeg ? 1.0 : -1.0;
        const auto value = [&smooth, polarity] (std::size_t s) { return polarity * smooth[s]; };

        const auto peakIn = [&value] (std::size_t from, std::size_t to)
        {
            std::size_t best = from;
            for (std::size_t s = from; s < to; ++s)
            {
                if (value (s) > value (best))
                {
                    best = s;
                }
            }
            return best;
        };

        // First mark: strongest peak within the first period.
        const double firstPeriod = sr / f0At (track, regionStart, sr, firstFrame, lastFrame);
        std::size_t mark =
            peakIn (regionStart, std::min (regionEnd, regionStart + static_cast<std::size_t> (firstPeriod) + 1));
        marks.positions.push_back (mark);

        // Next marks: one period later, searched within +-25% of a period.
        while (true)
        {
            const double period = sr / f0At (track, mark, sr, firstFrame, lastFrame);
            const auto expected = static_cast<double> (mark) + period;
            const auto from = static_cast<std::size_t> (
                std::max (static_cast<double> (mark) + 0.5 * period, expected - 0.25 * period));
            const auto to = static_cast<std::size_t> (expected + 0.25 * period) + 1;
            if (to >= regionEnd || from >= to)
            {
                break;
            }
            mark = peakIn (from, to);
            marks.positions.push_back (mark);
        }
    }
    return marks;
}
} // namespace melisma::analysis
