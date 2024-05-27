#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-visualizer.h"

#include "ep_visualizer.h"
#include "../search_space.h"

#include <vector>

namespace synapse {

class SSVisualizer : public Graphviz {
private:
  std::vector<std::string> bdd_fpaths;

public:
  SSVisualizer();

  void visit(const SearchSpace &search_space);

  static void visualize(const SearchSpace &search_space, bool interrupt);
};

} // namespace synapse
