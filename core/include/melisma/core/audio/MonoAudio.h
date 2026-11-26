#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace melisma::audio
{
/** Sample rate every analysis and resynthesis step works at. Sources at other
    rates are resampled once on import, so the DSP only deals with one rate. */
inline constexpr double analysisSampleRate = 44100.0;

/** A mono audio signal and its sample rate. Treated as immutable once
    imported: edits never touch the source audio. */
struct MonoAudio
{
    std::vector<float> samples;
    double sampleRate = analysisSampleRate;

    std::size_t size() const { return samples.size(); }
    bool empty() const { return samples.empty(); }
    double durationSeconds() const;
};

/** Averages several channels into one. All channels must have the same length. */
MonoAudio mixToMono (std::span<const std::span<const float>> channels, double sampleRate);

/** High-quality offline resampling (windowed-sinc, Kaiser window) with
    anti-aliasing when downsampling. Returns the input unchanged when the rates
    already match. */
MonoAudio resample (const MonoAudio& input, double targetSampleRate);

/** mixToMono + resample to analysisSampleRate: the standard import path. */
MonoAudio prepareForAnalysis (std::span<const std::span<const float>> channels, double sampleRate);
} // namespace melisma::audio
