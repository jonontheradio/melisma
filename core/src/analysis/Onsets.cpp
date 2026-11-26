#include "melisma/core/analysis/Onsets.h"

#include "melisma/core/dsp/Fft.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace melisma::analysis
{
namespace
{
double toDb (double linear)
{
    return 20.0 * std::log10 (std::max (linear, 1e-9));
}

double median (std::vector<double> v)
{
    if (v.empty())
    {
        return 0.0;
    }
    const auto mid = v.begin() + static_cast<std::ptrdiff_t> (v.size() / 2);
    std::nth_element (v.begin(), mid, v.end());
    return *mid;
}

/** Half-wave rectified log-magnitude spectral flux per frame (up to 8 kHz). */
std::vector<double> spectralFlux (const audio::MonoAudio& audio, const PitchTrack& track)
{
    constexpr std::size_t fftSize = 2048;
    constexpr double compression = 100.0; // log(1 + c * |X|)
    const dsp::Fft fft (fftSize);

    std::vector<double> window (fftSize);
    for (std::size_t i = 0; i < fftSize; ++i)
    {
        window[i] =
            0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * static_cast<double> (i) / static_cast<double> (fftSize));
    }

    const auto maxBin =
        std::min<std::size_t> (fftSize / 2, static_cast<std::size_t> (8000.0 / audio.sampleRate * fftSize));
    std::vector<std::complex<double>> spectrum (fftSize);
    std::vector<double> previous (maxBin, 0.0);
    std::vector<double> flux (track.size(), 0.0);
    const auto total = static_cast<std::ptrdiff_t> (audio.size());

    for (std::size_t f = 0; f < track.size(); ++f)
    {
        const auto centre = static_cast<std::ptrdiff_t> (std::llround (track.frames[f].timeSeconds * audio.sampleRate));
        const auto start = centre - static_cast<std::ptrdiff_t> (fftSize / 2);
        for (std::size_t i = 0; i < fftSize; ++i)
        {
            const auto idx = start + static_cast<std::ptrdiff_t> (i);
            const double x =
                (idx >= 0 && idx < total) ? static_cast<double> (audio.samples[static_cast<std::size_t> (idx)]) : 0.0;
            spectrum[i] = x * window[i];
        }
        fft.forward (spectrum);

        double sum = 0.0;
        for (std::size_t k = 1; k < maxBin; ++k)
        {
            const double mag = std::log1p (compression * std::abs (spectrum[k]));
            sum += std::max (0.0, mag - previous[k]);
            previous[k] = mag;
        }
        flux[f] = f == 0 ? 0.0 : sum;
    }
    return flux;
}

/** Onsets from energy: a rise of `riseDb` within `riseFrames` after a local
    minimum that is itself `riseDb` below the preceding maximum (a real dip,
    not just a crescendo). */
std::vector<std::size_t> energyOnsets (const std::vector<double>& db, double riseDb, std::size_t riseFrames,
                                       double floorDb)
{
    std::vector<std::size_t> onsets;
    double runningMax = -1e9;
    std::size_t i = 1;
    while (i + 1 < db.size())
    {
        runningMax = std::max (runningMax, db[i - 1]);
        const bool localMin = db[i] <= db[i - 1] && db[i] < db[i + 1];
        if (!localMin || runningMax - db[i] < riseDb)
        {
            ++i;
            continue;
        }

        // Look for the rise after the dip.
        const std::size_t end = std::min (db.size(), i + riseFrames + 1);
        for (std::size_t j = i + 1; j < end; ++j)
        {
            if (db[j] - db[i] >= riseDb && db[j] > floorDb)
            {
                onsets.push_back (i); // the note starts at the bottom of the dip
                runningMax = db[j];
                i = j;
                break;
            }
        }
        ++i;
    }
    return onsets;
}

/** Peaks of the flux novelty above an adaptive (moving median) threshold. */
std::vector<std::size_t> fluxOnsets (const std::vector<double>& flux, double threshold, std::size_t radius,
                                     const std::vector<double>& db, double floorDb)
{
    std::vector<std::size_t> onsets;
    if (flux.size() < 3)
    {
        return onsets;
    }

    // Typical flux level over audible frames, so the threshold is level-independent.
    std::vector<double> audible;
    for (std::size_t i = 0; i < flux.size(); ++i)
    {
        if (db[i] > floorDb)
        {
            audible.push_back (flux[i]);
        }
    }
    const double scale = std::max (median (audible), 1e-9);

    for (std::size_t i = 1; i + 1 < flux.size(); ++i)
    {
        if (db[i] <= floorDb || flux[i] < flux[i - 1] || flux[i] <= flux[i + 1])
        {
            continue;
        }
        const std::size_t a = i > radius ? i - radius : 0;
        const std::size_t b = std::min (flux.size(), i + radius + 1);
        const double localMedian = median (std::vector<double> (flux.begin() + static_cast<std::ptrdiff_t> (a),
                                                                flux.begin() + static_cast<std::ptrdiff_t> (b)));
        if ((flux[i] - localMedian) / scale >= threshold)
        {
            onsets.push_back (i);
        }
    }
    return onsets;
}
} // namespace

