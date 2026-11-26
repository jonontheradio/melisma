#include "melisma/core/editor/Viewport.h"

#include <algorithm>

namespace melisma::editor
{
void Viewport::setSize (double widthPixels, double heightPixels)
{
    widthPx = std::max (1.0, widthPixels);
    heightPx = std::max (1.0, heightPixels);
}

void Viewport::setContentDuration (double seconds)
{
    duration = std::max (minVisibleSeconds, seconds);
    clamp();
}

void Viewport::setVisibleRange (double startSec, double endSec, double lowMidiValue, double highMidiValue)
{
    timeStart = startSec;
    timeSpan = endSec - startSec;
    midiLow = lowMidiValue;
    midiSpan = highMidiValue - lowMidiValue;
    clamp();
}

double Viewport::timeToX (double seconds) const
{
    return (seconds - timeStart) / timeSpan * widthPx;
}

double Viewport::xToTime (double x) const
{
    return timeStart + x / widthPx * timeSpan;
}

double Viewport::midiToY (double midi) const
{
    return (midiLow + midiSpan - midi) / midiSpan * heightPx;
}

double Viewport::yToMidi (double y) const
{
    return midiLow + midiSpan - y / heightPx * midiSpan;
}

void Viewport::zoomTime (double factor, double anchorX)
{
    if (factor <= 0.0)
    {
        return;
    }
    const double anchorTime = xToTime (anchorX);
    timeSpan /= factor;
    timeSpan = std::clamp (timeSpan, minVisibleSeconds, duration);
    timeStart = anchorTime - anchorX / widthPx * timeSpan;
    clamp();
}

void Viewport::zoomPitch (double factor, double anchorY)
{
    if (factor <= 0.0)
    {
        return;
    }
    const double anchorMidi = yToMidi (anchorY);
    midiSpan /= factor;
    midiSpan = std::clamp (midiSpan, minVisibleSemitones, highestMidi - lowestMidi);
    midiLow = anchorMidi - (1.0 - anchorY / heightPx) * midiSpan;
    clamp();
}

void Viewport::scrollTime (double pixels)
{
    timeStart += pixels / pixelsPerSecond();
    clamp();
}

void Viewport::scrollPitch (double pixels)
{
    midiLow += pixels / pixelsPerSemitone();
    clamp();
}

void Viewport::clamp()
{
    timeSpan = std::clamp (timeSpan, minVisibleSeconds, std::max (minVisibleSeconds, duration));
    timeStart = std::clamp (timeStart, 0.0, std::max (0.0, duration - timeSpan));
    midiSpan = std::clamp (midiSpan, minVisibleSemitones, highestMidi - lowestMidi);
    midiLow = std::clamp (midiLow, lowestMidi, highestMidi - midiSpan);
}
} // namespace melisma::editor
