#include "melisma/core/analysis/Voicing.h"

#include <algorithm>
#include <cmath>

namespace melisma::analysis
{
namespace
{
double toDb (double linear)
{
    return 20.0 * std::log10 (std::max (linear, 1e-12));
}

/** Zero crossings per second in a short window around `seconds`. */
double zeroCrossingHz (const audio::MonoAudio& audio, double seconds, double windowSeconds)
{
    const auto half = static_cast<std::ptrdiff_t> (0.5 * windowSeconds * audio.sampleRate);
    const auto centre = static_cast<std::ptrdiff_t> (std::llround (seconds * audio.sampleRate));
    const auto first = std::max<std::ptrdiff_t> (1, centre - half);
    const auto last = std::min<std::ptrdiff_t> (static_cast<std::ptrdiff_t> (audio.size()) - 1, centre + half);
    if (last <= first)
    {
        return 0.0;
    }

    std::size_t crossings = 0;
    for (auto i = first; i <= last; ++i)
    {
        const float a = audio.samples[static_cast<std::size_t> (i - 1)];
        const float b = audio.samples[static_cast<std::size_t> (i)];
        if ((a < 0.0f) != (b < 0.0f))
        {
            ++crossings;
        }
    }
    const double span = static_cast<double> (last - first + 1) / audio.sampleRate;
    return 0.5 * static_cast<double> (crossings) / span; // two crossings per cycle
}

/** Replaces runs of `from` that are shorter than maxFrames and enclosed by
    `boundary` on both sides with `to`. */
void replaceShortRuns (std::vector<FrameClass>& classes, FrameClass from, FrameClass boundary, FrameClass to,
                       std::size_t maxFrames, bool requireBoundaryOnBothSides)
{
    std::size_t i = 0;
    while (i < classes.size())
    {
        if (classes[i] != from)
        {
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < classes.size() && classes[end] == from)
        {
            ++end;
        }
        const bool leftOk = i > 0 && classes[i - 1] == boundary;
        const bool rightOk = end < classes.size() && classes[end] == boundary;
        const bool enclosed = requireBoundaryOnBothSides ? (leftOk && rightOk) : true;
        if (end - i < maxFrames && enclosed)
        {
            std::fill (classes.begin() + static_cast<std::ptrdiff_t> (i),
                       classes.begin() + static_cast<std::ptrdiff_t> (end), to);
        }
        i = end;
    }
}
} // namespace

std::vector<FrameClass> classifyFrames (const audio::MonoAudio& audio, const PitchTrack& track,
                                        const VoicingSettings& settings)
{
    std::vector<FrameClass> classes (track.size(), FrameClass::silence);
    if (track.empty())
    {
        return classes;
    }

    double peakRms = 0.0;
    for (const auto& f : track.frames)
    {
        peakRms = std::max (peakRms, f.rms);
    }
    const double silenceDb = std::max (settings.absoluteSilenceDb, toDb (peakRms) - settings.silenceDbBelowPeak);

    for (std::size_t i = 0; i < track.size(); ++i)
    {
        const auto& f = track.frames[i];
        if (toDb (f.rms) < silenceDb)
        {
            classes[i] = FrameClass::silence;
        }
        else if (f.hasPitch() && f.confidence >= settings.minConfidence
                 && zeroCrossingHz (audio, f.timeSeconds, 0.01) <= settings.maxVoicedZeroCrossingHz)
        {
            classes[i] = FrameClass::voiced;
        }
        else
        {
            classes[i] = FrameClass::unvoiced;
        }
    }

    const auto framesFor = [&track] (double seconds)
    { return static_cast<std::size_t> (std::ceil (seconds / track.hopSeconds - 1e-9)); };

    // Bridge short unvoiced dropouts inside voiced regions, then drop voiced blips.
    replaceShortRuns (classes, FrameClass::unvoiced, FrameClass::voiced, FrameClass::voiced,
                      framesFor (settings.fillGapsSeconds) + 1, true);
    replaceShortRuns (classes, FrameClass::voiced, FrameClass::voiced, FrameClass::unvoiced,
                      framesFor (settings.minVoicedSeconds), false);
    return classes;
}
} // namespace melisma::analysis
