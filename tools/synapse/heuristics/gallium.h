#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct GalliumComparator : public HeuristicCfg {
  Score get_score(const EP *ep) const override {
    Score score(
        ep, {
                {ScoreCategory::NumberOfCounters, ScoreObjective::MAX},
                {ScoreCategory::NumberOfSimpleTables, ScoreObjective::MAX},
                {ScoreCategory::NumberOfSwitchNodes, ScoreObjective::MAX},

                // Let's add this one to just speed up the process when
                // we are generating controller nodes. After all, we
                // only get to this point if all the metrics behind this
                // one are the same, and by that point who cares.
                {ScoreCategory::ProcessedBDDPercentage, ScoreObjective::MAX},
            });

    return score;
  }

  bool terminate_on_first_solution() const override { return true; }
};

using Gallium = Heuristic<GalliumComparator>;

} // namespace synapse
