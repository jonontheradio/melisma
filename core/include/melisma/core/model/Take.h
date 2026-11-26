#pragma once

#include "melisma/core/analysis/PitchTrack.h"
#include "melisma/core/audio/MonoAudio.h"
#include "melisma/core/model/Note.h"

#include <vector>

namespace melisma::model
{
/** One analysed recording: the source audio (never modified), its pitch
    analysis and the list of notes with their edits. */
class Take
{
public:
    Take() = default;
    Take (audio::MonoAudio source, analysis::PitchTrack pitch);

    const audio::MonoAudio& source() const { return sourceAudio; }
    const analysis::PitchTrack& pitch() const { return pitchTrack; }

    /** Notes sorted by start time. */
    const std::vector<Note>& notes() const { return noteList; }

    /** Replaces all notes (e.g. after segmentation). Assigns fresh ids and sorts. */
    void setNotes (std::vector<Note> newNotes);

    /** Adds a note, assigns it a fresh id and returns that id. */
    NoteId addNote (Note note);

    /** Removes a note. Returns false if no note had this id. */
    bool removeNote (NoteId id);

    const Note* findNote (NoteId id) const;
    Note* findNote (NoteId id);

    /** Note under a time in the source (nullptr if none). */
    const Note* noteAt (double seconds) const;

    /** Notes overlapping [startSeconds, endSeconds). */
    std::vector<const Note*> notesInRange (double startSeconds, double endSeconds) const;

    /** True when no note has edits, i.e. the output equals the source. */
    bool isUnedited() const;

private:
    void sortNotes();

    audio::MonoAudio sourceAudio;
    analysis::PitchTrack pitchTrack;
    std::vector<Note> noteList;
    NoteId nextId = 1;
};
} // namespace melisma::model
