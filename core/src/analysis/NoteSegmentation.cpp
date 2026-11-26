#include "melisma/core/analysis/NoteSegmentation.h"

#include "melisma/core/Pitch.h"

#include <algorithm>
#include <cmath>

namespace melisma::analysis
{
namespace
{
/** Half-open frame range [first, last). */
struct Range
{
    std::size_t first = 0;
    std::size_t last = 0;
    std::size_t size() const { return last - first; }
};

double median (std::vector<double> values)
{
    if (values.empty())
    {
        return 0.0;
    }
    const auto mid = values.begin() + static_cast<std::ptrdiff_t> (values.size() / 2);
    std::nth_element (values.begin(), mid, values.end());
    return *mid;
}

double medianOf (const std::vector<double>& midi, std::size_t first, std::size_t last)
{
    return median (std::vector<double> (midi.begin() + static_cast<std::ptrdiff_t> (first),
                                        midi.begin() + static_cast<std::ptrdiff_t> (last)));
}

/** Median-filtered MIDI pitch over a voiced range (radius 2 frames). */
std::vector<double> smoothedMidi (const PitchTrack& track, Range r)
{
    std::vector<double> raw (r.size());
    for (std::size_t i = 0; i < r.size(); ++i)
    {
        raw[i] = pitch::hzToMidi (track.frames[r.first + i].f0Hz);
    }

    std::vector<double> out (r.size());
    constexpr std::size_t radius = 2;
    for (std::size_t i = 0; i < r.size(); ++i)
    {
        const std::size_t a = i >= radius ? i - radius : 0;
        const std::size_t b = std::min (r.size(), i + radius + 1);
        out[i] = medianOf (raw, a, b);
    }
    return out;
}

/** Centred moving average (edges use the frames available). Removes vibrato
    when the window spans about one vibrato period. */
std::vector<double> movingAverage (const std::vector<double>& x, std::size_t radius)
{
    std::vector<double> prefix (x.size() + 1, 0.0);
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        prefix[i + 1] = prefix[i] + x[i];
    }
    std::vector<double> out (x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        const std::size_t a = i >= radius ? i - radius : 0;
        const std::size_t b = std::min (x.size(), i + radius + 1);
        out[i] = (prefix[b] - prefix[a]) / static_cast<double> (b - a);
    }
    return out;
}

/** Moves a split point to where the detailed pitch curve changes fastest,
    searching `radius` frames around it. */
std::size_t refineBoundary (const std::vector<double>& detail, std::size_t split, std::size_t radius, std::size_t lo,
                            std::size_t hi)
{
    const std::size_t a = std::max (lo + 1, split > radius ? split - radius : 0);
    const std::size_t b = std::min (hi, split + radius + 1);
    std::size_t best = split;
    double bestSlope = -1.0;
    for (std::size_t i = a; i < b; ++i)
    {
        const double slope = std::abs (detail[i] - detail[i - 1]);
        if (slope > bestSlope)
        {
            bestSlope = slope;
            best = i;
        }
    }
    return best;
}

/** Splits one voiced range at sustained pitch changes. Ranges are relative to `midi`. */
std::vector<Range> splitAtPitchChanges (const std::vector<double>& midi, double splitSemitones, std::size_t holdFrames)
{
    std::vector<Range> notes;
    std::size_t noteStart = 0;
    constexpr std::size_t referenceFrames = 60; // ~300 ms: follows slow drift, not transitions

    std::size_t deviationStart = 0;
    int deviationSide = 0;

    for (std::size_t i = 1; i < midi.size(); ++i)
    {
        const std::size_t refFirst = std::max (noteStart, i > referenceFrames ? i - referenceFrames : 0);
        const std::size_t refLast = deviationSide != 0 ? deviationStart : i;
        const double reference = medianOf (midi, refFirst, std::max (refLast, refFirst + 1));
        const double diff = midi[i] - reference;
        int side = 0;
        if (diff > splitSemitones)
        {
            side = 1;
        }
        else if (diff < -splitSemitones)
        {
            side = -1;
        }

        if (side == 0)
        {
            deviationSide = 0;
            continue;
        }
        if (side != deviationSide)
        {
            deviationSide = side;
            deviationStart = i;
        }
        if (i + 1 - deviationStart >= holdFrames && deviationStart > noteStart)
        {
            notes.push_back ({noteStart, deviationStart});
            noteStart = deviationStart;
            deviationSide = 0;
        }
    }
    notes.push_back ({noteStart, midi.size()});
    return notes;
}

/** Merges notes shorter than minFrames into their closest-in-pitch neighbour. */
void mergeShortNotes (std::vector<Range>& notes, const std::vector<double>& midi, std::size_t minFrames)
{
    bool changed = true;
    while (changed && notes.size() > 1)
    {
        changed = false;
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            if (notes[i].size() >= minFrames)
            {
                continue;
            }
            const double p = medianOf (midi, notes[i].first, notes[i].last);
            const bool hasPrev = i > 0;
            const bool hasNext = i + 1 < notes.size();
            const double dPrev = hasPrev ? std::abs (p - medianOf (midi, notes[i - 1].first, notes[i - 1].last)) : 1e9;
            const double dNext = hasNext ? std::abs (p - medianOf (midi, notes[i + 1].first, notes[i + 1].last)) : 1e9;

            if (dPrev <= dNext)
            {
                notes[i - 1].last = notes[i].last;
            }
            else
            {
                notes[i + 1].first = notes[i].first;
            }
            notes.erase (notes.begin() + static_cast<std::ptrdiff_t> (i));
            changed = true;
            break;
        }
    }
}

