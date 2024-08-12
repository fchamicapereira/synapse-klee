#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct RandomComparator : public HeuristicCfg {
  RandomComparator() : HeuristicCfg("Random") {}

  Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::Random, ScoreObjective::MAX},
                    });
    return score;
  }

  bool terminate_on_first_solution() const override { return false; }
};

using Random = Heuristic<RandomComparator>;
} // namespace synapse
