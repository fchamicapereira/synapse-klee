#pragma once

#include <memory>
#include <vector>
#include <unordered_set>

#include "execution_plan/execution_plan.h"
#include "heuristics/heuristic.h"
#include "search_space.h"

namespace synapse {

template <class HCfg> class SearchEngine {
private:
  std::shared_ptr<bdd::BDD> bdd;
  std::vector<const Target *> targets;
  Heuristic<HCfg> h;
  SearchSpace search_space;

  bool bdd_reordering;
  bdd::nodes_t nodes_to_peek;

public:
  SearchEngine(const bdd::BDD &bdd, Heuristic<HCfg> h, bool bdd_reordering,
               const bdd::nodes_t &nodes_to_peek);

  SearchEngine(const bdd::BDD &bdd, Heuristic<HCfg> h);

  ~SearchEngine();

  EP search();

  const SearchSpace &get_search_space() const { return search_space; }
};

} // namespace synapse
