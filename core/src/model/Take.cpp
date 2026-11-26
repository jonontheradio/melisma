#include "melisma/core/model/Take.h"

#include <algorithm>
#include <utility>

namespace melisma::model
{
bool NoteEdits::isNeutral() const
{
    return *this == NoteEdits{};
}

Take::Take (audio::MonoAudio source, analysis::PitchTrack pitch)
    : sourceAudio (std::move (source))
    , pitchTrack (std::move (pitch))
{
}

void Take::setNotes (std::vector<Note> newNotes)
{
    noteList = std::move (newNotes);
    for (auto& note : noteList)
    {
        note.id = nextId++;
    }
    sortNotes();
}

NoteId Take::addNote (Note note)
{
    note.id = nextId++;
    const NoteId id = note.id;
    noteList.push_back (note);
    sortNotes();
    return id;
}

bool Take::removeNote (NoteId id)
{
    return std::erase_if (noteList, [id] (const Note& n) { return n.id == id; }) > 0;
}

const Note* Take::findNote (NoteId id) const
{
    const auto it = std::ranges::find (noteList, id, &Note::id);
    return it != noteList.end() ? &*it : nullptr;
}

Note* Take::findNote (NoteId id)
{
    const auto it = std::ranges::find (noteList, id, &Note::id);
    return it != noteList.end() ? &*it : nullptr;
}

const Note* Take::noteAt (double seconds) const
{
    // Last note starting at or before `seconds` (notes are sorted and don't overlap).
    const auto it = std::ranges::upper_bound (noteList, seconds, {}, &Note::startSeconds);
    if (it == noteList.begin())
    {
        return nullptr;
    }
    const auto& candidate = *std::prev (it);
    return candidate.contains (seconds) ? &candidate : nullptr;
}

std::vector<const Note*> Take::notesInRange (double startSeconds, double endSeconds) const
{
    std::vector<const Note*> result;
    for (const auto& note : noteList)
    {
        if (note.endSeconds > startSeconds && note.startSeconds < endSeconds)
        {
            result.push_back (&note);
        }
    }
    return result;
}

bool Take::isUnedited() const
{
    return std::ranges::all_of (noteList, [] (const Note& n) { return n.edits.isNeutral(); });
}

void Take::sortNotes()
{
    std::ranges::stable_sort (noteList, {}, &Note::startSeconds);
}
} // namespace melisma::model
