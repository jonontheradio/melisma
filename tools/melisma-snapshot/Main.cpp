// melisma-snapshot: render the piano roll of an analysed file to a PNG.
//
//   melisma-snapshot voice.wav out.png [width height] [--select N] [--shift-cents C]
//
// --select N       selects the N-th note (1-based) before rendering
// --shift-cents C  moves the selected note by C cents (to see an edited blob)
#include "AudioFileLoader.h"
#include "PianoRoll.h"

#include <melisma/core/analysis/Analysis.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <exception>
#include <iostream>
#include <span>
#include <vector>

static int run (const std::vector<juce::String>& args)
{
    if (args.size() < 3)
    {
        std::cerr << "usage: melisma-snapshot <audio> <out.png> [width height] [--select N] [--shift-cents C]\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI gui;
    const auto cwd = juce::File::getCurrentWorkingDirectory();

    int width = 1200;
    int height = 600;
    int select = 0;
    double shiftCents = 0.0;
    std::size_t i = 3;
    if (args.size() >= 5 && args[3].containsOnly ("0123456789"))
    {
        width = args[3].getIntValue();
        height = args[4].getIntValue();
        i = 5;
    }
    for (; i + 1 < args.size(); i += 2)
    {
        if (args[i] == "--select")
        {
            select = args[i + 1].getIntValue();
        }
        else if (args[i] == "--shift-cents")
        {
            shiftCents = args[i + 1].getDoubleValue();
        }
    }

    juce::String error;
    auto loaded = melisma::adapters::loadAudioFile (cwd.getChildFile (args[1]), error);
    if (!loaded)
    {
        std::cerr << error << '\n';
        return 1;
    }
    auto take = melisma::analysis::analyseTake (std::move (loaded->audio));
    if (!take)
    {
        return 1;
    }

    melisma::ui::PianoRoll roll;
    roll.setSize (width, height);
    roll.setTake (&*take);
    if (select > 0 && static_cast<std::size_t> (select) <= take->notes().size())
    {
        const auto id = take->notes()[static_cast<std::size_t> (select - 1)].id;
        roll.selection().selectOnly (id);
        take->findNote (id)->edits.pitchOffsetCents = shiftCents;
    }
    roll.setPlayheadSeconds (0.25 * take->source().durationSeconds());

    const auto image = roll.createComponentSnapshot (roll.getLocalBounds(), true, 2.0f);
    juce::PNGImageFormat png;
    const auto outFile = cwd.getChildFile (args[2]);
    outFile.deleteFile();
    juce::FileOutputStream out (outFile);
    if (!out.openedOk() || !png.writeImageToStream (image, out))
    {
        std::cerr << "Cannot write " << outFile.getFullPathName() << '\n';
        return 1;
    }
    std::cout << take->notes().size() << " notes -> " << outFile.getFullPathName() << '\n';
    return 0;
}

int main (int argc, char* argv[])
{
    try
    {
        const std::span<char*> raw (argv, static_cast<std::size_t> (argc));
        return run ({raw.begin(), raw.end()});
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
    }
    return 1;
}
