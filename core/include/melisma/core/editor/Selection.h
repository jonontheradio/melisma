#pragma once

#include "melisma/core/model/Take.h"

#include <set>
#include <vector>

namespace melisma::editor
{
/** Set of selected notes, by id (ids survive edits and undo). */
class Selection
{
public:
    bool contains (model::NoteId id) const { return ids.contains (id); }
    bool empty() const { return ids.empty(); }
    std::size_t size() const { return ids.size(); }
    const std::set<model::NoteId>& noteIds() const { return ids; }
    std::vector<model::NoteId> toVector() const { return {ids.begin(), ids.end()}; }

    void clear() { ids.clear(); }

    /** Click: select only this note. */
    void selectOnly (model::NoteId id);
    /** Shift-click: add or remove this note. */
    void toggle (model::NoteId id);
    void add (model::NoteId id) { ids.insert (id); }

    void selectAll (const model::Take& take);

    /** Rubber band: notes overlapping the time range whose pitch lies in the
        MIDI range. With `additive`, keeps the current selection. */
    void selectInRegion (const model::Take& take, double startSeconds, double endSeconds, double lowMidi,
                         double highMidi, bool additive);

    /** Drops ids of notes that no longer exist in the take. */
    void prune (const model::Take& take);

private:
    std::set<model::NoteId> ids;
};
} // namespace melisma::editor
