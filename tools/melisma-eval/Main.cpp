// melisma-eval: analyse an audio file and dump the results.
//
//   melisma-eval voice.wav                 -> prints the notes, writes voice.f0.csv and voice.notes.csv
//   melisma-eval voice.wav --out results/  -> CSVs go to results/
//   options: --algorithm pyin|yin  --sensitivity 0..1  --min-hz N  --max-hz N
#include "AudioFileLoader.h"

#include <melisma/core/Pitch.h>
#include <melisma/core/analysis/Analysis.h>

#include <juce_core/juce_core.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <vector>

namespace
{
using melisma::analysis::FrameClass;

const char* className (FrameClass c)
{
    switch (c)
    {
        case FrameClass::silence:
            return "silence";
        case FrameClass::unvoiced:
            return "unvoiced";
        case FrameClass::voiced:
            return "voiced";
    }
    return "?";
}

int usage()
{
    std::cerr << "usage: melisma-eval <audio file> [--out DIR] [--algorithm pyin|yin] [--sensitivity 0..1] [--min-hz "
                 "N] [--max-hz N]\n";
    return 2;
}
} // namespace

static int run (const std::vector<juce::String>& args)
{
    if (args.size() < 2)
    {
        return usage();
    }

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File input = cwd.getChildFile (args[1]);
    juce::File outDir = input.getParentDirectory();
    melisma::analysis::AnalysisSettings settings;

    for (std::size_t i = 2; i < args.size(); i += 2)
    {
        if (i + 1 >= args.size())
        {
            return usage();
        }
        const auto& arg = args[i];
        const auto& value = args[i + 1];
        if (arg == "--out")
        {
            outDir = cwd.getChildFile (value);
        }
        else if (arg == "--sensitivity")
        {
            settings.segmentation.sensitivity = value.getDoubleValue();
        }
        else if (arg == "--algorithm")
        {
            if (value != "yin" && value != "pyin")
            {
                return usage();
            }
            settings.algorithm =
                value == "yin" ? melisma::analysis::PitchAlgorithm::yin : melisma::analysis::PitchAlgorithm::pyin;
        }
        else if (arg == "--min-hz")
        {
            settings.pitch.minHz = value.getDoubleValue();
        }
        else if (arg == "--max-hz")
        {
            settings.pitch.maxHz = value.getDoubleValue();
        }
        else
        {
            return usage();
        }
    }

    juce::String error;
    auto loaded = melisma::adapters::loadAudioFile (input, error);
    if (!loaded)
    {
        std::cerr << error << '\n';
        return 1;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const auto take = melisma::analysis::analyseTake (std::move (loaded->audio), settings);
    const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    if (!take)
    {
        std::cerr << "Analysis cancelled\n";
        return 1;
    }

    // Frame classes are recomputed here only to write them to the CSV
    // (same rule as analyseTake: pYIN decides voicing itself).
    auto voicing = settings.voicing;
    if (settings.algorithm == melisma::analysis::PitchAlgorithm::pyin)
    {
        voicing.minConfidence = 0.0;
    }
    const auto classes = melisma::analysis::classifyFrames (take->source(), take->pitch(), voicing);

    if (!outDir.createDirectory())
    {
        std::cerr << "Cannot create " << outDir.getFullPathName() << '\n';
        return 1;
    }
    const auto base = input.getFileNameWithoutExtension();

    juce::String f0Csv = "time_s,f0_hz,confidence,rms,class\n";
    for (std::size_t i = 0; i < take->pitch().size(); ++i)
    {
        const auto& f = take->pitch().frames[i];
        f0Csv << juce::String (f.timeSeconds, 4) << ',' << juce::String (f.f0Hz, 3) << ','
              << juce::String (f.confidence, 3) << ',' << juce::String (f.rms, 6) << ',' << className (classes[i])
              << '\n';
    }

    juce::String notesCsv = "id,start_s,end_s,duration_s,midi,note,cents_off\n";
    std::cout << '\n'
              << input.getFileName() << ": " << take->notes().size() << " notes ("
              << juce::String (take->source().durationSeconds(), 2) << " s of audio analysed in "
              << juce::String (elapsed, 2) << " s)\n\n";
    std::cout << "   #   start     end    note   cents\n";
    for (const auto& n : take->notes())
    {
        const auto name = melisma::pitch::noteName (n.detectedMidi);
        const double cents = melisma::pitch::centsFromNearestNote (n.detectedMidi);
        notesCsv << static_cast<int> (n.id) << ',' << juce::String (n.startSeconds, 4) << ','
                 << juce::String (n.endSeconds, 4) << ',' << juce::String (n.durationSeconds(), 4) << ','
                 << juce::String (n.detectedMidi, 3) << ',' << juce::String (name) << ',' << juce::String (cents, 1)
                 << '\n';

        std::array<char, 96> line{};
        std::snprintf (line.data(), line.size(), "%4d %7.3f %7.3f   %-5s %+6.1f\n", static_cast<int> (n.id),
                       n.startSeconds, n.endSeconds, name.c_str(), cents);
        std::cout << line.data();
    }

    const auto f0File = outDir.getChildFile (base + ".f0.csv");
    const auto notesFile = outDir.getChildFile (base + ".notes.csv");
    if (!f0File.replaceWithText (f0Csv) || !notesFile.replaceWithText (notesCsv))
    {
        std::cerr << "Cannot write results to " << outDir.getFullPathName() << '\n';
        return 1;
    }
    std::cout << "\nWrote " << f0File.getFullPathName() << "\n      " << notesFile.getFullPathName() << '\n';
    return 0;
}

int main (int argc, char* argv[])
{
    try
    {
        const std::span<char*> raw (argv, static_cast<std::size_t> (argc));
        const std::vector<juce::String> args (raw.begin(), raw.end());
        return run (args);
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "unknown error\n";
    }
    return 1;
}
