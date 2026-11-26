#pragma once

#include <array>
#include <cstdint>
#include <vector>

/** Synthetic signals with known ground truth, used to measure the accuracy of
    pitch detection, segmentation and resynthesis. Everything is deterministic
    (noise uses a fixed seed) so test results are reproducible. */
namespace melisma::testsignals
{
/** A generated signal and its ground truth. */
struct Signal
{
    std::vector<float> samples;
    double sampleRate = 44100.0;

    /** True fundamental frequency at each sample, in Hz. 0 = unvoiced or silent. */
    std::vector<double> f0;

    double durationSeconds() const { return static_cast<double> (samples.size()) / sampleRate; }

    /** Ground-truth f0 at a time in seconds (0 when out of range). */
    double f0At (double seconds) const;

    /** Appends another signal recorded at the same sample rate. */
    void append (const Signal& other);
};

/** Pure sine at a fixed frequency. */
Signal sine (double hz, double seconds, double sampleRate = 44100.0, float amplitude = 0.5f);

/** Sine with sinusoidal vibrato. depthCents is the peak deviation. */
Signal vibratoSine (double centreHz, double vibratoRateHz, double depthCents, double seconds,
                    double sampleRate = 44100.0, float amplitude = 0.5f);

/** Exponential (constant cents per second) glide from startHz to endHz. */
Signal glissando (double startHz, double endHz, double seconds, double sampleRate = 44100.0, float amplitude = 0.5f);

/** Digital silence. */
Signal silence (double seconds, double sampleRate = 44100.0);

/** White noise (unvoiced, f0 = 0) with a fixed seed. */
Signal noise (double seconds, double sampleRate = 44100.0, float amplitude = 0.1f, std::uint32_t seed = 1234);

/** Vowel-like voiced sound: band-limited glottal pulse train shaped by formant
    resonators. Closer to a real voice than a sine: rich harmonics and a
    spectral envelope that does not follow the pitch. */
struct VowelSettings
{
    double f0Hz = 220.0;
    std::array<double, 3> formantsHz = {730.0, 1090.0, 2440.0}; // vowel "a"
    std::array<double, 3> bandwidthsHz = {90.0, 110.0, 170.0};
    double vibratoRateHz = 0.0;
    double vibratoDepthCents = 0.0;
    double jitterCents = 0.0; // random slow pitch wander (seeded)
    float amplitude = 0.5f;
};

Signal vowel (const VowelSettings& settings, double seconds, double sampleRate = 44100.0);

/** One note of a melody. */
struct Note
{
    double midi = 60.0;
    double seconds = 0.5;
};

/** A monophonic melody of sine or vowel notes separated by short silences.
    Notes get short fades so their edges are clean. */
Signal melody (const std::vector<Note>& notes, bool useVowel = true, double gapSeconds = 0.08,
               double sampleRate = 44100.0);

/** Applies linear fade-in/out of the given length in place. */
void applyFades (Signal& signal, double fadeSeconds);
} // namespace melisma::testsignals
