#include "TestSignals.h"

#include <melisma/core/audio/MonoAudio.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <numbers>

using Catch::Approx;
namespace audio = melisma::audio;
namespace ts = melisma::testsignals;

namespace
{
/** Amplitude of the component at `hz` (single-bin DFT over the middle of the signal). */
double amplitudeAt (const audio::MonoAudio& a, double hz)
{
    const std::size_t start = a.size() / 4;
    const std::size_t len = a.size() / 2;
    double re = 0.0;
    double im = 0.0;
    for (std::size_t i = 0; i < len; ++i)
    {
        const double phase = 2.0 * std::numbers::pi * hz * static_cast<double> (start + i) / a.sampleRate;
        re += static_cast<double> (a.samples[start + i]) * std::cos (phase);
        im += static_cast<double> (a.samples[start + i]) * std::sin (phase);
    }
    return 2.0 * std::hypot (re, im) / static_cast<double> (len);
}

audio::MonoAudio toMono (const ts::Signal& s)
{
    return {s.samples, s.sampleRate};
}
} // namespace

TEST_CASE ("Mixdown averages channels", "[core][audio]")
{
    const std::array<float, 3> left = {1.0f, 0.0f, -1.0f};
    const std::array<float, 3> right = {0.0f, 1.0f, -1.0f};
    const std::array<std::span<const float>, 2> channels = {std::span<const float> (left),
                                                            std::span<const float> (right)};

    const auto mono = audio::mixToMono (channels, 48000.0);
    REQUIRE (mono.size() == 3);
    CHECK (mono.sampleRate == 48000.0);
    CHECK (mono.samples[0] == Approx (0.5f));
    CHECK (mono.samples[1] == Approx (0.5f));
    CHECK (mono.samples[2] == Approx (-1.0f));
}

TEST_CASE ("Resampling to the same rate is a copy", "[core][audio]")
{
    const auto in = toMono (ts::sine (440.0, 0.1, 44100.0));
    const auto out = audio::resample (in, 44100.0);
    CHECK (out.samples == in.samples);
}

TEST_CASE ("Resampling keeps frequency, level and duration", "[core][audio]")
{
    for (const double fromRate : {48000.0, 96000.0, 22050.0})
    {
        INFO ("from " << fromRate << " Hz");
        const auto in = toMono (ts::sine (1000.0, 1.0, fromRate, 0.5f));
        const auto out = audio::resample (in, audio::analysisSampleRate);

        CHECK (out.sampleRate == audio::analysisSampleRate);
        CHECK (out.durationSeconds() == Approx (1.0).margin (1e-3));
        CHECK (amplitudeAt (out, 1000.0) == Approx (0.5).margin (0.005));
        CHECK (amplitudeAt (out, 1010.0) < 0.01); // energy is at the right frequency
    }
}

TEST_CASE ("Downsampling removes content above the new Nyquist", "[core][audio]")
{
    // 15 kHz at 48 kHz -> 16 kHz (Nyquist 8 kHz) would alias to 1 kHz without filtering.
    const auto in = toMono (ts::sine (15000.0, 1.0, 48000.0, 0.5f));
    const auto out = audio::resample (in, 16000.0);

    CHECK (amplitudeAt (out, 1000.0) < 0.5 * 0.001); // < -60 dB
}

TEST_CASE ("prepareForAnalysis mixes down and resamples", "[core][audio]")
{
    const auto left = ts::sine (440.0, 0.5, 48000.0, 0.5f);
    const auto right = ts::sine (440.0, 0.5, 48000.0, 0.5f);
    const std::array<std::span<const float>, 2> channels = {std::span<const float> (left.samples),
                                                            std::span<const float> (right.samples)};

    const auto out = audio::prepareForAnalysis (channels, 48000.0);
    CHECK (out.sampleRate == audio::analysisSampleRate);
    CHECK (out.durationSeconds() == Approx (0.5).margin (1e-3));
    CHECK (amplitudeAt (out, 440.0) == Approx (0.5).margin (0.01));
}
