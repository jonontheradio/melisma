#include <melisma/core/editor/EditHistory.h>

#include <catch2/catch_test_macros.hpp>

using melisma::editor::EditHistory;
using melisma::editor::EditSnapshot;
using melisma::model::Note;
using melisma::model::Take;

namespace
{
Take makeTake()
{
    Take take;
    std::vector<Note> notes (3);
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        notes[i].startSeconds = static_cast<double> (i);
        notes[i].endSeconds = static_cast<double> (i) + 0.5;
        notes[i].detectedMidi = 60.0;
    }
    take.setNotes (notes);
    return take;
}

double offset (const Take& t, std::size_t i)
{
    return t.notes()[i].edits.pitchOffsetCents;
}
} // namespace

TEST_CASE ("Perform, undo and redo", "[core][editor][history]")
{
    auto take = makeTake();
    EditHistory history;
    const auto id = take.notes()[0].id;

    auto after = take.notes()[0].edits;
    after.pitchOffsetCents = 100.0;
    history.perform (take, {"Move pitch", {{id, take.notes()[0].edits, after}}});

    CHECK (offset (take, 0) == 100.0);
    CHECK (history.canUndo());
    CHECK (history.undoName() == "Move pitch");

    CHECK (history.undo (take));
    CHECK (offset (take, 0) == 0.0);
    CHECK_FALSE (history.canUndo());
    CHECK (history.redoName() == "Move pitch");

    CHECK (history.redo (take));
    CHECK (offset (take, 0) == 100.0);
    CHECK_FALSE (history.redo (take));
}

TEST_CASE ("A whole drag is one undo step", "[core][editor][history]")
{
    auto take = makeTake();
    EditHistory history;
    const std::vector ids{take.notes()[0].id, take.notes()[2].id};

    const EditSnapshot snapshot (take, ids);
    for (int step = 1; step <= 10; ++step) // mouse moves
    {
        take.findNote (ids[0])->edits.pitchOffsetCents = step * 10.0;
        take.findNote (ids[1])->edits.pitchOffsetCents = step * 10.0;
    }
    const auto command = snapshot.toCommand ("Move pitch", take);
    CHECK (command.changes.size() == 2);
    history.push (command);

    CHECK (history.undo (take));
    CHECK (offset (take, 0) == 0.0);
    CHECK (offset (take, 2) == 0.0);
    CHECK_FALSE (history.canUndo());
}

TEST_CASE ("Unchanged drags and the redo branch", "[core][editor][history]")
{
    auto take = makeTake();
    EditHistory history;
    const auto id = take.notes()[1].id;

    // A click without movement records nothing.
    const EditSnapshot noop (take, {id});
    history.push (noop.toCommand ("Move pitch", take));
    CHECK_FALSE (history.canUndo());

    // A new command after an undo drops the redo branch.
    auto a = take.notes()[1].edits;
    a.gainDb = -6.0;
    history.perform (take, {"Gain", {{id, {}, a}}});
    history.undo (take);
    auto b = take.notes()[1].edits;
    b.muted = true;
    history.perform (take, {"Mute", {{id, {}, b}}});
    CHECK_FALSE (history.canRedo());
    CHECK (history.undoName() == "Mute");
}

TEST_CASE ("Escape restores a drag", "[core][editor][history]")
{
    auto take = makeTake();
    const auto id = take.notes()[0].id;
    const EditSnapshot snapshot (take, {id});
    take.findNote (id)->edits.timeStretch = 3.0;
    snapshot.restore (take);
    CHECK (take.notes()[0].edits.isNeutral());
}

TEST_CASE ("History keeps a bounded number of steps", "[core][editor][history]")
{
    auto take = makeTake();
    EditHistory history (3);
    const auto id = take.notes()[0].id;
    for (int i = 1; i <= 5; ++i)
    {
        auto e = take.notes()[0].edits;
        e.pitchOffsetCents = i;
        history.perform (take, {"Step", {{id, take.notes()[0].edits, e}}});
    }
    int undos = 0;
    while (history.undo (take))
    {
        ++undos;
    }
    CHECK (undos == 3);
    CHECK (offset (take, 0) == 2.0);
}
