#pragma once

#include <melisma/core/model/Take.h>

#include <map>
#include <vector>

namespace melisma::ui
{
/** Amplitude envelope of each note (peak per 5 ms block, normalised to the
    loudest block of the take), computed once per take and used to draw the
    blob shapes. */
class NoteEnvelopes
{
public:
    static constexpr double blockSeconds = 0.005;

    void build (const model::Take& take);
    void clear() { envelopes.clear(); }

    /** Envelope of a note (empty if unknown). */
    const std::vector<float>& of (model::NoteId id) const;

private:
    std::map<model::NoteId, std::vector<float>> envelopes;
};
} // namespace melisma::ui
