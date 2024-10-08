#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct BFSComparator : public HeuristicCfg {
  BFSComparator() : HeuristicCfg("BFS") {}

  virtual Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::Depth, ScoreObjective::MIN},
                    });
    return score;
  }
};

using BFS = Heuristic<BFSComparator>;

} // namespace synapse
