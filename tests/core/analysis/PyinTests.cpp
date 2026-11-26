#include "PitchAccuracy.h"
#include "TestSignals.h"

#include <melisma/core/analysis/Pyin.h>
#include <melisma/core/analysis/Yin.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::detectPitchPyin;
using melisma::analysis::detectPitchYin;
using melisma::testsupport::measure;

namespace
{
melisma::audio::MonoAudio toMono (const ts::Signal& s)
{
    return {s.samples, s.sampleRate};
}

/** Adds white noise at the given signal-to-noise ratio (in dB, RMS based). */
ts::Signal withNoise (ts::Signal s, double snrDb)
{
    double signalPower = 0.0;
    for (const float x : s.samples)
    {
        signalPower += static_cast<double> (x) * static_cast<double> (x);
    }
    signalPower /= static_cast<double> (s.samples.size());
    const double noiseRms = std::sqrt (signalPower / std::pow (10.0, snrDb / 10.0));
    // Uniform noise in [-a, a] has RMS a / sqrt(3).
    const auto n =
        ts::noise (s.durationSeconds(), s.sampleRate, static_cast<float> (noiseRms * std::numbers::sqrt3), 99);
    for (std::size_t i = 0; i < s.samples.size(); ++i)
    {
        s.samples[i] += n.samples[i];
    }
    return s;
}
} // namespace

TEST_CASE ("pYIN is accurate on sines", "[core][analysis][pyin]")
{
    for (const double hz : {80.0, 196.0, 440.0, 880.0})
    {
        const auto s = ts::sine (hz, 0.5);
        const auto acc = measure (detectPitchPyin (toMono (s)), s);
        INFO (hz << " Hz: median " << acc.medianCents << ", max " << acc.maxCents);
        REQUIRE (acc.framesCompared > 50);
        CHECK (acc.framesWithoutPitch == 0);
        CHECK (acc.medianCents < 1.0);
        CHECK (acc.maxCents < 5.0);
    }
}

TEST_CASE ("pYIN tracks vowels with vibrato and a glissando", "[core][analysis][pyin]")
{
    for (const double hz : {98.0, 220.0, 523.0})
    {
        ts::VowelSettings v;
        v.f0Hz = hz;
        v.vibratoRateHz = 5.5;
        v.vibratoDepthCents = 40.0;
        v.jitterCents = 10.0;
        const auto s = ts::vowel (v, 1.0);
        const auto acc = measure (detectPitchPyin (toMono (s)), s);
        INFO (hz << " Hz: median " << acc.medianCents << ", p95 " << acc.p95Cents << ", gross " << acc.grossErrors);
        CHECK (acc.grossErrors == 0);
        CHECK (acc.framesWithoutPitch <= acc.framesCompared / 100);
        CHECK (acc.medianCents < 4.0);
        CHECK (acc.p95Cents < 10.0);
    }

    const auto gliss = ts::glissando (150.0, 600.0, 2.0);
    const auto acc = measure (detectPitchPyin (toMono (gliss)), gliss);
    INFO ("glissando: median " << acc.medianCents << ", p95 " << acc.p95Cents);
    CHECK (acc.grossErrors == 0);
    CHECK (acc.medianCents < 5.0);
}

TEST_CASE ("pYIN decides voicing: silence, noise and melody gaps", "[core][analysis][pyin]")
{
    const auto countPitched = [] (const auto& track)
    { return std::ranges::count_if (track.frames, [] (const auto& f) { return f.hasPitch(); }); };

    CHECK (countPitched (detectPitchPyin (toMono (ts::silence (1.0)))) == 0);

    const auto noisy = detectPitchPyin (toMono (ts::noise (1.0, 44100.0, 0.3f)));
    CHECK (static_cast<std::size_t> (countPitched (noisy)) < noisy.size() / 20);

    const auto s = ts::melody ({{55.0, 0.4}, {62.0, 0.3}, {67.0, 0.5}});
    const auto acc = measure (detectPitchPyin (toMono (s)), s);
    CHECK (acc.grossErrors == 0);
    CHECK (acc.framesWithoutPitch == 0);
}

TEST_CASE ("pYIN is at least as robust as YIN in noise", "[core][analysis][pyin]")
{
    ts::VowelSettings v;
    v.f0Hz = 175.0;
    v.vibratoRateHz = 5.0;
    v.vibratoDepthCents = 30.0;
    const auto s = withNoise (ts::vowel (v, 1.5), 6.0);

    const auto yin = measure (detectPitchYin (toMono (s)), s);
    const auto pyin = measure (detectPitchPyin (toMono (s)), s);
    INFO ("YIN: gross " << yin.grossErrors << ", unvoiced " << yin.framesWithoutPitch << ", median " << yin.medianCents
                        << " | pYIN: gross " << pyin.grossErrors << ", unvoiced " << pyin.framesWithoutPitch
                        << ", median " << pyin.medianCents << " (of " << pyin.framesCompared << ")");

    CHECK (pyin.grossErrors + pyin.framesWithoutPitch <= yin.grossErrors + yin.framesWithoutPitch);
    CHECK (pyin.grossErrors <= pyin.framesCompared / 100);
}

TEST_CASE ("pYIN can be cancelled", "[core][analysis][pyin]")
{
    const auto s = ts::vowel ({}, 3.0);
    const auto track = detectPitchPyin (toMono (s), {}, [] (double p) { return p < 0.2; });
    CHECK (track.size() < 600 / 2);
}
