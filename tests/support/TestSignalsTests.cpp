#include "TestSignals.h"

#include <melisma/core/Pitch.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

using Catch::Approx;
namespace ts = melisma::testsignals;

namespace
{
/** Reference f0 estimate for checking the generators themselves: picks the
    autocorrelation peak in a 60-1100 Hz range with parabolic interpolation. */
double autocorrelationF0 (const ts::Signal& s, double fromSeconds, double windowSeconds)
{
    const auto start = static_cast<std::size_t> (fromSeconds * s.sampleRate);
    const auto len = static_cast<std::size_t> (windowSeconds * s.sampleRate);
    const auto minLag = static_cast<std::size_t> (s.sampleRate / 1100.0);
    const auto maxLag = static_cast<std::size_t> (s.sampleRate / 60.0);

    std::vector<double> r (maxLag + 2, 0.0);
    for (std::size_t lag = minLag - 1; lag <= maxLag + 1; ++lag)
    {
        double acc = 0.0;
        for (std::size_t i = 0; i < len; ++i)
        {
            acc += static_cast<double> (s.samples[start + i]) * static_cast<double> (s.samples[start + i + lag]);
        }
        r[lag] = acc;
    }

    std::size_t best = minLag;
    for (std::size_t lag = minLag; lag <= maxLag; ++lag)
    {
        if (r[lag] > r[best])
        {
            best = lag;
        }
    }

    // Prefer the shortest lag whose peak is close to the best (avoids octave errors).
    for (std::size_t lag = minLag; lag < best; ++lag)
    {
        if (r[lag] > 0.9 * r[best] && r[lag] >= r[lag - 1] && r[lag] >= r[lag + 1])
        {
            best = lag;
            break;
        }
    }

    const double a = r[best - 1];
    const double b = r[best];
    const double c = r[best + 1];
    const double denom = a - 2.0 * b + c;
    const double offset = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
    return s.sampleRate / (static_cast<double> (best) + offset);
}

float peakAbs (const ts::Signal& s)
{
    float peak = 0.0f;
    for (const float x : s.samples)
    {
        peak = std::max (peak, std::abs (x));
    }
    return peak;
}
} // namespace

TEST_CASE ("Sine has the requested length, amplitude and pitch", "[testsignals]")
{
    const auto s = ts::sine (220.0, 1.0, 48000.0, 0.5f);

    CHECK (s.samples.size() == 48000);
    CHECK (s.f0.size() == s.samples.size());
    CHECK (s.durationSeconds() == Approx (1.0));
    CHECK (peakAbs (s) == Approx (0.5f).margin (1e-3));
    CHECK (std::abs (melisma::pitch::centsBetween (220.0, autocorrelationF0 (s, 0.2, 0.05))) < 2.0);
}

TEST_CASE ("Vibrato ground truth stays within the requested depth", "[testsignals]")
{
    const auto s = ts::vibratoSine (300.0, 5.5, 50.0, 1.0);

    double minCents = 0.0;
    double maxCents = 0.0;
    for (const double hz : s.f0)
    {
        const double c = melisma::pitch::centsBetween (300.0, hz);
        minCents = std::min (minCents, c);
        maxCents = std::max (maxCents, c);
    }
    CHECK (maxCents == Approx (50.0).margin (0.5));
    CHECK (minCents == Approx (-50.0).margin (0.5));
}

TEST_CASE ("Glissando goes from start to end exponentially", "[testsignals]")
{
    const auto s = ts::glissando (200.0, 400.0, 2.0);

    CHECK (s.f0.front() == Approx (200.0));
    CHECK (s.f0.back() == Approx (400.0));
    CHECK (s.f0At (1.0) == Approx (200.0 * std::sqrt (2.0)).epsilon (1e-3)); // half an octave
}

TEST_CASE ("Silence and noise are unvoiced", "[testsignals]")
{
    const auto quiet = ts::silence (0.5);
    CHECK (peakAbs (quiet) == 0.0f);

    const auto n1 = ts::noise (0.5);
    const auto n2 = ts::noise (0.5);
    CHECK (peakAbs (n1) > 0.05f);
    CHECK (n1.samples == n2.samples); // deterministic
    CHECK (std::ranges::all_of (n1.f0, [] (double f) { return f == 0.0; }));
}

TEST_CASE ("Vowel is periodic at its f0 and normalised", "[testsignals]")
{
    for (const double hz : {110.0, 220.0, 440.0})
    {
        ts::VowelSettings v;
        v.f0Hz = hz;
        const auto s = ts::vowel (v, 0.6);

        CHECK (peakAbs (s) == Approx (0.5f).margin (1e-3));
        const double measured = autocorrelationF0 (s, 0.25, 0.06);
        INFO ("f0 = " << hz << ", measured = " << measured);
        CHECK (std::abs (melisma::pitch::centsBetween (hz, measured)) < 5.0);
    }
}

TEST_CASE ("Melody has one voiced region per note with the right pitch", "[testsignals]")
{
    const std::vector<ts::Note> notes = {{57.0, 0.4}, {60.0, 0.3}, {64.0, 0.5}};
    const auto s = ts::melody (notes, true, 0.1);

    CHECK (s.durationSeconds() == Approx (0.4 + 0.3 + 0.5 + 3 * 0.1).margin (1e-3));

    int regions = 0;
    bool voiced = false;
    for (const double f : s.f0)
    {
        if (f > 0.0 && !voiced)
        {
            ++regions;
        }
        voiced = f > 0.0;
    }
    CHECK (regions == 3);

    CHECK (s.f0At (0.2) == Approx (melisma::pitch::midiToHz (57.0)));
    CHECK (s.f0At (0.65) == Approx (melisma::pitch::midiToHz (60.0)));
    CHECK (s.f0At (1.1) == Approx (melisma::pitch::midiToHz (64.0)));
    CHECK (s.f0At (0.45) == 0.0); // gap
}
