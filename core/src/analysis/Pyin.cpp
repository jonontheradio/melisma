#include "melisma/core/analysis/Pyin.h"

#include "YinInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace melisma::analysis
{
namespace
{
constexpr std::size_t numThresholds = 100;
constexpr double binCents = 20.0;
constexpr int maxJumpBins = 13; // ~260 cents per frame
constexpr double voicingSwitchProbability = 0.01;
constexpr double noCandidateWeight = 0.01; // pYIN's "absolute minimum" fallback
constexpr double logFloor = -1.0e30;

/** Beta(2, 18) prior over thresholds 0.01 .. 1.00 (mean 0.1), normalised. */
std::array<double, numThresholds> thresholdPrior()
{
    std::array<double, numThresholds> w{};
    double sum = 0.0;
    for (std::size_t k = 0; k < numThresholds; ++k)
    {
        const double s = 0.01 * static_cast<double> (k + 1);
        w[k] = s * std::pow (1.0 - s, 17.0); // x^(a-1) (1-x)^(b-1)
        sum += w[k];
    }
    for (auto& v : w)
    {
        v /= sum;
    }
    return w;
}

struct Candidate
{
    double lag = 0.0; // refined lag in samples
    double probability = 0.0;
    double cmndf = 1.0;
};

/** Pitch candidates of one frame from its CMNDF. */
std::vector<Candidate> frameCandidates (const std::vector<double>& d, std::size_t minLag, std::size_t maxLag,
                                        const std::array<double, numThresholds>& prior)
{
    // Local minima of the CMNDF.
    std::vector<std::size_t> minima;
    for (std::size_t tau = minLag; tau <= maxLag; ++tau)
    {
        const bool left = tau == minLag || d[tau] < d[tau - 1];
        const bool right = tau == maxLag || d[tau] <= d[tau + 1];
        if (left && right)
        {
            minima.push_back (tau);
        }
    }
    if (minima.empty())
    {
        return {};
    }

    std::vector<double> mass (minima.size(), 0.0);
    const auto globalMin = static_cast<std::size_t> (
        std::ranges::min_element (minima, {}, [&d] (std::size_t t) { return d[t]; }) - minima.begin());

    for (std::size_t k = 0; k < numThresholds; ++k)
    {
        const double s = 0.01 * static_cast<double> (k + 1);
        const auto it = std::ranges::find_if (minima, [&d, s] (std::size_t t) { return d[t] < s; });
        if (it != minima.end())
        {
            mass[static_cast<std::size_t> (it - minima.begin())] += prior[k];
        }
        else
        {
            mass[globalMin] += prior[k] * noCandidateWeight;
        }
    }

    std::vector<Candidate> out;
    for (std::size_t i = 0; i < minima.size(); ++i)
    {
        if (mass[i] > 0.0)
        {
            out.push_back ({detail::parabolicLag (d, minima[i]), mass[i], d[minima[i]]});
        }
    }
    return out;
}
} // namespace

PitchTrack detectPitchPyin (const audio::MonoAudio& audio, const YinSettings& settings,
                            const ProgressCallback& progress)
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
    const std::size_t window = maxLag;
    const std::size_t frameLength = window + maxLag;

    const auto numBins =
        static_cast<std::size_t> (std::floor (1200.0 * std::log2 (settings.maxHz / settings.minHz) / binCents)) + 1;
    const auto binOf = [&] (double hz)
    {
        const double b = std::round (1200.0 * std::log2 (hz / settings.minHz) / binCents);
        return static_cast<std::size_t> (std::clamp (b, 0.0, static_cast<double> (numBins - 1)));
    };

    detail::YinFrameAnalyser analyser (window, maxLag);
    const auto prior = thresholdPrior();
    std::vector<double> frame (frameLength);

    const auto numFrames = static_cast<std::size_t> (std::floor (audio.durationSeconds() / settings.hopSeconds)) + 1;
    track.frames.resize (numFrames);
    std::vector<std::vector<Candidate>> candidates (numFrames);
    const auto totalSamples = static_cast<std::ptrdiff_t> (audio.size());

    // ---- 1. Candidates per frame (the expensive part: ~90% of the time) ----
    constexpr std::size_t progressInterval = 200;
    for (std::size_t f = 0; f < numFrames; ++f)
    {
        if (progress && f % progressInterval == 0
            && !progress (0.9 * static_cast<double> (f) / static_cast<double> (numFrames)))
        {
            track.frames.resize (f);
            return track; // cancelled
        }

        auto& out = track.frames[f];
        out.timeSeconds = static_cast<double> (f) * settings.hopSeconds;
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
            candidates[f] = frameCandidates (analyser.values(), minLag, maxLag, prior);
        }
    }

    // ---- 2. Viterbi over states: [0, numBins) voiced, [numBins, 2*numBins) unvoiced ----
    const std::size_t numStates = 2 * numBins;
    std::vector<double> transition (static_cast<std::size_t> (maxJumpBins) + 1);
    {
        double sum = 0.0;
        for (int j = -maxJumpBins; j <= maxJumpBins; ++j)
        {
            sum += static_cast<double> (maxJumpBins + 1 - std::abs (j));
        }
        for (int j = 0; j <= maxJumpBins; ++j)
        {
            transition[static_cast<std::size_t> (j)] = std::log (static_cast<double> (maxJumpBins + 1 - j) / sum);
        }
    }
    const double logStay = std::log (1.0 - voicingSwitchProbability);
    const double logSwitch = std::log (voicingSwitchProbability);

    std::vector<double> observation (numStates);
    const auto fillObservation = [&] (std::size_t f)
    {
        std::ranges::fill (observation, 0.0);
        double voicedMass = 0.0;
        for (const auto& c : candidates[f])
        {
            observation[binOf (sr / c.lag)] += c.probability;
            voicedMass += c.probability;
        }
        voicedMass = std::min (voicedMass, 1.0);
        const double unvoiced = (1.0 - voicedMass) / static_cast<double> (numBins);
        for (std::size_t b = 0; b < numBins; ++b)
        {
            observation[b] = observation[b] > 0.0 ? std::log (observation[b]) : logFloor;
            observation[numBins + b] = std::log (std::max (unvoiced, 1e-300));
        }
    };

    std::vector<double> score (numStates);
    std::vector<double> next (numStates);
    std::vector<std::uint16_t> back (numFrames * numStates);

    fillObservation (0);
    for (std::size_t s = 0; s < numStates; ++s)
    {
        score[s] = observation[s] - std::log (static_cast<double> (numStates));
    }

    for (std::size_t f = 1; f < numFrames; ++f)
    {
        fillObservation (f);
        for (std::size_t to = 0; to < numStates; ++to)
        {
            const bool toVoiced = to < numBins;
            const auto toBin = static_cast<int> (toVoiced ? to : to - numBins);
            double best = -std::numeric_limits<double>::infinity();
            std::size_t bestFrom = to;
            const int lo = std::max (0, toBin - maxJumpBins);
            const int hi = std::min (static_cast<int> (numBins) - 1, toBin + maxJumpBins);
            for (int fromBin = lo; fromBin <= hi; ++fromBin)
            {
                const double jump = transition[static_cast<std::size_t> (std::abs (fromBin - toBin))];
                for (const bool fromVoiced : {true, false})
                {
                    const std::size_t from = static_cast<std::size_t> (fromBin) + (fromVoiced ? 0 : numBins);
                    const double v = score[from] + jump + (fromVoiced == toVoiced ? logStay : logSwitch);
                    if (v > best)
                    {
                        best = v;
                        bestFrom = from;
                    }
                }
            }
            next[to] = best + observation[to];
            back[f * numStates + to] = static_cast<std::uint16_t> (bestFrom);
        }
        // Renormalise to keep numbers in range.
        const double top = *std::ranges::max_element (next);
        for (std::size_t s = 0; s < numStates; ++s)
        {
            score[s] = next[s] - top;
        }
    }

    // ---- 3. Backtrack and read the pitch from the best candidate near each bin ----
    std::vector<std::size_t> path (numFrames);
    path[numFrames - 1] = static_cast<std::size_t> (std::ranges::max_element (score) - score.begin());
    for (std::size_t f = numFrames - 1; f > 0; --f)
    {
        path[f - 1] = back[f * numStates + path[f]];
    }

    std::vector<double> estimateTimes (numFrames, 0.0);
    for (std::size_t f = 0; f < numFrames; ++f)
    {
        auto& out = track.frames[f];
        const std::size_t state = path[f];
        if (state >= numBins || candidates[f].empty())
        {
            continue; // unvoiced: no pitch
        }

        const Candidate* best = nullptr;
        for (const auto& c : candidates[f])
        {
            const auto distance = std::abs (static_cast<int> (binOf (sr / c.lag)) - static_cast<int> (state));
            if (distance <= 1 && (best == nullptr || c.probability > best->probability))
            {
                best = &c;
            }
        }
        if (best == nullptr)
        {
            continue;
        }
        out.f0Hz = sr / best->lag;
        out.confidence = std::clamp (1.0 - best->cmndf, 0.0, 1.0);
        estimateTimes[f] = out.timeSeconds + 0.5 * best->lag / sr;
    }

    detail::alignToFrameTimes (track, estimateTimes);
    if (progress)
    {
        progress (1.0);
    }
    return track;
}
} // namespace melisma::analysis
