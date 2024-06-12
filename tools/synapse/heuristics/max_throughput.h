#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct MaxThroughputComparator : public HeuristicCfg {
  Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::Throughput, ScoreObjective::MAX},
                    });

    return score;
  }

  bool terminate_on_first_solution() const override { return true; }
};

using MaxThroughput = Heuristic<MaxThroughputComparator>;

} // namespace synapse
