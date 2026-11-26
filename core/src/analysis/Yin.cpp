#include "melisma/core/analysis/Yin.h"

#include "melisma/core/dsp/Fft.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace melisma::analysis
{
namespace
{
/** Reusable buffers and FFT plan for one analysis run. */
class YinFrameAnalyser
{
public:
    YinFrameAnalyser (std::size_t windowLength, std::size_t maxLagSamples)
        : window (windowLength)
        , maxLag (maxLagSamples)
        , fft (dsp::Fft::nextPowerOfTwo (2 * windowLength + maxLagSamples))
        , spectrumA (fft.size())
        , spectrumB (fft.size())
        , energyPrefix (windowLength + maxLagSamples + 1)
        , cmndf (maxLagSamples + 1)
    {
    }

    /** frame must hold window + maxLag samples. Fills cmndf[0..maxLag]. */
    void computeCmndf (const std::vector<double>& frame)
    {
        // Cross-correlation r(tau) = sum_{j<W} x[j] x[j+tau] via FFT.
        std::ranges::fill (spectrumA, std::complex<double>{});
        std::ranges::fill (spectrumB, std::complex<double>{});
        for (std::size_t j = 0; j < window; ++j)
        {
            spectrumA[j] = frame[j];
        }
        for (std::size_t j = 0; j < frame.size(); ++j)
        {
            spectrumB[j] = frame[j];
        }
        fft.forward (spectrumA);
        fft.forward (spectrumB);
        for (std::size_t k = 0; k < spectrumA.size(); ++k)
        {
            spectrumB[k] *= std::conj (spectrumA[k]);
        }
        fft.inverse (spectrumB); // spectrumB[tau].real() = r(tau)

        // Energies of each window position via prefix sums of x^2.
        energyPrefix[0] = 0.0;
        for (std::size_t j = 0; j < frame.size(); ++j)
        {
            energyPrefix[j + 1] = energyPrefix[j] + frame[j] * frame[j];
        }
        const double energy0 = energyPrefix[window];

        // Difference function d(tau) and its cumulative mean normalisation.
        cmndf[0] = 1.0;
        double runningSum = 0.0;
        for (std::size_t tau = 1; tau <= maxLag; ++tau)
        {
            const double energyTau = energyPrefix[tau + window] - energyPrefix[tau];
            const double d = std::max (0.0, energy0 + energyTau - 2.0 * spectrumB[tau].real());
            runningSum += d;
            cmndf[tau] = runningSum > 0.0 ? d * static_cast<double> (tau) / runningSum : 1.0;
        }
    }

    const std::vector<double>& values() const { return cmndf; }

private:
    std::size_t window;
    std::size_t maxLag;
    dsp::Fft fft;
    std::vector<std::complex<double>> spectrumA, spectrumB;
    std::vector<double> energyPrefix;
    std::vector<double> cmndf;
};

/** Refines an integer lag with a parabola through its neighbours. */
double parabolicLag (const std::vector<double>& v, std::size_t tau)
{
    if (tau == 0 || tau + 1 >= v.size())
    {
        return static_cast<double> (tau);
    }
    const double a = v[tau - 1];
    const double b = v[tau];
    const double c = v[tau + 1];
    const double denom = a - 2.0 * b + c;
    if (std::abs (denom) < 1e-12)
    {
        return static_cast<double> (tau);
    }
    const double offset = std::clamp (0.5 * (a - c) / denom, -0.5, 0.5);
    return static_cast<double> (tau) + offset;
}
/** Moves each pitch estimate from the time it really describes (estimateTimes)
    back to its frame time, interpolating in log-frequency between consecutive
    voiced estimates. Without this, a moving pitch would read several
    milliseconds late (several cents of error on vibrato and glides). */
void alignToFrameTimes (PitchTrack& track, const std::vector<double>& estimateTimes)
{
    auto& frames = track.frames;
    std::vector<double> aligned (frames.size(), 0.0);

    std::size_t j = 0; // index of the estimate at or before the current frame time
    for (std::size_t i = 0; i < frames.size(); ++i)
    {
        aligned[i] = frames[i].f0Hz;
        if (!frames[i].hasPitch())
        {
            continue;
        }

        const double t = frames[i].timeSeconds;
        j = std::min (j, i);
        while (j > 0 && frames[j - 1].hasPitch() && estimateTimes[j] > t)
        {
            --j;
        }
        while (j + 1 < frames.size() && frames[j + 1].hasPitch() && estimateTimes[j + 1] <= t)
        {
            ++j;
        }

        const std::size_t k = j + 1;
        if (!frames[j].hasPitch() || k >= frames.size() || !frames[k].hasPitch() || estimateTimes[j] > t
            || estimateTimes[k] < t || estimateTimes[k] <= estimateTimes[j])
        {
            continue; // not bracketed by two voiced estimates: keep the raw value
        }

        const double frac = (t - estimateTimes[j]) / (estimateTimes[k] - estimateTimes[j]);
        aligned[i] =
            std::exp2 (std::log2 (frames[j].f0Hz) + frac * (std::log2 (frames[k].f0Hz) - std::log2 (frames[j].f0Hz)));
    }

    for (std::size_t i = 0; i < frames.size(); ++i)
    {
        frames[i].f0Hz = aligned[i];
    }
}
} // namespace

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
