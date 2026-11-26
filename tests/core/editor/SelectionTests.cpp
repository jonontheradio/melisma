#include <melisma/core/editor/Selection.h>
#include <melisma/core/editor/Snapping.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using melisma::editor::PitchSnap;
using melisma::editor::Selection;
using melisma::model::Note;
using melisma::model::Take;

namespace
{
Take makeTake()
{
    Take take;
    std::vector<Note> notes;
    for (int i = 0; i < 4; ++i)
    {
        Note n;
        n.startSeconds = i;
        n.endSeconds = i + 0.5;
        n.detectedMidi = 60.0 + 2.0 * i; // 60, 62, 64, 66
        notes.push_back (n);
    }
    take.setNotes (notes);
    return take;
}
} // namespace

TEST_CASE ("Click, shift-click and select all", "[core][editor][selection]")
{
    const auto take = makeTake();
    const auto& n = take.notes();
    Selection s;

    s.selectOnly (n[0].id);
    CHECK (s.size() == 1);
    s.toggle (n[2].id);
    CHECK (s.size() == 2);
    CHECK (s.contains (n[2].id));
    s.toggle (n[2].id);
    CHECK_FALSE (s.contains (n[2].id));
    s.selectOnly (n[3].id);
    CHECK (s.toVector() == std::vector{n[3].id});

    s.selectAll (take);
    CHECK (s.size() == 4);
    s.clear();
    CHECK (s.empty());
}

TEST_CASE ("Rubber-band selection uses time and pitch", "[core][editor][selection]")
{
    const auto take = makeTake();
    const auto& n = take.notes();
    Selection s;

    s.selectInRegion (take, 0.9, 2.1, 61.0, 70.0, false); // notes 1 and 2
    CHECK (s.size() == 2);
    CHECK (s.contains (n[1].id));
    CHECK (s.contains (n[2].id));

    s.selectInRegion (take, 3.2, 2.9, 70.0, 65.0, true); // reversed corners, additive: note 3
    CHECK (s.size() == 3);

    s.selectInRegion (take, 0.0, 4.0, 59.0, 60.5, false); // only note 0 by pitch
    CHECK (s.toVector() == std::vector{n[0].id});
}

TEST_CASE ("Selection forgets deleted notes", "[core][editor][selection]")
{
    auto take = makeTake();
    Selection s;
    s.selectAll (take);
    take.removeNote (take.notes()[1].id);
    s.prune (take);
    CHECK (s.size() == 3);
}

TEST_CASE ("Pitch snapping", "[core][editor][snap]")
{
    using melisma::editor::snappedPitchOffsetCents;
    CHECK (snappedPitchOffsetCents (60.2, 62.4, PitchSnap::free) == Approx (220.0));
    CHECK (snappedPitchOffsetCents (60.2, 62.4, PitchSnap::semitone) == Approx (180.0)); // lands on 62
    CHECK (snappedPitchOffsetCents (59.7, 59.6, PitchSnap::semitone) == Approx (30.0));  // corrects to 60
}
