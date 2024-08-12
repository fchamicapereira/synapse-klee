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
};

struct search_meta_t {
  size_t ss_size;
  time_t elapsed_time;
  uint64_t steps;
  uint64_t backtracks;
  std::unordered_map<bdd::node_id_t, int> visits_per_node;
  std::unordered_map<bdd::node_id_t, float> avg_children_per_node;
  uint64_t avg_bdd_size;
  float branching_factor;
  float total_ss_size_estimation;
  int solutions;

  search_meta_t()
      : ss_size(0), elapsed_time(0), steps(0), backtracks(0), avg_bdd_size(0),
        branching_factor(0), total_ss_size_estimation(0), solutions(0) {}

  search_meta_t(const search_meta_t &other) = default;
  search_meta_t(search_meta_t &&other) = default;
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
