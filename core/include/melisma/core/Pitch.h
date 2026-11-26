#pragma once

#include <array>
#include <cmath>
#include <string>

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
/** Name of the nearest equal-tempered note, e.g. 69.2 -> "A4" (middle C = C4). */
inline std::string noteName (double midi)
{
    static constexpr std::array<const char*, 12> names = {"C",  "C#", "D",  "D#", "E",  "F",
                                                          "F#", "G",  "G#", "A",  "A#", "B"};
    const long nearest = std::lround (midi);
    const auto pitchClass = static_cast<std::size_t> (((nearest % 12) + 12) % 12);
    const long octave = static_cast<long> (std::floor (static_cast<double> (nearest) / 12.0)) - 1;
    return std::string (names[pitchClass]) + std::to_string (octave);
}

/** Deviation from the nearest equal-tempered note, in cents (-50..50). */
inline double centsFromNearestNote (double midi)
{
    return 100.0 * (midi - std::round (midi));
}
} // namespace melisma::pitch
