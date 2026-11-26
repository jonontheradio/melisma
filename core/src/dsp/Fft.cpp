#include "melisma/core/dsp/Fft.h"

#include <cassert>
#include <cmath>
#include <numbers>
#include <utility>

namespace melisma::dsp
{
Fft::Fft (std::size_t size)
    : n (size)
{
    assert (n >= 2 && (n & (n - 1)) == 0);

    std::size_t bits = 0;
    while ((std::size_t{1} << bits) < n)
    {
        ++bits;
    }

    bitReversed.resize (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        std::size_t r = 0;
        for (std::size_t b = 0; b < bits; ++b)
        {
            r |= ((i >> b) & 1U) << (bits - 1 - b);
        }
        bitReversed[i] = r;
    }

    twiddles.resize (n / 2);
    for (std::size_t k = 0; k < n / 2; ++k)
    {
        const double angle = -2.0 * std::numbers::pi * static_cast<double> (k) / static_cast<double> (n);
        twiddles[k] = {std::cos (angle), std::sin (angle)};
    }
}

std::size_t Fft::nextPowerOfTwo (std::size_t value)
{
    std::size_t p = 2;
    while (p < value)
    {
        p <<= 1U;
    }
    return p;
}

void Fft::forward (std::vector<std::complex<double>>& data) const
{
    transform (data, false);
}

void Fft::inverse (std::vector<std::complex<double>>& data) const
{
    transform (data, true);
    const double scale = 1.0 / static_cast<double> (n);
    for (auto& x : data)
    {
        x *= scale;
    }
}

void Fft::transform (std::vector<std::complex<double>>& data, bool inverse) const
{
    assert (data.size() == n);

    for (std::size_t i = 0; i < n; ++i)
    {
        if (i < bitReversed[i])
        {
            std::swap (data[i], data[bitReversed[i]]);
        }
    }

    for (std::size_t len = 2; len <= n; len <<= 1U)
    {
        const std::size_t half = len / 2;
        const std::size_t step = n / len;
        for (std::size_t start = 0; start < n; start += len)
        {
            for (std::size_t k = 0; k < half; ++k)
            {
                const auto w = inverse ? std::conj (twiddles[k * step]) : twiddles[k * step];
                const auto t = w * data[start + k + half];
                data[start + k + half] = data[start + k] - t;
                data[start + k] += t;
            }
        }
    }
}
} // namespace melisma::dsp
