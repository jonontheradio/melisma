#include "melisma/core/analysis/Analysis.h"

#include <utility>

namespace melisma::analysis
{
std::optional<model::Take> analyseTake (audio::MonoAudio audio, const AnalysisSettings& settings,
                                        const ProgressCallback& progress)
{
    // Pitch detection is most of the work; the rest is reported at the end.
    constexpr double pitchShare = 0.9;
    bool keepGoing = true;
    const ProgressCallback pitchProgress = [&] (double p)
    {
        keepGoing = !progress || progress (p * pitchShare);
        return keepGoing;
    };

    auto track = detectPitchYin (audio, settings.pitch, pitchProgress);
    if (!keepGoing)
    {
        return std::nullopt;
    }

    const auto classes = classifyFrames (audio, track, settings.voicing);
    const auto onsets = detectOnsets (audio, track, settings.onsets);
    auto notes = segmentNotes (track, classes, settings.segmentation, onsets);

    model::Take take (std::move (audio), std::move (track));
    take.setNotes (std::move (notes));

    if (progress && !progress (1.0))
    {
        return std::nullopt;
    }
    return take;
}

AnalysisJob::AnalysisJob (audio::MonoAudio audio, AnalysisSettings settings)
    : worker (
          [this, a = std::move (audio), s = settings] (const std::stop_token& stop) mutable
          {
              auto take = analyseTake (std::move (a), s,
                                       [this, &stop] (double p)
                                       {
                                           progressValue.store (p);
                                           return !stop.stop_requested();
                                       });
              if (take)
              {
                  result = std::make_unique<model::Take> (std::move (*take));
              }
              else
              {
                  cancelled.store (true);
              }
              finished.store (true); // publishes `result` to the reading thread
          })
{
}

AnalysisJob::~AnalysisJob()
{
    worker.request_stop();
    // std::jthread joins on destruction.
}

void AnalysisJob::wait()
{
    if (worker.joinable())
    {
        worker.join();
    }
}

std::optional<model::Take> AnalysisJob::takeResult()
{
    if (!finished.load() || result == nullptr)
    {
        return std::nullopt;
    }
    std::optional<model::Take> out (std::move (*result));
    result.reset();
    return out;
}
} // namespace melisma::analysis
