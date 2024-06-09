#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct GalliumComparator : public HeuristicCfg {
  Score get_score(const EP *ep) const override {
    Score score(ep,
                {
                    {ScoreCategory::SwitchNodes, ScoreObjective::MAX},
                    {ScoreCategory::SwitchDataStructures, ScoreObjective::MAX},
                    {ScoreCategory::Recirculations, ScoreObjective::MIN},
                });

    return score;
  }

  bool terminate_on_first_solution() const override { return true; }
};

using Gallium = Heuristic<GalliumComparator>;

} // namespace synapse
