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

  bool terminate_on_first_solution() const override { return false; }
};

using LeastReordered = Heuristic<LeastReorderedComparator>;
} // namespace synapse