/** Energy-weighted mean pitch over the middle 60% of the note (edges hold the
    transitions, which would bias the centre). */
double centrePitch (const PitchTrack& track, const std::vector<double>& midi, std::size_t offset, Range note)
{
    std::size_t first = note.first;
    std::size_t last = note.last;
    if (note.size() >= 5)
    {
        const std::size_t trim = note.size() / 5;
        first += trim;
        last -= trim;
    }

    double weighted = 0.0;
    double weights = 0.0;
    for (std::size_t i = first; i < last; ++i)
    {
        const double w = track.frames[offset + i].rms;
        weighted += w * midi[i];
        weights += w;
    }
    return weights > 0.0 ? weighted / weights : medianOf (midi, note.first, note.last);
}
} // namespace

double SegmentationSettings::splitSemitones() const
{
    const double s = std::clamp (sensitivity, 0.0, 1.0);
    return 1.0 + s * (0.35 - 1.0); // 1.0 st (low) .. 0.35 st (high)
}

double SegmentationSettings::holdSeconds() const
{
    const double s = std::clamp (sensitivity, 0.0, 1.0);
    return 0.06 + s * (0.025 - 0.06); // 60 ms .. 25 ms
}

std::vector<model::Note> segmentNotes (const PitchTrack& track, const std::vector<FrameClass>& classes,
                                       const SegmentationSettings& settings)
{
    std::vector<model::Note> result;
    const std::size_t n = std::min (track.size(), classes.size());
    if (n == 0 || track.hopSeconds <= 0.0)
    {
        return result;
    }

    const double hop = track.hopSeconds;
    const auto framesFor = [hop] (double seconds)
    { return std::max<std::size_t> (1, static_cast<std::size_t> (std::lround (seconds / hop))); };
    const std::size_t holdFrames = framesFor (settings.holdSeconds());
    const std::size_t minFrames = framesFor (settings.minNoteSeconds);

    std::size_t i = 0;
    while (i < n)
    {
        // Voiced frames with a pitch form a region.
        if (classes[i] != FrameClass::voiced || !track.frames[i].hasPitch())
        {
            ++i;
            continue;
        }
        Range region{i, i};
        while (region.last < n && classes[region.last] == FrameClass::voiced && track.frames[region.last].hasPitch())
        {
            ++region.last;
        }
        i = region.last;

        if (region.size() < minFrames)
        {
            continue; // isolated blip
        }

        const auto midi = smoothedMidi (track, region);

        // Detect changes on a vibrato-free trend, then place each boundary where
        // the detailed curve moves fastest.
        const std::size_t trendRadius = framesFor (0.09); // ~180 ms window: one vibrato period
        const auto trend = movingAverage (midi, trendRadius);
        auto notes = splitAtPitchChanges (trend, settings.splitSemitones(), holdFrames);
        for (std::size_t k = 1; k < notes.size(); ++k)
        {
            const auto boundary = refineBoundary (midi, notes[k].first, trendRadius, notes[k - 1].first, notes[k].last);
            notes[k - 1].last = boundary;
            notes[k].first = boundary;
        }
        std::erase_if (notes, [] (const Range& r) { return r.size() == 0; });
        mergeShortNotes (notes, midi, minFrames);

        for (const auto& r : notes)
        {
            model::Note note;
            note.startSeconds = std::max (0.0, track.frames[region.first + r.first].timeSeconds - 0.5 * hop);
            note.endSeconds = track.frames[region.first + r.last - 1].timeSeconds + 0.5 * hop;
            note.detectedMidi = centrePitch (track, midi, region.first, r);
            result.push_back (note);
        }
    }
    return result;
}
} // namespace melisma::analysis
