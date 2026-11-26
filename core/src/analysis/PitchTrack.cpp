#include "melisma/core/analysis/PitchTrack.h"

#include <algorithm>
#include <cmath>

namespace melisma::analysis
{
std::size_t PitchTrack::frameIndexAt (double seconds) const
{
    if (frames.empty() || hopSeconds <= 0.0)
    {
        return 0;
    }
    const double index = std::round (std::max (0.0, seconds) / hopSeconds);
    return std::min (static_cast<std::size_t> (index), frames.size() - 1);
}
} // namespace melisma::analysis
