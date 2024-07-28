#pragma once

#include <memory>
#include <vector>
#include <unordered_set>

#include "execution_plan/execution_plan.h"
#include "heuristics/heuristic.h"
#include "search_space.h"
#include "profiler.h"

namespace synapse {

struct search_config_t {
  std::string heuristic;
  unsigned random_seed;
};

struct search_meta_t {
  size_t ss_size;
  time_t elapsed_time;
  uint64_t steps;
  uint64_t backtracks;
  float branching_factor;
};

struct search_solution_t {
  const EP *ep;
  const SearchSpace *search_space;
  Score score;
  std::string throughput_estimation;
  std::string throughput_speculation;
};

struct search_report_t {
  const search_config_t config;
  const search_solution_t solution;
  const search_meta_t meta;
};

template <class HCfg> class SearchEngine {
private:
  std::shared_ptr<bdd::BDD> bdd;
  Heuristic<HCfg> *h;
  std::shared_ptr<Profiler> profiler;
  const targets_t targets;

  const bool allow_bdd_reordering;
  const std::unordered_set<ep_id_t> peek;
  const bool pause_and_show_on_backtrack;

public:
  SearchEngine(const bdd::BDD *bdd, Heuristic<HCfg> *h, Profiler *profiler,
               const targets_t &targets, bool allow_bdd_reordering,
               const std::unordered_set<ep_id_t> &peek,
               bool _pause_and_show_on_backtrack);

  SearchEngine(const bdd::BDD *bdd, Heuristic<HCfg> *h, Profiler *profiler,
               const targets_t &_targets);

  SearchEngine(const SearchEngine &) = delete;
  SearchEngine(SearchEngine &&) = delete;

  SearchEngine &operator=(const SearchEngine &) = delete;

  search_report_t search();
};

} // namespace synapse
