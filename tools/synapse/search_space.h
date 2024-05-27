#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>

#include "heuristics/heuristic.h"
#include "heuristics/score.h"

#include "execution_plan/execution_plan.h"

namespace synapse {

typedef int ss_node_id_t;

struct SSNode {
  ss_node_id_t node_id;
  ep_id_t ep_id;
  Score score;
  TargetType target;
  const Module *module;
  const bdd::Node *node;
  std::vector<SSNode *> children;

  SSNode(ss_node_id_t _node_id, ep_id_t _ep_id, Score _score,
         TargetType _target, const Module *_module, const bdd::Node *_node)
      : node_id(_node_id), ep_id(_ep_id), score(_score), target(_target),
        module(_module), node(_node) {}

  SSNode(ss_node_id_t _node_id, ep_id_t _ep_id, Score _score,
         TargetType _target)
      : SSNode(_node_id, _ep_id, _score, _target, nullptr, nullptr) {}

  ~SSNode() {
    for (SSNode *child : children) {
      if (child) {
        delete child;
        child = nullptr;
      }
    }
  }
};

class SearchSpace {
private:
  SSNode *root;
  SSNode *active_leaf;
  std::vector<SSNode *> leaves;
  std::unordered_set<ss_node_id_t> explored;
  const HeuristicCfg *hcfg;

public:
  SearchSpace(const HeuristicCfg *_hcfg)
      : root(nullptr), active_leaf(nullptr), hcfg(_hcfg) {}

  ~SearchSpace() { delete root; }

  void activate_leaf(const EP &ep);
  void add_to_active_leaf(const bdd::Node *node, const Module *module,
                          const std::vector<EP> &generated_eps);

  SSNode *get_root() const;
  bool was_explored(ss_node_id_t node_id) const;
};

} // namespace synapse
