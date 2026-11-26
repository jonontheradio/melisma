#include "TestSignals.h"

#include <melisma/core/Pitch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>

namespace melisma::testsignals
{
namespace
{
constexpr double twoPi = 2.0 * std::numbers::pi;

std::size_t numSamples (double seconds, double sampleRate)
{
    return static_cast<std::size_t> (std::llround (std::max (0.0, seconds) * sampleRate));
}

/** Two-pole resonator (formant filter), unity gain at its centre frequency. */
class Resonator
{
public:
    Resonator (double centreHz, double bandwidthHz, double sampleRate)
    {
        const double r = std::exp (-std::numbers::pi * bandwidthHz / sampleRate);
        const double theta = twoPi * centreHz / sampleRate;
        a1 = 2.0 * r * std::cos (theta);
        a2 = -r * r;
        gain = (1.0 - r) * std::sqrt (1.0 - 2.0 * r * std::cos (2.0 * theta) + r * r);
    }

    double process (double x)
    {
        const double y = gain * x + a1 * y1 + a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }

private:
    double a1 = 0.0, a2 = 0.0, gain = 1.0;
    double y1 = 0.0, y2 = 0.0;
};
} // namespace

double Signal::f0At (double seconds) const
{
    if (seconds < 0.0 || f0.empty())
    {
        return 0.0;
    }

    const auto index = static_cast<std::size_t> (seconds * sampleRate);
    return index < f0.size() ? f0[index] : 0.0;
}

void Signal::append (const Signal& other)
{
    samples.insert (samples.end(), other.samples.begin(), other.samples.end());
    f0.insert (f0.end(), other.f0.begin(), other.f0.end());
}

Signal sine (double hz, double seconds, double sampleRate, float amplitude)
{
    return vibratoSine (hz, 0.0, 0.0, seconds, sampleRate, amplitude);
}

Signal vibratoSine (double centreHz, double vibratoRateHz, double depthCents, double seconds, double sampleRate,
                    float amplitude)
{
    Signal s;
    s.sampleRate = sampleRate;
    const auto n = numSamples (seconds, sampleRate);
    s.samples.resize (n);
    s.f0.resize (n);

    double phase = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sampleRate;
        const double hz = centreHz * pitch::centsToRatio (depthCents * std::sin (twoPi * vibratoRateHz * t));
        s.f0[i] = hz;
        s.samples[i] = amplitude * static_cast<float> (std::sin (phase));
        phase = std::fmod (phase + twoPi * hz / sampleRate, twoPi);
    }
    return s;
}

Signal glissando (double startHz, double endHz, double seconds, double sampleRate, float amplitude)
{
    Signal s;
    s.sampleRate = sampleRate;
    const auto n = numSamples (seconds, sampleRate);
    s.samples.resize (n);
    s.f0.resize (n);

    double phase = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double progress = n > 1 ? static_cast<double> (i) / static_cast<double> (n - 1) : 0.0;
        const double hz = startHz * std::pow (endHz / startHz, progress);
        s.f0[i] = hz;
        s.samples[i] = amplitude * static_cast<float> (std::sin (phase));
        phase = std::fmod (phase + twoPi * hz / sampleRate, twoPi);
    }
    return s;
}

Signal silence (double seconds, double sampleRate)
{
    Signal s;
    s.sampleRate = sampleRate;
    const auto n = numSamples (seconds, sampleRate);
    s.samples.assign (n, 0.0f);
    s.f0.assign (n, 0.0);
    return s;
}

Signal noise (double seconds, double sampleRate, float amplitude, std::uint32_t seed)
{
    Signal s = silence (seconds, sampleRate);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
    for (auto& x : s.samples)
    {
        x = amplitude * dist (rng);
    }
    return s;
}

Signal vowel (const VowelSettings& settings, double seconds, double sampleRate)
{
    Signal s;
    s.sampleRate = sampleRate;
    const auto n = numSamples (seconds, sampleRate);
    s.samples.resize (n);
    s.f0.resize (n);

    std::array<Resonator, 3> formants = {
        Resonator{settings.formantsHz[0], settings.bandwidthsHz[0], sampleRate},
        Resonator{settings.formantsHz[1], settings.bandwidthsHz[1], sampleRate},
        Resonator{settings.formantsHz[2], settings.bandwidthsHz[2], sampleRate},
    };

    // Slow random pitch wander: smoothed random walk. The fixed seed is intended:
    // test signals must be identical on every run.
    std::mt19937 rng (42); // NOLINT(bugprone-random-generator-seed)
    std::normal_distribution<double> step (0.0, 1.0);
    double wander = 0.0;
    const double wanderSmoothing = std::exp (-twoPi * 3.0 / sampleRate); // ~3 Hz

    double phase = 0.0; // in cycles
    std::vector<double> raw (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sampleRate;
        wander = wanderSmoothing * wander + (1.0 - wanderSmoothing) * step (rng) * 40.0;
        const double cents = settings.vibratoDepthCents * std::sin (twoPi * settings.vibratoRateHz * t)
                             + settings.jitterCents * std::clamp (wander, -1.0, 1.0);
        const double hz = settings.f0Hz * pitch::centsToRatio (cents);
        s.f0[i] = hz;

        // Band-limited sawtooth (additive, harmonics below Nyquist) as glottal source.
        double source = 0.0;
        const int harmonics = static_cast<int> ((0.45 * sampleRate) / hz);
        for (int h = 1; h <= harmonics; ++h)
        {
            source += std::sin (twoPi * phase * h) / h;
        }

        double y = 0.0;
        for (auto& f : formants)
        {
            y += f.process (source);
        }
        raw[i] = y;

        phase += hz / sampleRate;
        phase -= std::floor (phase);
    }

    double peak = 0.0;
    for (const double v : raw)
    {
        peak = std::max (peak, std::abs (v));
    }
    const double scale = peak > 0.0 ? settings.amplitude / peak : 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        s.samples[i] = static_cast<float> (raw[i] * scale);
    }
    return s;
}

Signal melody (const std::vector<Note>& notes, bool useVowel, double gapSeconds, double sampleRate)
{
    Signal out = silence (0.0, sampleRate);
    for (const auto& note : notes)
    {
        const double hz = pitch::midiToHz (note.midi);
        Signal part;
        if (useVowel)
        {
            VowelSettings v;
            v.f0Hz = hz;
            part = vowel (v, note.seconds, sampleRate);
        }
        else
        {
            part = sine (hz, note.seconds, sampleRate);
        }
        applyFades (part, 0.01);
        out.append (part);
        out.append (silence (gapSeconds, sampleRate));
    }
    return out;
}

void applyFades (Signal& signal, double fadeSeconds)
{
    const auto n = signal.samples.size();
    const auto fade = std::min (numSamples (fadeSeconds, signal.sampleRate), n / 2);
    for (std::size_t i = 0; i < fade; ++i)
    {
        const float g = static_cast<float> (i) / static_cast<float> (fade);
        signal.samples[i] *= g;
        signal.samples[n - 1 - i] *= g;
    }
}
} // namespace melisma::testsignals
