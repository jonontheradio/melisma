#pragma once

#include "melisma/core/analysis/NoteSegmentation.h"
#include "melisma/core/analysis/Voicing.h"
#include "melisma/core/analysis/Yin.h"
#include "melisma/core/model/Take.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

namespace melisma::analysis
{
/** All settings of the analysis pipeline (see docs/PLAN.md, "Análisis"). */
struct AnalysisSettings
{
    YinSettings pitch;
    VoicingSettings voicing;
    SegmentationSettings segmentation;
};

/** Runs the whole pipeline on analysis-ready audio:
    pitch detection -> voicing -> note segmentation -> Take.
    Returns nullopt if `progress` cancelled it. */
std::optional<model::Take> analyseTake (audio::MonoAudio audio, const AnalysisSettings& settings = {},
                                        const ProgressCallback& progress = {});

/** Runs analyseTake on a background thread.

    The owner polls progress()/isFinished() (e.g. from a GUI timer) and takes
    the result once finished. Destroying the job cancels it and waits for the
    thread, so it never outlives its owner. */
class AnalysisJob
{
public:
    AnalysisJob (audio::MonoAudio audio, AnalysisSettings settings);
    ~AnalysisJob();

    AnalysisJob (const AnalysisJob&) = delete;
    AnalysisJob& operator= (const AnalysisJob&) = delete;
    AnalysisJob (AnalysisJob&&) = delete;
    AnalysisJob& operator= (AnalysisJob&&) = delete;

    /** 0..1 */
    double progress() const { return progressValue.load(); }
    bool isFinished() const { return finished.load(); }
    bool wasCancelled() const { return cancelled.load(); }

    /** Requests cancellation; the thread stops at its next progress check. */
    void cancel() { worker.request_stop(); }

    /** Blocks until the job ends. */
    void wait();

    /** The analysed take. Only valid once isFinished() and not cancelled;
        can be taken once. */
    std::optional<model::Take> takeResult();

private:
    std::atomic<double> progressValue{0.0};
    std::atomic<bool> finished{false};
    std::atomic<bool> cancelled{false};
    std::unique_ptr<model::Take> result;
    std::jthread worker; // last member: started after everything above exists
};
} // namespace melisma::analysis
