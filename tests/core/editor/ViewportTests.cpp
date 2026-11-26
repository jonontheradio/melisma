#include <melisma/core/editor/Viewport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using melisma::editor::Viewport;

namespace
{
Viewport make()
{
    Viewport v;
    v.setSize (1000.0, 480.0);
    v.setContentDuration (20.0);
    v.setVisibleRange (2.0, 12.0, 48.0, 72.0); // 10 s, 24 semitones
    return v;
}
} // namespace

TEST_CASE ("Viewport maps time and pitch to pixels and back", "[core][editor][viewport]")
{
    const auto v = make();
    CHECK (v.timeToX (2.0) == Approx (0.0));
    CHECK (v.timeToX (7.0) == Approx (500.0));
    CHECK (v.xToTime (250.0) == Approx (4.5));
    CHECK (v.midiToY (72.0) == Approx (0.0));   // top
    CHECK (v.midiToY (48.0) == Approx (480.0)); // bottom
    CHECK (v.yToMidi (240.0) == Approx (60.0));
    CHECK (v.pixelsPerSecond() == Approx (100.0));
    CHECK (v.pixelsPerSemitone() == Approx (20.0));
}

TEST_CASE ("Zooming keeps the anchor point fixed", "[core][editor][viewport]")
{
    auto v = make();
    const double timeAtAnchor = v.xToTime (300.0);
    v.zoomTime (2.0, 300.0);
    CHECK (v.visibleSeconds() == Approx (5.0));
    CHECK (v.xToTime (300.0) == Approx (timeAtAnchor));

    const double midiAtAnchor = v.yToMidi (100.0);
    v.zoomPitch (2.0, 100.0);
    CHECK (v.visibleSemitones() == Approx (12.0));
    CHECK (v.yToMidi (100.0) == Approx (midiAtAnchor));
}

TEST_CASE ("Zoom and scroll stay inside the content", "[core][editor][viewport]")
{
    auto v = make();
    v.zoomTime (0.01, 0.0); // zoom far out
    CHECK (v.visibleSeconds() == Approx (20.0));
    CHECK (v.startSeconds() == Approx (0.0));

    v.zoomTime (1.0e6, 500.0); // zoom far in
    CHECK (v.visibleSeconds() == Approx (Viewport::minVisibleSeconds));

    v.scrollTime (-1.0e9);
    CHECK (v.startSeconds() == Approx (0.0));
    v.scrollTime (1.0e9);
    CHECK (v.endSeconds() == Approx (20.0));

    v.scrollPitch (1.0e9);
    CHECK (v.highMidi() == Approx (Viewport::highestMidi));
    v.scrollPitch (-1.0e9);
    CHECK (v.lowMidi() == Approx (Viewport::lowestMidi));

    v.zoomPitch (1.0e6, 0.0);
    CHECK (v.visibleSemitones() == Approx (Viewport::minVisibleSemitones));
}

TEST_CASE ("Scrolling moves by pixels", "[core][editor][viewport]")
{
    auto v = make();
    v.scrollTime (100.0); // 1 s at 100 px/s
    CHECK (v.startSeconds() == Approx (3.0));
    v.scrollPitch (40.0); // 2 semitones at 20 px/st
    CHECK (v.lowMidi() == Approx (50.0));
}
