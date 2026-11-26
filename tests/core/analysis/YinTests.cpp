#include "TestSignals.h"

#include <melisma/core/Pitch.h>
#include <melisma/core/analysis/Yin.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::detectPitchYin;
using melisma::analysis::PitchTrack;
using melisma::analysis::YinSettings;

namespace
{
melisma::audio::MonoAudio toMono (const ts::Signal& s)
{
    return {s.samples, s.sampleRate};
}

struct Accuracy
{
    std::size_t framesCompared = 0;
    std::size_t framesWithoutPitch = 0;
    std::size_t grossErrors = 0; // |error| > 50 cents (octave errors, wrong harmonic...)
    double medianCents = 0.0;
    double p95Cents = 0.0;
    double maxCents = 0.0;
};

/** Compares detected f0 with ground truth on frames that are fully voiced
    (truth > 0 for `margin` seconds around the frame). */
Accuracy measure (const PitchTrack& track, const ts::Signal& truth, double margin = 0.03)
{
    Accuracy acc;
    std::vector<double> errors;
    for (const auto& frame : track.frames)
    {
        const double t = frame.timeSeconds;
        if (truth.f0At (t - margin) <= 0.0 || truth.f0At (t) <= 0.0 || truth.f0At (t + margin) <= 0.0)
        {
            continue;
        }
        ++acc.framesCompared;
        if (!frame.hasPitch())
        {
            ++acc.framesWithoutPitch;
            continue;
        }
        const double cents = std::abs (melisma::pitch::centsBetween (truth.f0At (t), frame.f0Hz));
        if (cents > 50.0)
        {
            ++acc.grossErrors;
        }
        errors.push_back (cents);
    }

    if (!errors.empty())
    {
        std::ranges::sort (errors);
        acc.medianCents = errors[errors.size() / 2];
        acc.p95Cents = errors[static_cast<std::size_t> (0.95 * static_cast<double> (errors.size() - 1))];
        acc.maxCents = errors.back();
    }
    return acc;
}

std::size_t framesWithPitch (const PitchTrack& track)
{
    return static_cast<std::size_t> (std::ranges::count_if (track.frames, [] (const auto& f) { return f.hasPitch(); }));
}
} // namespace

TEST_CASE ("YIN frame layout", "[core][analysis][yin]")
{
    const auto s = ts::sine (220.0, 1.0);
    const auto track = detectPitchYin (toMono (s), YinSettings{.hopSeconds = 0.01});

    CHECK (track.hopSeconds == 0.01);
    CHECK (track.size() == 101); // frames at 0, 10 ms, ..., 1000 ms
    CHECK (track.frames[50].timeSeconds == 0.5);
    CHECK (track.frameIndexAt (0.504) == 50);
    CHECK (track.frameIndexAt (99.0) == 100);
}

TEST_CASE ("YIN is accurate to < 5 cents on sines from 80 Hz to 1 kHz", "[core][analysis][yin]")
{
    for (const double hz : {80.0, 110.0, 196.0, 261.63, 440.0, 659.26, 880.0, 1000.0})
    {
        const auto s = ts::sine (hz, 0.5);
        const auto acc = measure (detectPitchYin (toMono (s), YinSettings{.maxHz = 1100.0}), s);

        INFO (hz << " Hz: median " << acc.medianCents << ", max " << acc.maxCents << " cents");
        REQUIRE (acc.framesCompared > 50);
        CHECK (acc.framesWithoutPitch == 0);
        CHECK (acc.medianCents < 1.0);
        CHECK (acc.maxCents < 5.0);
    }
}

TEST_CASE ("YIN tracks sung-like vowels without octave errors", "[core][analysis][yin]")
{
    for (const double hz : {98.0, 147.0, 220.0, 330.0, 523.0})
    {
        ts::VowelSettings v;
        v.f0Hz = hz;
        v.vibratoRateHz = 5.5;
        v.vibratoDepthCents = 40.0;
        v.jitterCents = 10.0;
        const auto s = ts::vowel (v, 1.0);
        const auto acc = measure (detectPitchYin (toMono (s)), s);

        INFO (hz << " Hz: median " << acc.medianCents << ", p95 " << acc.p95Cents << ", gross " << acc.grossErrors
                 << ", unvoiced " << acc.framesWithoutPitch << " / " << acc.framesCompared);
        REQUIRE (acc.framesCompared > 100);
        CHECK (acc.grossErrors == 0);
        CHECK (acc.framesWithoutPitch <= acc.framesCompared / 100);
        CHECK (acc.medianCents < 4.0);
        CHECK (acc.p95Cents < 10.0);
    }
}

TEST_CASE ("YIN follows a two-octave glissando", "[core][analysis][yin]")
{
    const auto s = ts::glissando (150.0, 600.0, 2.0);
    const auto acc = measure (detectPitchYin (toMono (s)), s);

    INFO ("median " << acc.medianCents << ", p95 " << acc.p95Cents);
    CHECK (acc.grossErrors == 0);
    CHECK (acc.medianCents < 5.0);
    CHECK (acc.p95Cents < 15.0);
}

TEST_CASE ("YIN reports no pitch for silence and very little for noise", "[core][analysis][yin]")
{
    const auto quiet = detectPitchYin (toMono (ts::silence (1.0)));
    CHECK (framesWithPitch (quiet) == 0);

    const auto noisy = detectPitchYin (toMono (ts::noise (1.0, 44100.0, 0.3f)));
    CHECK (framesWithPitch (noisy) < noisy.size() / 20);
}

TEST_CASE ("YIN finds the notes of a melody", "[core][analysis][yin]")
{
    const std::vector<ts::Note> notes = {{55.0, 0.4}, {59.0, 0.3}, {62.0, 0.3}, {67.0, 0.5}, {64.0, 0.4}};
    const auto s = ts::melody (notes);
    const auto acc = measure (detectPitchYin (toMono (s)), s);

    INFO ("median " << acc.medianCents << ", gross " << acc.grossErrors << " / " << acc.framesCompared);
    CHECK (acc.grossErrors == 0);
    CHECK (acc.framesWithoutPitch == 0);
    CHECK (acc.medianCents < 2.0);
}

TEST_CASE ("YIN handles empty input and invalid settings", "[core][analysis][yin]")
{
    CHECK (detectPitchYin ({}).empty());
    CHECK (detectPitchYin (toMono (ts::sine (220.0, 0.1)), YinSettings{.minHz = 500.0, .maxHz = 100.0}).empty());
}
