#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>

#include "heuristics/heuristic.h"
#include "heuristics/score.h"

namespace synapse {

struct generated_data_t {
  TargetType target;
  int execution_plan_id;
  Score score;
  Module_ptr module;
  BDD::Node_ptr node;

  generated_data_t(TargetType _target, int _execution_plan_id, Score _score)
      : target(_target), execution_plan_id(_execution_plan_id), score(_score) {}

  generated_data_t(TargetType _target, int _execution_plan_id, Score _score,
                   const Module_ptr &_module, BDD::Node_ptr _node)
      : target(_target), execution_plan_id(_execution_plan_id), score(_score),
        module(_module), node(_node) {}

  generated_data_t(const generated_data_t &data)
      : target(data.target), execution_plan_id(data.execution_plan_id),
        score(data.score), module(data.module), node(data.node) {}
};

typedef int ss_node_id_t;

struct ss_node_t {
  ss_node_id_t node_id;
  generated_data_t data;
  std::shared_ptr<ss_node_t> prev;
  std::vector<std::shared_ptr<ss_node_t>> next;

  ss_node_t(ss_node_id_t _node_id, const generated_data_t &_data,
            std::shared_ptr<ss_node_t> _prev)
      : node_id(_node_id), data(_data), prev(_prev) {}

  ss_node_t(ss_node_id_t _node_id, const generated_data_t &_data)
      : node_id(_node_id), data(_data) {}
};

typedef std::shared_ptr<ss_node_t> ss_node_ref;

class SearchSpace {
private:
  struct pending_leaves_t {
    int source_execution_plan_id;
    std::vector<generated_data_t> data;

    pending_leaves_t() : source_execution_plan_id(-1) {}

    void reset() {
      source_execution_plan_id = -1;
      data.clear();
    }

    void add(const generated_data_t &_data) { data.push_back(_data); }
  };

private:
  ss_node_id_t node_id_counter;
  ss_node_ref root;
  std::vector<ss_node_ref> leaves;
  pending_leaves_t pending_leaves;
  std::unordered_set<ss_node_id_t> winners;

  const HeuristicConfiguration *hc;

public:
  SearchSpace() : node_id_counter(0) {}

  void init(const HeuristicConfiguration *_hc, const ExecutionPlan &ep) {
    auto target = ep.get_current_target();
    auto data = generated_data_t(target->type, ep.get_id(), _hc->get_score(ep));
    auto node_id = get_and_inc_node_id();

    root = ss_node_ref(new ss_node_t(node_id, data));
    hc = _hc;
    leaves.push_back(root);
  }

  ss_node_id_t get_and_inc_node_id() {
    auto node_id = node_id_counter;
    node_id_counter++;
    return node_id;
  }

  void add_leaves(const ExecutionPlan &ep, BDD::Node_ptr node,
                  Module_ptr module,
                  const std::vector<ExecutionPlan> &next_eps) {
    assert(hc);
    assert(node);
    assert(module);

    auto target = ep.get_current_target();

    int source_execution_plan_id = ep.get_id();

    assert(pending_leaves.source_execution_plan_id == -1 ||
           pending_leaves.source_execution_plan_id == source_execution_plan_id);

    pending_leaves.source_execution_plan_id = source_execution_plan_id;

    auto n_elems = next_eps.size();

    for (auto i = 0u; i < n_elems; i++) {
      auto next_ep = next_eps[i].get_id();
      auto score = hc->get_score(ep);

      auto data = generated_data_t(target->type, next_ep, score, module, node);
      pending_leaves.add(data);
    }
  }

  void submit_leaves() {
    if (pending_leaves.data.size() == 0) {
      return;
    }

    auto ss_node_matcher = [&](ss_node_ref node) {
      return node->data.execution_plan_id ==
             pending_leaves.source_execution_plan_id;
    };

    auto found_it = std::find_if(leaves.begin(), leaves.end(), ss_node_matcher);
    assert(found_it != leaves.end() && "Leaf not found");

    auto leaf = *found_it;
    leaves.erase(found_it);

    for (auto i = 0u; i < pending_leaves.data.size(); i++) {
      auto node_id = get_and_inc_node_id();
      const auto &data = pending_leaves.data[i];

      leaf->next.emplace_back(new ss_node_t(node_id, data, leaf));
      leaves.push_back(leaf->next.back());
    }

    pending_leaves.reset();
  }

  void set_winner(const ExecutionPlan &ep) {
    winners.clear();

    auto ss_node_matcher = [&](ss_node_ref node) {
      if (node->data.execution_plan_id < 0) {
        return false;
      }

      auto node_id = static_cast<unsigned>(node->data.execution_plan_id);
      return node_id == ep.get_id();
    };

    auto found_it = std::find_if(leaves.begin(), leaves.end(), ss_node_matcher);
    assert(found_it != leaves.end() && "Leaf not found");

    auto node = *found_it;

    while (node) {
      winners.insert(node->node_id);
      node = node->prev;
    }
  }

  bool is_winner(ss_node_ref node) const {
    assert(node);
    return winners.find(node->node_id) != winners.end();
  }

  const std::vector<ss_node_ref> &get_leaves() const { return leaves; }
  const ss_node_ref &get_root() const { return root; }
};
} // namespace synapse
