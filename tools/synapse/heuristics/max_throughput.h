#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct MaxThroughputComparator : public HeuristicCfg {
  Score get_score(const EP *ep) const override {
    Score score(
        ep, {
                {ScoreCategory::SpeculativeThroughput, ScoreObjective::MAX},

                // Now that we use speculative throughput, many EPs will have
                // the exact same score.
                // Let's solve the ties by looking at the actual throughput.
                {ScoreCategory::Throughput, ScoreObjective::MAX},

                // Let's incentivize the ones that have already processed more
                // BDD nodes.
                {ScoreCategory::ProcessedBDDPercentage, ScoreObjective::MAX},
            });

    return score;
  }

  bool terminate_on_first_solution() const override { return true; }
};

using MaxThroughput = Heuristic<MaxThroughputComparator>;

} // namespace synapse
