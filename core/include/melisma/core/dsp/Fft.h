#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace melisma::dsp
{
/** In-place radix-2 complex FFT. Construct once per size and reuse: twiddle
    factors and the bit-reversal table are precomputed. Not thread-safe per
    instance; use one instance per thread. */
class Fft
{
public:
    /** size must be a power of two (>= 2). */
    explicit Fft (std::size_t size);

    std::size_t size() const { return n; }

    /** Forward transform (no scaling). data.size() must equal size(). */
    void forward (std::vector<std::complex<double>>& data) const;

    /** Inverse transform, scaled by 1/N so inverse(forward(x)) == x. */
    void inverse (std::vector<std::complex<double>>& data) const;

    /** Smallest power of two >= value (and >= 2). */
    static std::size_t nextPowerOfTwo (std::size_t value);

private:
    void transform (std::vector<std::complex<double>>& data, bool inverse) const;

    std::size_t n;
    std::vector<std::size_t> bitReversed;
    std::vector<std::complex<double>> twiddles; // e^{-2 pi i k / n}, k < n/2
};
} // namespace melisma::dsp
