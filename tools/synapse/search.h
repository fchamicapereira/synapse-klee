#pragma once

#include <memory>
#include <vector>
#include <unordered_set>

#include "execution_plan/execution_plan.h"
#include "heuristics/heuristic.h"
#include "search_space.h"
#include "profiler.h"

namespace synapse {

struct search_report_t {
  const EP *ep;
  const SearchSpace *search_space;

  const std::string heuristic_name;
  const unsigned random_seed;
  const size_t ss_size;
  const Score winner_score;
  const double search_time;
  const uint64_t backtracks;

  search_report_t(const EP *_ep, const SearchSpace *_search_space,
                  const std::string &_heuristic_name, unsigned _random_seed,
                  size_t _ss_size, Score _winner_score, double _search_time,
                  uint64_t _backtracks);

  search_report_t(const search_report_t &) = delete;
  search_report_t(search_report_t &&other);

  ~search_report_t();
};

template <class HCfg> class SearchEngine {
private:
  std::shared_ptr<bdd::BDD> bdd;
  std::vector<const Target *> targets;
  Heuristic<HCfg> *h;
  std::shared_ptr<Profiler> profiler;

  bool allow_bdd_reordering;
  std::unordered_set<ep_id_t> peek;
  bool pause_and_show_on_backtrack;

public:
  SearchEngine(const bdd::BDD *bdd, Heuristic<HCfg> *h, Profiler *profiler,
               bool allow_bdd_reordering,
               const std::unordered_set<ep_id_t> &peek,
               bool _pause_and_show_on_backtrack);

  SearchEngine(const bdd::BDD *bdd, Heuristic<HCfg> *h, Profiler *profiler);

  SearchEngine(const SearchEngine &) = delete;
  SearchEngine(SearchEngine &&) = delete;

  SearchEngine &operator=(const SearchEngine &) = delete;

  ~SearchEngine();

  search_report_t search();
};

} // namespace synapse
