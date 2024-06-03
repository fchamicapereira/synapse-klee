#pragma once

#include <memory>
#include <vector>
#include <unordered_set>

#include "execution_plan/execution_plan.h"
#include "heuristics/heuristic.h"
#include "search_space.h"

namespace synapse {

struct search_product_t {
  const EP *ep;
  const SearchSpace *search_space;

  search_product_t(const EP *_ep, const SearchSpace *_search_space)
      : ep(_ep), search_space(_search_space) {}

  ~search_product_t() {
    if (ep) {
      delete ep;
      ep = nullptr;
    }

    if (search_space) {
      delete search_space;
      search_space = nullptr;
    }
  }
};

template <class HCfg> class SearchEngine {
private:
  std::shared_ptr<bdd::BDD> bdd;
  std::vector<const Target *> targets;
  Heuristic<HCfg> h;

  bool allow_bdd_reordering;
  std::unordered_set<ep_id_t> peek;

public:
  SearchEngine(const bdd::BDD &bdd, Heuristic<HCfg> h,
               bool allow_bdd_reordering,
               const std::unordered_set<ep_id_t> &peek);

  SearchEngine(const bdd::BDD &bdd, Heuristic<HCfg> h);

  ~SearchEngine();

  search_product_t search();
};

} // namespace synapse
