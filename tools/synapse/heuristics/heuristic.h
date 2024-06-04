#pragma once

#include "../execution_plan/execution_plan.h"
#include "score.h"

#include <set>
#include <random>

namespace synapse {

struct HeuristicCfg {
  virtual Score get_score(const EP *e) const = 0;

  virtual bool operator()(const EP *e1, const EP *e2) const {
    return get_score(e1) > get_score(e2);
  }

  virtual bool terminate_on_first_solution() const = 0;
};

template <class HCfg> class Heuristic {
  static_assert(std::is_base_of<HeuristicCfg, HCfg>::value,
                "HCfg must inherit from HeuristicCfg");

  // What a mouth full...
  // This is the product of the bind expression below.
  typedef std::_Bind<std::uniform_int_distribution<int>(
      std::default_random_engine)>
      CoinTosser;

protected:
  std::multiset<const EP *, HCfg> execution_plans;
  HCfg configuration;

  unsigned rand_seed;
  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> random_dist;
  CoinTosser coin_toss;

public:
  Heuristic(unsigned _rand_seed)
      : rand_seed(_rand_seed), random_engine(rand_seed), random_dist(0, 1),
        coin_toss(std::bind(random_dist, random_engine)) {}

  ~Heuristic() {
    for (const EP *ep : execution_plans) {
      if (ep) {
        delete ep;
        ep = nullptr;
      }
    }
  }

  bool finished() { return get_next_it() == execution_plans.end(); }

  const EP *get() { return *get_best_it(); }

  const EP *get(ep_id_t id) const {
    for (const EP *ep : execution_plans) {
      if (ep->get_id() == id) {
        return ep;
      }
    }

    return nullptr;
  }

  unsigned get_random_seed() const { return rand_seed; }

  std::vector<const EP *> get_all() const {
    std::vector<const EP *> eps;
    eps.assign(execution_plans.begin(), execution_plans.end());
    return eps;
  }

  const EP *pop() {
    auto it = get_next_it();
    assert(it != execution_plans.end());

    const EP *ep = *it;
    execution_plans.erase(it);

    return ep;
  }

  void add(const std::vector<const EP *> &next_eps) {
    for (const EP *ep : next_eps) {
      execution_plans.insert(ep);
    }
  }

  size_t size() const { return execution_plans.size(); }

  const HCfg *get_cfg() const { return &configuration; }

  Score get_score(const EP *e) const {
    auto conf = static_cast<const HeuristicCfg *>(&configuration);
    return conf->get_score(e);
  }

private:
  typename std::set<const EP *, HCfg>::iterator get_best_it() {
    assert(execution_plans.size());

    auto it = execution_plans.begin();
    Score best_score = get_score(*it);

    while (1) {
      if (it == execution_plans.end() || get_score(*it) != best_score) {
        it = execution_plans.begin();
      }

      if (coin_toss()) {
        break;
      }

      it = std::next(it);
    }

    return it;
  }

  typename std::set<const EP *, HCfg>::iterator get_next_it() {
    if (execution_plans.size() == 0) {
      Log::err() << "No more execution plans to pick!\n";
      exit(1);
    }

    auto it = get_best_it();

    const HeuristicCfg *cfg = static_cast<const HeuristicCfg *>(&configuration);
    while (!cfg->terminate_on_first_solution() && it != execution_plans.end() &&
           !(*it)->get_next_node()) {
      ++it;
    }

    if (it != execution_plans.end() && !(*it)->get_next_node()) {
      it = execution_plans.end();
    }

    return it;
  }
};

} // namespace synapse
