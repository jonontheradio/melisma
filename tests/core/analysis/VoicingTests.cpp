#include "TestSignals.h"

#include <melisma/core/analysis/Voicing.h>
#include <melisma/core/analysis/Yin.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::classifyFrames;
using melisma::analysis::FrameClass;

namespace
{
struct Run
{
    FrameClass cls;
    double start;
    double end;
};

/** Collapses per-frame classes into runs [start, end). */
std::vector<Run> runsOf (const std::vector<FrameClass>& classes, double hop)
{
    std::vector<Run> runs;
    for (std::size_t i = 0; i < classes.size(); ++i)
    {
        const double t = static_cast<double> (i) * hop;
        if (runs.empty() || runs.back().cls != classes[i])
        {
            runs.push_back ({classes[i], t, t + hop});
        }
        else
        {
            runs.back().end = t + hop;
        }
    }
    return runs;
}

std::vector<Run> voicedRuns (const ts::Signal& s)
{
    const melisma::audio::MonoAudio audio{s.samples, s.sampleRate};
    const auto track = melisma::analysis::detectPitchYin (audio);
    auto runs = runsOf (classifyFrames (audio, track), track.hopSeconds);
    std::erase_if (runs, [] (const Run& r) { return r.cls != FrameClass::voiced; });
    return runs;
}

std::size_t countOf (const std::vector<FrameClass>& classes, FrameClass c)
{
    return static_cast<std::size_t> (std::ranges::count (classes, c));
}
} // namespace

TEST_CASE ("Silence is silence", "[core][analysis][voicing]")
{
    const auto s = ts::silence (0.5);
    const melisma::audio::MonoAudio audio{s.samples, s.sampleRate};
    const auto classes = classifyFrames (audio, melisma::analysis::detectPitchYin (audio));
    CHECK (countOf (classes, FrameClass::silence) == classes.size());
}

TEST_CASE ("Noise is unvoiced", "[core][analysis][voicing]")
{
    const auto s = ts::noise (1.0, 44100.0, 0.2f);
    const melisma::audio::MonoAudio audio{s.samples, s.sampleRate};
    const auto classes = classifyFrames (audio, melisma::analysis::detectPitchYin (audio));
    CHECK (countOf (classes, FrameClass::voiced) == 0);
    CHECK (countOf (classes, FrameClass::unvoiced) > classes.size() * 9 / 10);
}

TEST_CASE ("Each note of a melody is one voiced region", "[core][analysis][voicing]")
{
    const std::vector<ts::Note> notes = {{55.0, 0.4}, {59.0, 0.3}, {62.0, 0.3}, {67.0, 0.5}};
    const auto s = ts::melody (notes, true, 0.1);
    const auto runs = voicedRuns (s);

    REQUIRE (runs.size() == notes.size());
    double expectedStart = 0.0;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        INFO ("note " << i << ": " << runs[i].start << " - " << runs[i].end);
        CHECK (std::abs (runs[i].start - expectedStart) < 0.03);
        CHECK (std::abs (runs[i].end - (expectedStart + notes[i].seconds)) < 0.03);
        expectedStart += notes[i].seconds + 0.1;
    }
}

TEST_CASE ("A consonant between vowels splits voiced regions", "[core][analysis][voicing]")
{
    ts::VowelSettings v;
    v.f0Hz = 196.0;
    auto s = ts::vowel (v, 0.4);
    s.append (ts::noise (0.12, 44100.0, 0.15f)); // "s"-like fricative
    s.append (ts::vowel (v, 0.4));

    const auto runs = voicedRuns (s);
    REQUIRE (runs.size() == 2);
    CHECK (runs[0].end < 0.43);
    CHECK (runs[1].start > 0.49);
}

TEST_CASE ("Vibrato and jitter do not break a sustained note", "[core][analysis][voicing]")
{
    ts::VowelSettings v;
    v.f0Hz = 262.0;
    v.vibratoRateHz = 6.0;
    v.vibratoDepthCents = 60.0;
    v.jitterCents = 15.0;
    auto s = ts::vowel (v, 1.5);
    ts::applyFades (s, 0.02);

    const auto runs = voicedRuns (s);
    REQUIRE (runs.size() == 1);
    CHECK (runs[0].end - runs[0].start > 1.4);
}
