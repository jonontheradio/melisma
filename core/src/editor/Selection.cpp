#include "melisma/core/editor/Selection.h"

#include <algorithm>

namespace melisma::editor
{
void Selection::selectOnly (model::NoteId id)
{
    ids = {id};
}

void Selection::toggle (model::NoteId id)
{
    if (ids.erase (id) == 0)
    {
        ids.insert (id);
    }
}

void Selection::selectAll (const model::Take& take)
{
    ids.clear();
    for (const auto& n : take.notes())
    {
        ids.insert (n.id);
    }
}

void Selection::selectInRegion (const model::Take& take, double startSeconds, double endSeconds, double lowMidi,
                                double highMidi, bool additive)
{
    if (!additive)
    {
        ids.clear();
    }
    const double t0 = std::min (startSeconds, endSeconds);
    const double t1 = std::max (startSeconds, endSeconds);
    const double m0 = std::min (lowMidi, highMidi);
    const double m1 = std::max (lowMidi, highMidi);
    for (const auto& n : take.notes())
    {
        const double pitch = n.targetMidi();
        if (n.editedEndSeconds() > t0 && n.editedStartSeconds() < t1 && pitch >= m0 && pitch <= m1)
        {
            ids.insert (n.id);
        }
    }
}

void Selection::prune (const model::Take& take)
{
    std::erase_if (ids, [&take] (model::NoteId id) { return take.findNote (id) == nullptr; });
}
} // namespace melisma::editor
