#pragma once

#include "../execution_plan/execution_plan.h"
#include "score.h"

#include <set>

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

protected:
  std::multiset<const EP *, HCfg> execution_plans;
  HCfg configuration;

public:
  ~Heuristic() {
    for (const EP *ep : execution_plans) {
      if (ep) {
        delete ep;
        ep = nullptr;
      }
    }
  }

private:
  typename std::set<const EP *, HCfg>::iterator get_best_it() const {
    assert(execution_plans.size());
    return execution_plans.begin();
  }

  typename std::set<const EP *, HCfg>::iterator get_next_it() const {
    if (execution_plans.size() == 0) {
      Log::err() << "No more execution plans to pick!\n";
      exit(1);
    }

    const HeuristicCfg *cfg = static_cast<const HeuristicCfg *>(&configuration);

    auto it = execution_plans.begin();
    while (!cfg->terminate_on_first_solution() && it != execution_plans.end() &&
           !(*it)->get_next_node()) {
      ++it;
    }

    if (it != execution_plans.end() && !(*it)->get_next_node()) {
      it = execution_plans.end();
    }

    return it;
  }

public:
  bool finished() const { return get_next_it() == execution_plans.end(); }

  const EP *get() { return *get_best_it(); }

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
      bool found = false;

      for (const EP *saved_ep : execution_plans) {
        if (saved_ep == ep) {
          found = true;
          break;
        }
      }

      if (found) {
        continue;
      }

      execution_plans.insert(ep);
    }
  }

  size_t size() const { return execution_plans.size(); }

  const HCfg *get_cfg() const { return &configuration; }

  Score get_score(const EP *e) const {
    auto conf = static_cast<const HeuristicCfg *>(&configuration);
    return conf->get_score(e);
  }
};
} // namespace synapse
