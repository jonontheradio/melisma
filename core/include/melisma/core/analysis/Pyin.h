#pragma once

#include "melisma/core/analysis/Yin.h"

namespace melisma::analysis
{
/** Probabilistic YIN (Mauch & Dixon, 2014).

    Instead of one threshold, every frame is analysed with 100 thresholds
    weighted by a Beta prior, giving several pitch candidates with
    probabilities. A hidden Markov model over (pitch bin x voiced/unvoiced)
    then picks the most plausible path through time with Viterbi, which
    removes most octave jumps and isolated errors and decides voicing.

    Uses minHz, maxHz, hopSeconds and silenceRms from `settings` (threshold
    is not used). confidence = 1 - CMNDF at the chosen lag, comparable to YIN. */
PitchTrack detectPitchPyin (const audio::MonoAudio& audio, const YinSettings& settings = {},
                            const ProgressCallback& progress = {});
} // namespace melisma::analysis
