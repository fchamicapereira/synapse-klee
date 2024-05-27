#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct DFSComparator : public HeuristicCfg {
  virtual Score get_score(const EP &ep) const override {
    Score score(ep, {
                        {ScoreCategory::Depth, ScoreObjective::MAX},
                    });
    return score;
  }

  bool terminate_on_first_solution() const override { return true; }
};

using DFS = Heuristic<DFSComparator>;
} // namespace synapse
