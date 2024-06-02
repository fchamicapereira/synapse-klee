#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-visualizer.h"

#include "ep_visualizer.h"
#include "../search_space.h"

#include <vector>

namespace synapse {

class SSVisualizer : public Graphviz {
private:
  std::set<ep_id_t> highlight;

  SSVisualizer();
  SSVisualizer(const EP *highlight);

  void visit(const SearchSpace *search_space);

public:
  static void visualize(const SearchSpace *search_space, bool interrupt);
  static void visualize(const SearchSpace *search_space, const EP *highlight,
                        bool interrupt);
};

} // namespace synapse
