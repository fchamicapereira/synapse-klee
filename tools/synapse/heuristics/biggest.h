#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct BiggestComparator : public HeuristicCfg {
  Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::Nodes, ScoreObjective::MAX},
                    });
    return score;
  }

  bool terminate_on_first_solution() const override { return false; }
};

using Biggest = Heuristic<BiggestComparator>;
} // namespace synapse
