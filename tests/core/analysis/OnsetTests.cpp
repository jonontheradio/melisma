#include "TestSignals.h"

#include <melisma/core/analysis/Analysis.h>
#include <melisma/core/analysis/Onsets.h>
#include <melisma/core/analysis/Yin.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::detectOnsets;
using melisma::analysis::OnsetSettings;

namespace
{
melisma::audio::MonoAudio toMono (const ts::Signal& s)
{
    return {s.samples, s.sampleRate};
}

/** Three re-articulated notes of the same pitch ("la-la-la"): one continuous
    legato vowel whose level dips to `dipGain` between notes. */
ts::Signal repeatedNotes (double dipGain, double noteSeconds = 0.4)
{
    auto s = ts::silence (0.1);
    s.append (ts::legatoMelody ({{62.0, noteSeconds}, {62.0, noteSeconds}, {62.0, noteSeconds}}));
    s.append (ts::silence (0.1));

    const auto dipCentres = std::vector<double>{0.1 + noteSeconds, 0.1 + 2 * noteSeconds};
    for (std::size_t i = 0; i < s.samples.size(); ++i)
    {
        const double t = static_cast<double> (i) / s.sampleRate;
        double gain = 1.0;
        for (const double c : dipCentres)
        {
            const double d = std::abs (t - c) / 0.04; // 40 ms either side
            if (d < 1.0)
            {
                gain = std::min (gain, dipGain + (1.0 - dipGain) * d * d);
            }
        }
        s.samples[i] *= static_cast<float> (gain);
    }
    return s;
}

std::string describe (const std::vector<double>& onsets)
{
    std::string out;
    for (const double t : onsets)
    {
        out += std::to_string (t) + " ";
    }
    return out;
}

std::vector<double> onsetsInside (const std::vector<double>& onsets, double from, double to)
{
    std::vector<double> out;
    std::ranges::copy_if (onsets, std::back_inserter (out), [from, to] (double t) { return t > from && t < to; });
    return out;
}
} // namespace

TEST_CASE ("Onset settings follow sensitivity", "[core][analysis][onsets]")
{
    CHECK (OnsetSettings{.sensitivity = 0.0}.energyRiseDb() > OnsetSettings{.sensitivity = 1.0}.energyRiseDb());
    CHECK (OnsetSettings{.sensitivity = 0.0}.fluxThreshold() > OnsetSettings{.sensitivity = 1.0}.fluxThreshold());
}

TEST_CASE ("Re-articulated notes of the same pitch are found", "[core][analysis][onsets]")
{
    const auto s = repeatedNotes (0.1);
    const auto audio = toMono (s);
    const auto onsets = detectOnsets (audio, melisma::analysis::detectPitchYin (audio));

    const auto inside = onsetsInside (onsets, 0.2, 1.25);
    INFO ("onsets: " << describe (onsets));
    REQUIRE (inside.size() == 2);
    CHECK (std::abs (inside[0] - 0.5) < 0.04);
    CHECK (std::abs (inside[1] - 0.9) < 0.04);
}

TEST_CASE ("A sustained note with vibrato has no onsets inside", "[core][analysis][onsets]")
{
    ts::VowelSettings v;
    v.f0Hz = 247.0;
    v.vibratoRateHz = 5.5;
    v.vibratoDepthCents = 60.0;
    v.jitterCents = 15.0;
    auto s = ts::silence (0.1);
    s.append (ts::vowel (v, 1.5));
    s.append (ts::silence (0.1));
    ts::applyFades (s, 0.02);

    const auto audio = toMono (s);
    const auto onsets = detectOnsets (audio, melisma::analysis::detectPitchYin (audio));
    CHECK (onsetsInside (onsets, 0.2, 1.55).empty());
}

TEST_CASE ("Shallow dips only count at high sensitivity", "[core][analysis][onsets]")
{
    const auto audio = toMono (repeatedNotes (0.4)); // ~8 dB dips
    const auto track = melisma::analysis::detectPitchYin (audio);

    CHECK (onsetsInside (detectOnsets (audio, track, {.sensitivity = 0.0}), 0.2, 1.25).empty());
    CHECK (onsetsInside (detectOnsets (audio, track, {.sensitivity = 1.0}), 0.2, 1.25).size() == 2);
}

TEST_CASE ("The pipeline splits repeated notes using onsets", "[core][analysis][onsets]")
{
    const auto take = melisma::analysis::analyseTake (toMono (repeatedNotes (0.1)));
    REQUIRE (take.has_value());
    REQUIRE (take->notes().size() == 3);
    for (const auto& n : take->notes())
    {
        CHECK (std::abs (n.detectedMidi - 62.0) < 0.1);
    }
}
