#pragma once

#include <cmath>

/** Pitch unit conversions. MIDI note numbers are fractional (60.5 = C4 + 50 cents). */
namespace melisma::pitch
{
inline constexpr double defaultReferenceHz = 440.0; // A4
inline constexpr double referenceMidi = 69.0;       // A4

/** Frequency in Hz -> fractional MIDI note number. hz must be > 0. */
inline double hzToMidi (double hz, double referenceHz = defaultReferenceHz)
{
    return referenceMidi + 12.0 * std::log2 (hz / referenceHz);
}

/** Fractional MIDI note number -> frequency in Hz. */
inline double midiToHz (double midi, double referenceHz = defaultReferenceHz)
{
    return referenceHz * std::exp2 ((midi - referenceMidi) / 12.0);
}

/** Interval from a to b in cents (positive when b is higher). Both must be > 0. */
inline double centsBetween (double hzA, double hzB)
{
    return 1200.0 * std::log2 (hzB / hzA);
}

/** Multiply a frequency by this to shift it by the given number of cents. */
inline double centsToRatio (double cents)
{
    return std::exp2 (cents / 1200.0);
}
} // namespace melisma::pitch
