#include "TestSignals.h"

#include <melisma/core/analysis/Analysis.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <vector>

namespace ts = melisma::testsignals;
using melisma::analysis::analyseTake;
using melisma::analysis::AnalysisJob;

namespace
{
melisma::audio::MonoAudio melodyAudio (double repeats = 1.0)
{
    std::vector<ts::Note> notes;
    for (int i = 0; i < static_cast<int> (repeats); ++i)
    {
        notes.insert (notes.end(), {{60.0, 0.3}, {64.0, 0.3}, {67.0, 0.4}});
    }
    const auto s = ts::melody (notes);
    return {s.samples, s.sampleRate};
}
} // namespace

TEST_CASE ("analyseTake runs the whole pipeline", "[core][analysis][pipeline]")
{
    std::vector<double> reported;
    const auto take = analyseTake (melodyAudio(), {},
                                   [&reported] (double p)
                                   {
                                       reported.push_back (p);
                                       return true;
                                   });

    REQUIRE (take.has_value());
    CHECK (take->notes().size() == 3);
    CHECK (take->notes()[0].id != 0);
    CHECK (take->pitch().size() > 100);
    CHECK (take->source().durationSeconds() > 1.0);
    CHECK (take->isUnedited());

    REQUIRE (!reported.empty());
    CHECK (reported.back() == 1.0);
    CHECK (std::ranges::is_sorted (reported));
}

TEST_CASE ("analyseTake can be cancelled", "[core][analysis][pipeline]")
{
    const auto take = analyseTake (melodyAudio (3), {}, [] (double p) { return p < 0.3; });
    CHECK_FALSE (take.has_value());
}

TEST_CASE ("AnalysisJob delivers the take from a background thread", "[core][analysis][pipeline]")
{
    AnalysisJob job (melodyAudio(), {});
    job.wait();

    CHECK (job.isFinished());
    CHECK_FALSE (job.wasCancelled());
    CHECK (job.progress() == 1.0);

    auto take = job.takeResult();
    REQUIRE (take.has_value());
    CHECK (take->notes().size() == 3);
    CHECK_FALSE (job.takeResult().has_value()); // can only be taken once
}

TEST_CASE ("AnalysisJob cancels cleanly", "[core][analysis][pipeline]")
{
    AnalysisJob job (melodyAudio (20), {});
    job.cancel();
    job.wait();

    CHECK (job.isFinished());
    CHECK (job.wasCancelled());
    CHECK_FALSE (job.takeResult().has_value());
}

TEST_CASE ("Destroying a running AnalysisJob stops it", "[core][analysis][pipeline]")
{
    const auto start = std::chrono::steady_clock::now();
    {
        const AnalysisJob job (melodyAudio (40), {});
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
    CHECK (std::chrono::steady_clock::now() - start < std::chrono::seconds (5));
}