double OnsetSettings::energyRiseDb() const
{
    const double s = std::clamp (sensitivity, 0.0, 1.0);
    return 12.0 + s * (5.0 - 12.0); // 12 dB (strict) .. 5 dB
}

double OnsetSettings::fluxThreshold() const
{
    const double s = std::clamp (sensitivity, 0.0, 1.0);
    return 6.0 + s * (2.5 - 6.0); // peak must exceed its local median by 6x .. 2.5x the typical flux
}

std::vector<double> detectOnsets (const audio::MonoAudio& audio, const PitchTrack& track, const OnsetSettings& settings)
{
    std::vector<double> times;
    if (track.size() < 3 || audio.empty() || track.hopSeconds <= 0.0)
    {
        return times;
    }

    const double hop = track.hopSeconds;
    std::vector<double> db (track.size());
    double peakDb = -1e9;
    for (std::size_t i = 0; i < track.size(); ++i)
    {
        db[i] = toDb (track.frames[i].rms);
        peakDb = std::max (peakDb, db[i]);
    }
    const double floorDb = peakDb - 45.0; // ignore activity in near-silence

    const auto frames = [hop] (double seconds)
    { return std::max<std::size_t> (1, static_cast<std::size_t> (std::lround (seconds / hop))); };

    const auto fromEnergy = energyOnsets (db, settings.energyRiseDb(), frames (0.04), floorDb);
    const auto fromFlux =
        fluxOnsets (spectralFlux (audio, track), settings.fluxThreshold(), frames (0.15), db, floorDb);

    // Energy dips are the reliable cue. A flux peak only counts when the level
    // also rises clearly around it (an attack), which rejects vibrato and
    // spectral wobble, and an energy onset nearby always wins.
    const std::size_t minGap = frames (settings.minIntervalSeconds);
    const std::size_t riseWindow = frames (0.03);
    std::vector<std::size_t> candidates (fromEnergy.begin(), fromEnergy.end());
    for (const auto f : fromFlux)
    {
        const std::size_t a = f > riseWindow ? f - riseWindow : 0;
        const std::size_t b = std::min (db.size() - 1, f + riseWindow);
        const double rise = db[b]
                            - *std::min_element (db.begin() + static_cast<std::ptrdiff_t> (a),
                                                 db.begin() + static_cast<std::ptrdiff_t> (f) + 1);
        const bool nearEnergyOnset =
            std::ranges::any_of (fromEnergy, [f, minGap] (std::size_t e) { return (e > f ? e - f : f - e) < minGap; });
        if (rise >= 0.75 * settings.energyRiseDb() && !nearEnergyOnset)
        {
            candidates.push_back (f);
        }
    }
    std::ranges::sort (candidates);

    // Merge onsets closer than the minimum interval (keep the first).
    std::size_t last = 0;
    bool any = false;
    for (const auto c : candidates)
    {
        if (!any || c - last >= minGap)
        {
            times.push_back (track.frames[c].timeSeconds);
            last = c;
            any = true;
        }
    }
    return times;
}
} // namespace melisma::analysis
