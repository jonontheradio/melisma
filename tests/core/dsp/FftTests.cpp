#include <melisma/core/dsp/Fft.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using Catch::Approx;
using melisma::dsp::Fft;

TEST_CASE ("FFT of a cosine has energy only in its bin", "[core][dsp][fft]")
{
    const std::size_t n = 64;
    const std::size_t bin = 5;
    std::vector<std::complex<double>> x (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        x[i] = std::cos (2.0 * std::numbers::pi * static_cast<double> (bin * i) / static_cast<double> (n));
    }

    Fft (n).forward (x);

    for (std::size_t k = 0; k < n; ++k)
    {
        const double expected = (k == bin || k == n - bin) ? static_cast<double> (n) / 2.0 : 0.0;
        CHECK (std::abs (x[k]) == Approx (expected).margin (1e-9));
    }
}

TEST_CASE ("Inverse FFT restores the input", "[core][dsp][fft]")
{
    const std::size_t n = 1024;
    std::vector<std::complex<double>> x (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        x[i] = {std::sin (0.37 * static_cast<double> (i)), std::cos (1.3 * static_cast<double> (i * i % 17))};
    }
    const auto original = x;

    const Fft fft (n);
    fft.forward (x);
    fft.inverse (x);

    for (std::size_t i = 0; i < n; ++i)
    {
        CHECK (x[i].real() == Approx (original[i].real()).margin (1e-12));
        CHECK (x[i].imag() == Approx (original[i].imag()).margin (1e-12));
    }
}

TEST_CASE ("nextPowerOfTwo", "[core][dsp][fft]")
{
    CHECK (Fft::nextPowerOfTwo (0) == 2);
    CHECK (Fft::nextPowerOfTwo (2) == 2);
    CHECK (Fft::nextPowerOfTwo (3) == 4);
    CHECK (Fft::nextPowerOfTwo (1000) == 1024);
    CHECK (Fft::nextPowerOfTwo (1024) == 1024);
}
