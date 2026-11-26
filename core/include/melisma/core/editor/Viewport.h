#pragma once

namespace melisma::editor
{
/** What part of the take is visible and how it maps to pixels.
    x grows with time; y grows downwards while pitch grows upwards. */
class Viewport
{
public:
    // Content limits.
    static constexpr double lowestMidi = 24.0;   // C1
    static constexpr double highestMidi = 108.0; // C8
    static constexpr double minVisibleSeconds = 0.05;
    static constexpr double minVisibleSemitones = 6.0;

    void setSize (double widthPixels, double heightPixels);
    void setContentDuration (double seconds);

    double width() const { return widthPx; }
    double height() const { return heightPx; }

    double startSeconds() const { return timeStart; }
    double endSeconds() const { return timeStart + timeSpan; }
    double visibleSeconds() const { return timeSpan; }
    double lowMidi() const { return midiLow; }
    double highMidi() const { return midiLow + midiSpan; }
    double visibleSemitones() const { return midiSpan; }

    /** Shows exactly this region (clamped to the content limits). */
    void setVisibleRange (double startSec, double endSec, double lowMidiValue, double highMidiValue);

    double timeToX (double seconds) const;
    double xToTime (double x) const;
    double midiToY (double midi) const;
    double yToMidi (double y) const;
    double pixelsPerSecond() const { return widthPx / timeSpan; }
    double pixelsPerSemitone() const { return heightPx / midiSpan; }

    /** Zooms by `factor` (> 1 = zoom in) keeping the point under the given pixel fixed. */
    void zoomTime (double factor, double anchorX);
    void zoomPitch (double factor, double anchorY);

    /** Scrolls by a number of pixels (positive = later in time / higher pitch). */
    void scrollTime (double pixels);
    void scrollPitch (double pixels);

private:
    void clamp();

    double widthPx = 800.0;
    double heightPx = 400.0;
    double duration = 10.0;

    double timeStart = 0.0;
    double timeSpan = 10.0;
    double midiLow = 48.0;
    double midiSpan = 24.0;
};
} // namespace melisma::editor
