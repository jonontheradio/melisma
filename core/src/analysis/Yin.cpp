#include "melisma/core/analysis/Yin.h"

#include "YinInternal.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace melisma::analysis
{
using detail::alignToFrameTimes;
using detail::parabolicLag;
using detail::YinFrameAnalyser;

PitchTrack detectPitchYin (const audio::MonoAudio& audio, const YinSettings& settings, const ProgressCallback& progress)
{
    PitchTrack track;
    track.hopSeconds = settings.hopSeconds;
    if (audio.empty() || audio.sampleRate <= 0.0 || settings.hopSeconds <= 0.0 || settings.minHz <= 0.0
        || settings.maxHz <= settings.minHz)
    {
        return track;
    }

    const double sr = audio.sampleRate;
    const auto minLag = std::max<std::size_t> (2, static_cast<std::size_t> (std::floor (sr / settings.maxHz)));
    const auto maxLag = static_cast<std::size_t> (std::ceil (sr / settings.minHz));
    const std::size_t window = maxLag; // integration window: one period of the lowest pitch
    const std::size_t frameLength = window + maxLag;

    YinFrameAnalyser analyser (window, maxLag);
    std::vector<double> frame (frameLength);

    const auto numFrames = static_cast<std::size_t> (std::floor (audio.durationSeconds() / settings.hopSeconds)) + 1;
    track.frames.reserve (numFrames);
    std::vector<double> estimateTimes (numFrames, 0.0);
    const auto totalSamples = static_cast<std::ptrdiff_t> (audio.size());

    constexpr std::size_t progressInterval = 200; // frames (~1 s of audio at 5 ms hop)
    for (std::size_t f = 0; f < numFrames; ++f)
    {
        if (progress && f % progressInterval == 0
            && !progress (static_cast<double> (f) / static_cast<double> (numFrames)))
        {
            return track; // cancelled
        }

        PitchFrame out;
        out.timeSeconds = static_cast<double> (f) * settings.hopSeconds;

        // The first integration window is centred on the frame time; the lagged
        // copy extends forward. Zero-padded at the edges.
        const auto centre = static_cast<std::ptrdiff_t> (std::llround (out.timeSeconds * sr));
        const auto start = centre - static_cast<std::ptrdiff_t> (window / 2);
        double energy = 0.0;
        for (std::size_t j = 0; j < frameLength; ++j)
        {
            const auto idx = start + static_cast<std::ptrdiff_t> (j);
            frame[j] = (idx >= 0 && idx < totalSamples)
                           ? static_cast<double> (audio.samples[static_cast<std::size_t> (idx)])
                           : 0.0;
            energy += frame[j] * frame[j];
        }
        out.rms = std::sqrt (energy / static_cast<double> (frameLength));

        if (out.rms >= settings.silenceRms)
        {
            analyser.computeCmndf (frame);
            const auto& d = analyser.values();

            // First lag under the threshold, then walk down to its local minimum.
            std::size_t best = 0;
            for (std::size_t tau = minLag; tau <= maxLag; ++tau)
            {
                if (d[tau] < settings.threshold)
                {
                    while (tau + 1 <= maxLag && d[tau + 1] < d[tau])
                    {
                        ++tau;
                    }
                    best = tau;
                    break;
                }
            }

            if (best != 0)
            {
                const double lag = parabolicLag (d, best);
                out.f0Hz = sr / lag;
                // YIN compares x[t] with x[t + lag]: the estimate belongs half a period later.
                estimateTimes[f] = out.timeSeconds + 0.5 * lag / sr;
                out.confidence = std::clamp (1.0 - d[best], 0.0, 1.0);
            }
            else
            {
                // Not periodic enough: report how periodic the best lag was, but no pitch.
                const auto it = std::min_element (d.begin() + static_cast<std::ptrdiff_t> (minLag), d.end());
                out.confidence = std::clamp (1.0 - *it, 0.0, 1.0);
            }
        }

        track.frames.push_back (out);
    }

    alignToFrameTimes (track, estimateTimes);
    return track;
}
} // namespace melisma::analysis
