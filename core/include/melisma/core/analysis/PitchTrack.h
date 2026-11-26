#pragma once

#include <cstddef>
#include <vector>

namespace melisma::analysis
{
/** Pitch estimate for one analysis frame. */
struct PitchFrame
{
    double timeSeconds = 0.0; // centre of the frame
    double f0Hz = 0.0;        // 0 = no pitch found
    double confidence = 0.0;  // 0..1, how periodic the frame is (1 = perfectly periodic)
    double rms = 0.0;         // frame energy (linear)

    bool hasPitch() const { return f0Hz > 0.0; }
};

/** Frame-by-frame pitch analysis of a signal, at a fixed hop. */
struct PitchTrack
{
    double hopSeconds = 0.005;
    std::vector<PitchFrame> frames;

    std::size_t size() const { return frames.size(); }
    bool empty() const { return frames.empty(); }

    /** Index of the frame closest to a time (clamped to the valid range). */
    std::size_t frameIndexAt (double seconds) const;
};
} // namespace melisma::analysis
