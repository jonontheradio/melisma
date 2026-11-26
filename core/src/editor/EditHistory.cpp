#include "melisma/core/editor/EditHistory.h"

#include <algorithm>
#include <utility>

namespace melisma::editor
{
namespace
{
void applyEdits (model::Take& take, const EditCommand& command, bool useAfter)
{
    for (const auto& change : command.changes)
    {
        if (auto* note = take.findNote (change.id))
        {
            note->edits = useAfter ? change.after : change.before;
        }
    }
}
} // namespace

bool EditCommand::empty() const
{
    return std::ranges::all_of (changes, [] (const NoteChange& c) { return c.before == c.after; });
}

EditSnapshot::EditSnapshot (const model::Take& take, const std::vector<model::NoteId>& ids)
{
    for (const auto id : ids)
    {
        if (const auto* note = take.findNote (id))
        {
            saved.emplace_back (id, note->edits);
        }
    }
}

EditCommand EditSnapshot::toCommand (std::string name, const model::Take& take) const
{
    EditCommand command{std::move (name), {}};
    for (const auto& [id, before] : saved)
    {
        if (const auto* note = take.findNote (id); note != nullptr && note->edits != before)
        {
            command.changes.push_back ({id, before, note->edits});
        }
    }
    return command;
}

void EditSnapshot::restore (model::Take& take) const
{
    for (const auto& [id, before] : saved)
    {
        if (auto* note = take.findNote (id))
        {
            note->edits = before;
        }
    }
}

const model::NoteEdits* EditSnapshot::find (model::NoteId id) const
{
    const auto it = std::ranges::find (saved, id, &std::pair<model::NoteId, model::NoteEdits>::first);
    return it != saved.end() ? &it->second : nullptr;
}

std::vector<model::NoteId> EditSnapshot::ids() const
{
    std::vector<model::NoteId> out;
    out.reserve (saved.size());
    for (const auto& entry : saved)
    {
        out.push_back (entry.first);
    }
    return out;
}

void EditHistory::perform (model::Take& take, EditCommand command)
{
    applyEdits (take, command, true);
    push (std::move (command));
}

void EditHistory::push (EditCommand command)
{
    if (command.empty())
    {
        return;
    }
    commands.resize (position); // drop the redo branch
    commands.push_back (std::move (command));
    if (commands.size() > limit)
    {
        commands.erase (commands.begin());
    }
    position = commands.size();
}

std::string EditHistory::undoName() const
{
    return canUndo() ? commands[position - 1].name : std::string{};
}

std::string EditHistory::redoName() const
{
    return canRedo() ? commands[position].name : std::string{};
}

bool EditHistory::undo (model::Take& take)
{
    if (!canUndo())
    {
        return false;
    }
    --position;
    applyEdits (take, commands[position], false);
    return true;
}

bool EditHistory::redo (model::Take& take)
{
    if (!canRedo())
    {
        return false;
    }
    applyEdits (take, commands[position], true);
    ++position;
    return true;
}

void EditHistory::clear()
{
    commands.clear();
    position = 0;
}
} // namespace melisma::editor
