#pragma once

#include "../execution_plan/execution_plan.h"
#include "score.h"

#include <set>

#include "../random_engine.h"

namespace synapse {

struct HeuristicCfg {
  std::string name;

  HeuristicCfg(const std::string &_name) : name(_name) {}

  virtual Score get_score(const EP *e) const = 0;

  virtual bool operator()(const EP *e1, const EP *e2) const {
    return get_score(e1) > get_score(e2);
  }
};

template <class HCfg> class Heuristic {
  static_assert(std::is_base_of<HeuristicCfg, HCfg>::value,
                "HCfg must inherit from HeuristicCfg");

protected:
  HCfg configuration;
  std::multiset<const EP *, HCfg> execution_plans;
  typename std::set<const EP *, HCfg>::iterator best_it;
  bool terminate_on_first_solution;

public:
  Heuristic() : terminate_on_first_solution(true) {}

  ~Heuristic() {
    for (const EP *ep : execution_plans) {
      if (ep) {
        delete ep;
      }
    }
  }

  bool finished() { return get_next_it() == execution_plans.end(); }

  const EP *get() {
    get_best_it();
    return *best_it;
  }

  const EP *get(ep_id_t id) const {
    for (const EP *ep : execution_plans) {
      if (ep->get_id() == id) {
        return ep;
      }
    }

    return nullptr;
  }

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

    reset_best_it();

    return ep;
  }

  void add(const std::vector<const EP *> &next_eps) {
    for (const EP *ep : next_eps) {
      execution_plans.insert(ep);
    }

    reset_best_it();
  }

  size_t size() const { return execution_plans.size(); }

  const HCfg *get_cfg() const { return &configuration; }

  Score get_score(const EP *e) const {
    auto conf = static_cast<const HeuristicCfg *>(&configuration);
    return conf->get_score(e);
  }

private:
  void get_best_it() {
    assert(execution_plans.size());

    if (best_it != execution_plans.end()) {
      return;
    }

    best_it = execution_plans.begin();
    Score best_score = get_score(*best_it);

    while (1) {
      if (best_it == execution_plans.end() ||
          get_score(*best_it) != best_score) {
        best_it = execution_plans.begin();
      }

      if (RandomEngine::generate() % 2 == 0) {
        break;
      }

      best_it++;
    }
  }

  void reset_best_it() { best_it = execution_plans.end(); }

  typename std::set<const EP *, HCfg>::iterator get_next_it() {
    if (execution_plans.size() == 0) {
      Log::err() << "No more execution plans to pick!\n";
      exit(1);
    }

    get_best_it();

    auto it = best_it;
    assert(it != execution_plans.end());

    if (terminate_on_first_solution && !(*it)->get_next_node()) {
      return execution_plans.end();
    }

    while (it != execution_plans.end() && !(*it)->get_next_node()) {
      it++;
    }

    return it;
  }
};

} // namespace synapse
