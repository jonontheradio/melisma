#include "TestSignals.h"

#include <melisma/core/Pitch.h>
#include <melisma/core/analysis/NoteSegmentation.h>
#include <melisma/core/analysis/Yin.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::SegmentationSettings;
using melisma::model::Note;

namespace
{
std::vector<Note> analyse (const ts::Signal& s, const SegmentationSettings& settings = {})
{
    const melisma::audio::MonoAudio audio{s.samples, s.sampleRate};
    const auto track = melisma::analysis::detectPitchYin (audio);
    const auto classes = melisma::analysis::classifyFrames (audio, track);
    return melisma::analysis::segmentNotes (track, classes, settings);
}

/** Legato line padded with silence. */
ts::Signal legato (const std::vector<ts::Note>& notes)
{
    ts::Signal out = ts::silence (0.1);
    out.append (ts::legatoMelody (notes));
    out.append (ts::silence (0.1));
    return out;
}

double cents (double midiA, double midiB)
{
    return 100.0 * std::abs (midiA - midiB);
}
} // namespace

TEST_CASE ("Sensitivity maps to split threshold and hold time", "[core][analysis][segmentation]")
{
    CHECK (SegmentationSettings{.sensitivity = 0.0}.splitSemitones()
           > SegmentationSettings{.sensitivity = 1.0}.splitSemitones());
    CHECK (SegmentationSettings{.sensitivity = 0.0}.holdSeconds()
           > SegmentationSettings{.sensitivity = 1.0}.holdSeconds());
}

TEST_CASE ("A melody with gaps gives one note per sung note", "[core][analysis][segmentation]")
{
    const std::vector<ts::Note> sung = {{55.0, 0.4}, {59.0, 0.3}, {62.0, 0.3}, {67.0, 0.5}, {64.0, 0.4}};
    const auto notes = analyse (ts::melody (sung, true, 0.1));

    REQUIRE (notes.size() == sung.size());
    double start = 0.0;
    for (std::size_t i = 0; i < sung.size(); ++i)
    {
        INFO ("note " << i << ": " << notes[i].startSeconds << "-" << notes[i].endSeconds << " midi "
                      << notes[i].detectedMidi);
        CHECK (cents (notes[i].detectedMidi, sung[i].midi) < 10.0);
        CHECK (std::abs (notes[i].startSeconds - start) < 0.03);
        CHECK (std::abs (notes[i].endSeconds - (start + sung[i].seconds)) < 0.03);
        CHECK (notes[i].id == 0);
        start += sung[i].seconds + 0.1;
    }
}

TEST_CASE ("Legato pitch changes split notes without any gap", "[core][analysis][segmentation]")
{
    const std::vector<ts::Note> sung = {{60.0, 0.35}, {62.0, 0.3}, {64.0, 0.3}, {60.0, 0.4}, {57.0, 0.5}};
    const auto notes = analyse (legato (sung));

    REQUIRE (notes.size() == sung.size());
    double boundary = 0.1;
    for (std::size_t i = 0; i < sung.size(); ++i)
    {
        INFO ("note " << i << ": " << notes[i].startSeconds << "-" << notes[i].endSeconds << " midi "
                      << notes[i].detectedMidi);
        CHECK (cents (notes[i].detectedMidi, sung[i].midi) < 10.0);
        CHECK (std::abs (notes[i].startSeconds - boundary) < 0.04);
        boundary += sung[i].seconds;
    }
}

TEST_CASE ("Legato with vibrato still splits only at the real notes", "[core][analysis][segmentation]")
{
    const std::vector<ts::Note> sung = {{62.0, 0.6}, {64.0, 0.6}, {66.0, 0.7}, {62.0, 0.8}};
    ts::VowelSettings voice;
    voice.vibratoRateHz = 5.8;
    voice.vibratoDepthCents = 45.0;
    voice.jitterCents = 10.0;

    ts::Signal s = ts::silence (0.1);
    s.append (ts::legatoMelody (sung, 0.06, voice));
    s.append (ts::silence (0.1));
    const auto notes = analyse (s);

    REQUIRE (notes.size() == sung.size());
    double boundary = 0.1;
    for (std::size_t i = 0; i < sung.size(); ++i)
    {
        INFO ("note " << i << ": " << notes[i].startSeconds << "-" << notes[i].endSeconds << " midi "
                      << notes[i].detectedMidi);
        CHECK (cents (notes[i].detectedMidi, sung[i].midi) < 15.0);
        CHECK (std::abs (notes[i].startSeconds - boundary) < 0.06);
        boundary += sung[i].seconds;
    }
}

TEST_CASE ("Vibrato and drift stay inside one note", "[core][analysis][segmentation]")
{
    ts::VowelSettings v;
    v.f0Hz = melisma::pitch::midiToHz (65.0);
    v.vibratoRateHz = 5.5;
    v.vibratoDepthCents = 60.0;
    v.jitterCents = 20.0;
    auto s = ts::vowel (v, 1.5);
    ts::applyFades (s, 0.02);

    const auto notes = analyse (s);
    REQUIRE (notes.size() == 1);
    CHECK (cents (notes[0].detectedMidi, 65.0) < 15.0);
    CHECK (notes[0].durationSeconds() > 1.4);
}

TEST_CASE ("Sensitivity decides whether small steps are new notes", "[core][analysis][segmentation]")
{
    // Steps of 60 cents: below the low-sensitivity threshold, above the high one.
    const std::vector<ts::Note> sung = {{60.0, 0.4}, {60.6, 0.4}, {61.2, 0.4}};
    const auto s = legato (sung);

    CHECK (analyse (s, {.sensitivity = 0.0}).size() == 1);
    CHECK (analyse (s, {.sensitivity = 1.0}).size() == 3);
}

TEST_CASE ("Silence and noise give no notes", "[core][analysis][segmentation]")
{
    CHECK (analyse (ts::silence (1.0)).empty());
    CHECK (analyse (ts::noise (1.0, 44100.0, 0.2f)).empty());
}

TEST_CASE ("Very short voiced blips are ignored", "[core][analysis][segmentation]")
{
    const ts::VowelSettings v;
    auto s = ts::silence (0.2);
    const auto blip = ts::vowel (v, 0.035);
    s.append (blip);
    s.append (ts::silence (0.2));
    CHECK (analyse (s).empty());
}
