#pragma once

#include "melisma/core/model/Take.h"

#include <cstddef>
#include <string>
#include <vector>

namespace melisma::editor
{
/** One note's edits before and after a command. */
struct NoteChange
{
    model::NoteId id = 0;
    model::NoteEdits before;
    model::NoteEdits after;
};

/** A user action (e.g. "Move pitch" on 3 notes), undoable as one step. */
struct EditCommand
{
    std::string name;
    std::vector<NoteChange> changes;

    bool empty() const;
};

/** Edits of some notes captured before a gesture (e.g. at mouse down), so the
    whole drag becomes a single undo step at mouse up. */
class EditSnapshot
{
public:
    EditSnapshot() = default;
    EditSnapshot (const model::Take& take, const std::vector<model::NoteId>& ids);

    /** Command from the snapshot to the take's current state (only notes that changed). */
    EditCommand toCommand (std::string name, const model::Take& take) const;

    /** Puts the captured edits back (e.g. Escape during a drag). */
    void restore (model::Take& take) const;

private:
    std::vector<std::pair<model::NoteId, model::NoteEdits>> saved;
};

/** Undo/redo stack of edit commands. */
class EditHistory
{
public:
    explicit EditHistory (std::size_t maxSteps = 500)
        : limit (maxSteps)
    {
    }

    /** Applies the command's `after` values and records it. */
    void perform (model::Take& take, EditCommand command);

    /** Records a command whose changes are already applied (end of a drag). */
    void push (EditCommand command);

    bool canUndo() const { return position > 0; }
    bool canRedo() const { return position < commands.size(); }
    std::string undoName() const;
    std::string redoName() const;

    bool undo (model::Take& take);
    bool redo (model::Take& take);

    void clear();

private:
    std::vector<EditCommand> commands;
    std::size_t position = 0; // commands[0, position) are applied
    std::size_t limit;
};
} // namespace melisma::editor
