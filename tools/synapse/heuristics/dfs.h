#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct DFSComparator : public HeuristicCfg {
  DFSComparator() : HeuristicCfg("DFS") {}

  virtual Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::Depth, ScoreObjective::MAX},
                    });
    return score;
  }
};

using DFS = Heuristic<DFSComparator>;

} // namespace synapse
