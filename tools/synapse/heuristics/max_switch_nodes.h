#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct MaxSwitchNodesComparator : public HeuristicCfg {
  MaxSwitchNodesComparator() : HeuristicCfg("MaxSwitchNodes") {}

  Score get_score(const EP *ep) const override {
    Score score(
        ep, {
                {ScoreCategory::SwitchNodes, ScoreObjective::MAX},
                {ScoreCategory::SwitchLeaves, ScoreObjective::MAX},
                // {ScoreCategory::HasNextStatefulOperationInSwitch,
                // ScoreObjective::MAX},
                {ScoreCategory::ConsecutiveObjectOperationsInSwitch,
                 ScoreObjective::MAX},

                // Let's add this one to just speed up the process when
                // we are generating controller nodes. After all, we
                // only get to this point if all the metrics behind this
                // one are the same, and by that point who cares.
                {ScoreCategory::ProcessedBDDPercentage, ScoreObjective::MAX},
            });

    return score;
  }
};

using MaxSwitchNodes = Heuristic<MaxSwitchNodesComparator>;

} // namespace synapse
