#pragma once

#include "heuristic.h"
#include "score.h"

namespace synapse {

struct GalliumComparator : public HeuristicCfg {
  GalliumComparator() : HeuristicCfg("Gallium") {}

  Score get_score(const EP *ep) const override {
    Score score(
        ep, {
                {ScoreCategory::SwitchNodes, ScoreObjective::MAX},

                // Why processed BDD instead of a processed BDD percentage?
                // Because some modules (e.g. SendToController) increase the
                // number of nodes in the BDD. This means that the percentage of
                // processed BDD nodes can decrease even though the number of
                // processed BDD nodes is increasing, favoring other modules
                // like Recirculate.
                {ScoreCategory::ProcessedBDD, ScoreObjective::MAX},

                // {ScoreCategory::SwitchDataStructures, ScoreObjective::MAX},
                // {ScoreCategory::Recirculations, ScoreObjective::MIN},
            });

    return score;
  }

  bool terminate_on_first_solution() const override { return true; }
};

using Gallium = Heuristic<GalliumComparator>;

} // namespace synapse
