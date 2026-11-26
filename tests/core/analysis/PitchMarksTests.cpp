#include "TestSignals.h"

#include <melisma/core/Pitch.h>
#include <melisma/core/analysis/PitchMarks.h>
#include <melisma/core/analysis/Yin.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::PitchMarks;

namespace
{
struct Result
{
    PitchMarks marks;
    double medianPeriodErrorPercent = 0.0;
    double maxPeriodErrorPercent = 0.0;
};

/** Detects marks and compares the spacing of consecutive marks with the true period. */
Result analyse (const ts::Signal& s)
{
    const melisma::audio::MonoAudio audio{s.samples, s.sampleRate};
    const auto track = melisma::analysis::detectPitchYin (audio);
    const auto classes = melisma::analysis::classifyFrames (audio, track);

    Result r;
    r.marks = melisma::analysis::detectPitchMarks (audio, track, classes);

    std::vector<double> errors;
    for (std::size_t k = 1; k < r.marks.size(); ++k)
    {
        const auto a = r.marks.positions[k - 1];
        const auto b = r.marks.positions[k];
        const double truthHz = s.f0[(a + b) / 2];
        if (truthHz <= 0.0 || s.f0[a] <= 0.0 || s.f0[b] <= 0.0)
        {
            continue; // spans a gap between notes
        }
        const double truePeriod = s.sampleRate / truthHz;
        errors.push_back (100.0 * std::abs (static_cast<double> (b - a) - truePeriod) / truePeriod);
    }
    if (!errors.empty())
    {
        std::ranges::sort (errors);
        r.medianPeriodErrorPercent = errors[errors.size() / 2];
        r.maxPeriodErrorPercent = errors.back();
    }
    return r;
}
} // namespace

TEST_CASE ("One pitch mark per period on a vowel", "[core][analysis][pitchmarks]")
{
    for (const double hz : {110.0, 220.0, 440.0})
    {
        ts::VowelSettings v;
        v.f0Hz = hz;
        auto s = ts::vowel (v, 0.5);
        ts::applyFades (s, 0.01);
        const auto r = analyse (s);

        INFO (hz << " Hz: " << r.marks.size() << " marks, median error " << r.medianPeriodErrorPercent << "%, max "
                 << r.maxPeriodErrorPercent << "%");
        const double expected = 0.5 * hz;
        CHECK (std::abs (static_cast<double> (r.marks.size()) - expected) <= 0.06 * expected + 2);
        CHECK (r.medianPeriodErrorPercent < 1.0);
        CHECK (r.maxPeriodErrorPercent < 5.0);
        CHECK (std::ranges::is_sorted (r.marks.positions));
    }
}

TEST_CASE ("Pitch marks follow vibrato", "[core][analysis][pitchmarks]")
{
    ts::VowelSettings v;
    v.f0Hz = 196.0;
    v.vibratoRateHz = 5.5;
    v.vibratoDepthCents = 60.0;
    auto s = ts::vowel (v, 1.0);
    ts::applyFades (s, 0.01);
    const auto r = analyse (s);

    INFO ("median " << r.medianPeriodErrorPercent << "%, max " << r.maxPeriodErrorPercent << "%");
    CHECK (r.medianPeriodErrorPercent < 1.0);
    CHECK (r.maxPeriodErrorPercent < 5.0);
}

TEST_CASE ("Pitch marks sit on the same point of every cycle", "[core][analysis][pitchmarks]")
{
    // On a sine every mark must land on a crest, and always the same kind
    // (all positive or all negative peaks: a pure sine has no preferred polarity).
    const auto s = ts::sine (200.0, 0.5, 44100.0, 0.5f);
    const auto r = analyse (s);
    REQUIRE (r.marks.size() > 90);
    const float sign = s.samples[r.marks.positions.front()] > 0.0f ? 1.0f : -1.0f;
    for (const auto m : r.marks.positions)
    {
        CHECK (sign * s.samples[m] > 0.49f);
    }
}

TEST_CASE ("No pitch marks in silence or noise, none between notes", "[core][analysis][pitchmarks]")
{
    CHECK (analyse (ts::silence (0.5)).marks.empty());
    CHECK (analyse (ts::noise (0.5, 44100.0, 0.2f)).marks.empty());

    const auto s = ts::melody ({{60.0, 0.3}, {67.0, 0.3}}, true, 0.15);
    const auto r = analyse (s);
    const auto gapStart = static_cast<std::size_t> (0.32 * s.sampleRate);
    const auto gapEnd = static_cast<std::size_t> (0.43 * s.sampleRate);
    CHECK (std::ranges::none_of (r.marks.positions, [=] (std::size_t m) { return m > gapStart && m < gapEnd; }));
}
