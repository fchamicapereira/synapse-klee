#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct MostCompactComparator : public HeuristicCfg {
  MostCompactComparator() : HeuristicCfg("MostCompact") {}

  Score get_score(const EP *ep) const override {
    Score score(ep, {
                        {ScoreCategory::Nodes, ScoreObjective::MIN},
                    });
    return score;
  }

  bool terminate_on_first_solution() const override { return false; }
};

using MostCompact = Heuristic<MostCompactComparator>;
} // namespace synapse
