#include <melisma/core/Pitch.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
namespace pitch = melisma::pitch;

TEST_CASE ("MIDI and Hz conversions", "[core][pitch]")
{
    CHECK (pitch::midiToHz (69.0) == Approx (440.0));
    CHECK (pitch::midiToHz (60.0) == Approx (261.6256).epsilon (1e-6));
    CHECK (pitch::hzToMidi (880.0) == Approx (81.0));
    CHECK (pitch::hzToMidi (pitch::midiToHz (57.37)) == Approx (57.37));
}

TEST_CASE ("Custom reference tuning", "[core][pitch]")
{
    CHECK (pitch::midiToHz (69.0, 432.0) == Approx (432.0));
    CHECK (pitch::hzToMidi (432.0, 432.0) == Approx (69.0));
}

TEST_CASE ("Cents", "[core][pitch]")
{
    CHECK (pitch::centsBetween (440.0, 880.0) == Approx (1200.0));
    CHECK (pitch::centsBetween (880.0, 440.0) == Approx (-1200.0));
    CHECK (pitch::centsToRatio (100.0) == Approx (std::exp2 (1.0 / 12.0)));
    CHECK (pitch::centsBetween (440.0, 440.0 * pitch::centsToRatio (37.0)) == Approx (37.0));
}

TEST_CASE ("Note names", "[core][pitch]")
{
    CHECK (pitch::noteName (69.0) == "A4");
    CHECK (pitch::noteName (60.0) == "C4");
    CHECK (pitch::noteName (61.4) == "C#4");
    CHECK (pitch::noteName (59.6) == "C4");
    CHECK (pitch::noteName (11.0) == "B-1");
    CHECK (pitch::centsFromNearestNote (69.25) == Approx (25.0));
    CHECK (pitch::centsFromNearestNote (68.7) == Approx (-30.0));
}
