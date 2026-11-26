#include "NoteEnvelopes.h"

#include <algorithm>
#include <cmath>

namespace melisma::ui
{
void NoteEnvelopes::build (const model::Take& take)
{
    envelopes.clear();
    const auto& audio = take.source();
    if (audio.empty())
    {
        return;
    }

    const auto block = std::max<std::size_t> (1, static_cast<std::size_t> (blockSeconds * audio.sampleRate));
    float loudest = 1.0e-6f;
    for (const auto& note : take.notes())
    {
        const auto first = static_cast<std::size_t> (std::max (0.0, note.startSeconds * audio.sampleRate));
        const auto last = std::min (audio.size(), static_cast<std::size_t> (note.endSeconds * audio.sampleRate));
        std::vector<float> env;
        for (std::size_t b = first; b < last; b += block)
        {
            float peak = 0.0f;
            for (std::size_t i = b; i < std::min (last, b + block); ++i)
            {
                peak = std::max (peak, std::abs (audio.samples[i]));
            }
            env.push_back (peak);
            loudest = std::max (loudest, peak);
        }
        envelopes[note.id] = std::move (env);
    }

    for (auto& [id, env] : envelopes)
    {
        for (auto& v : env)
        {
            v = std::sqrt (v / loudest); // sqrt: quiet parts stay visible
        }
    }
}

const std::vector<float>& NoteEnvelopes::of (model::NoteId id) const
{
    static const std::vector<float> none;
    const auto it = envelopes.find (id);
    return it != envelopes.end() ? it->second : none;
}
} // namespace melisma::ui
