#include "melisma/core/audio/MonoAudio.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace melisma::audio
{
namespace
{
/** Zeroth-order modified Bessel function of the first kind (series expansion). */
double besselI0 (double x)
{
    double sum = 1.0;
    double term = 1.0;
    const double halfX = 0.5 * x;
    for (int k = 1; k < 50; ++k)
    {
        term *= (halfX / k) * (halfX / k);
        sum += term;
        if (term < sum * 1e-12)
        {
            break;
        }
    }
    return sum;
}

double sinc (double x)
{
    if (std::abs (x) < 1e-12)
    {
        return 1.0;
    }
    const double px = std::numbers::pi * x;
    return std::sin (px) / px;
}

/** Interpolation kernel half-width in input samples at cutoff 1 (no downsampling). */
constexpr int kernelHalfWidth = 32;
/** Kaiser beta: ~ -90 dB sidelobes. */
constexpr double kaiserBeta = 9.0;
/** Kernel table entries per input sample. */
constexpr double kernelTableResolution = 512.0;
/** Cutoff as a fraction of the lower Nyquist frequency, leaving a transition band. */
constexpr double cutoffFraction = 0.94;
} // namespace

double MonoAudio::durationSeconds() const
{
    return sampleRate > 0.0 ? static_cast<double> (samples.size()) / sampleRate : 0.0;
}

MonoAudio mixToMono (std::span<const std::span<const float>> channels, double sampleRate)
{
    MonoAudio out;
    out.sampleRate = sampleRate;
    if (channels.empty())
    {
        return out;
    }

    const auto length = channels.front().size();
    out.samples.assign (length, 0.0f);
    const float gain = 1.0f / static_cast<float> (channels.size());

    for (const auto& channel : channels)
    {
        assert (channel.size() == length);
        const auto n = std::min (length, channel.size());
        for (std::size_t i = 0; i < n; ++i)
        {
            out.samples[i] += channel[i] * gain;
        }
    }
    return out;
}

MonoAudio resample (const MonoAudio& input, double targetSampleRate)
{
    if (input.sampleRate == targetSampleRate || input.empty() || input.sampleRate <= 0.0 || targetSampleRate <= 0.0)
    {
        MonoAudio copy = input;
        copy.sampleRate = targetSampleRate > 0.0 ? targetSampleRate : input.sampleRate;
        return copy;
    }

    const double ratio = targetSampleRate / input.sampleRate; // output samples per input sample
    const double scale = std::min (1.0, ratio);               // < 1 when downsampling
    const double cutoff = cutoffFraction * scale;             // relative to the input Nyquist
    const double halfWidth = std::ceil (kernelHalfWidth / scale);

    // Kernel (sinc * Kaiser window) tabulated over [0, halfWidth] input samples,
    // then linearly interpolated: evaluating Bessel functions per tap is too slow.
    const auto tableSize = static_cast<std::size_t> (halfWidth * kernelTableResolution) + 2;
    std::vector<double> kernel (tableSize);
    const double invI0Beta = 1.0 / besselI0 (kaiserBeta);
    for (std::size_t t = 0; t < tableSize; ++t)
    {
        const double distance = static_cast<double> (t) / kernelTableResolution;
        const double w = std::min (1.0, distance / halfWidth);
        const double window = besselI0 (kaiserBeta * std::sqrt (1.0 - w * w)) * invI0Beta;
        kernel[t] = cutoff * sinc (cutoff * distance) * window;
    }

    const auto kernelAt = [&kernel] (double distance)
    {
        const double pos = std::abs (distance) * kernelTableResolution;
        const auto index = static_cast<std::size_t> (pos);
        if (index + 1 >= kernel.size())
        {
            return 0.0;
        }
        const double frac = pos - static_cast<double> (index);
        return kernel[index] + frac * (kernel[index + 1] - kernel[index]);
    };

    const auto inLength = static_cast<std::ptrdiff_t> (input.samples.size());
    const auto outLength = static_cast<std::size_t> (std::floor (static_cast<double> (inLength) * ratio));

    MonoAudio out;
    out.sampleRate = targetSampleRate;
    out.samples.resize (outLength);

    for (std::size_t i = 0; i < outLength; ++i)
    {
        const double centre = static_cast<double> (i) / ratio; // position in input samples
        const auto first = std::max<std::ptrdiff_t> (static_cast<std::ptrdiff_t> (std::ceil (centre - halfWidth)), 0);
        const auto last = std::min (static_cast<std::ptrdiff_t> (std::floor (centre + halfWidth)), inLength - 1);

        double acc = 0.0;
        for (auto k = first; k <= last; ++k)
        {
            acc += static_cast<double> (input.samples[static_cast<std::size_t> (k)])
                   * kernelAt (centre - static_cast<double> (k));
        }
        out.samples[i] = static_cast<float> (acc);
    }
    return out;
}

MonoAudio prepareForAnalysis (std::span<const std::span<const float>> channels, double sampleRate)
{
    return resample (mixToMono (channels, sampleRate), analysisSampleRate);
}
} // namespace melisma::audio
