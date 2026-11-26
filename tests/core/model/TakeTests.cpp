#include <melisma/core/model/Take.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using melisma::model::Note;
using melisma::model::Take;

namespace
{
Note makeNote (double start, double end, double midi)
{
    Note n;
    n.startSeconds = start;
    n.endSeconds = end;
    n.detectedMidi = midi;
    return n;
}
} // namespace

TEST_CASE ("Note edits are neutral by default", "[core][model]")
{
    const Note n = makeNote (1.0, 1.5, 60.0);
    CHECK (n.edits.isNeutral());
    CHECK (n.durationSeconds() == Approx (0.5));
    CHECK (n.targetMidi() == 60.0);
    CHECK (n.editedStartSeconds() == 1.0);
    CHECK (n.editedEndSeconds() == Approx (1.5));
    CHECK (n.contains (1.0));
    CHECK_FALSE (n.contains (1.5));
}

TEST_CASE ("Note edits change target pitch and timing", "[core][model]")
{
    Note n = makeNote (1.0, 1.5, 60.0);
    n.edits.pitchOffsetCents = 150.0;
    n.edits.timeOffsetSeconds = 0.25;
    n.edits.timeStretch = 2.0;

    CHECK_FALSE (n.edits.isNeutral());
    CHECK (n.targetMidi() == Approx (61.5));
    CHECK (n.editedStartSeconds() == Approx (1.25));
    CHECK (n.editedEndSeconds() == Approx (2.25));
}

TEST_CASE ("Take keeps notes sorted with unique ids", "[core][model]")
{
    Take take;
    take.setNotes ({makeNote (2.0, 2.5, 64.0), makeNote (0.0, 0.5, 60.0), makeNote (1.0, 1.5, 62.0)});

    REQUIRE (take.notes().size() == 3);
    CHECK (take.notes()[0].detectedMidi == 60.0);
    CHECK (take.notes()[1].detectedMidi == 62.0);
    CHECK (take.notes()[2].detectedMidi == 64.0);
    CHECK (take.notes()[0].id != take.notes()[1].id);
    CHECK (take.notes()[1].id != take.notes()[2].id);

    const auto added = take.addNote (makeNote (0.6, 0.9, 61.0));
    CHECK (take.notes()[1].id == added);
    CHECK (take.isUnedited());
}

TEST_CASE ("Take lookups", "[core][model]")
{
    Take take;
    take.setNotes ({makeNote (0.0, 0.5, 60.0), makeNote (1.0, 1.5, 62.0)});

    CHECK (take.noteAt (0.25)->detectedMidi == 60.0);
    CHECK (take.noteAt (1.0)->detectedMidi == 62.0);
    CHECK (take.noteAt (0.75) == nullptr);
    CHECK (take.noteAt (-1.0) == nullptr);
    CHECK (take.noteAt (2.0) == nullptr);

    CHECK (take.notesInRange (0.4, 1.1).size() == 2);
    CHECK (take.notesInRange (0.5, 1.0).empty());

    const auto id = take.notes()[1].id;
    REQUIRE (take.findNote (id) != nullptr);
    take.findNote (id)->edits.gainDb = -3.0;
    CHECK_FALSE (take.isUnedited());

    CHECK (take.removeNote (id));
    CHECK_FALSE (take.removeNote (id));
    CHECK (take.findNote (id) == nullptr);
    CHECK (take.notes().size() == 1);
}
