#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct LeastReorderedComparator : public HeuristicCfg {
  LeastReorderedComparator() : HeuristicCfg("LeastReordered") {}

  Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::ReorderedNodes, ScoreObjective::MIN},
                        {ScoreCategory::Nodes, ScoreObjective::MAX},
                    });
    return score;
  }
};

using LeastReordered = Heuristic<LeastReorderedComparator>;

} // namespace synapse
