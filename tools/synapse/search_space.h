#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>

#include "heuristics/heuristic.h"
#include "heuristics/score.h"

#include "execution_plan/execution_plan.h"

namespace synapse {

struct generated_data_t {
  TargetType target;
  ep_id_t execution_plan;
  Score score;
  Module_ptr module;
  bdd::Node_ptr node;

  generated_data_t(TargetType _target, ep_id_t _execution_plan_id, Score _score)
      : target(_target), execution_plan(_execution_plan_id), score(_score) {}

  generated_data_t(TargetType _target, ep_id_t _execution_plan_id, Score _score,
                   const Module_ptr &_module, bdd::Node_ptr _node)
      : target(_target), execution_plan(_execution_plan_id), score(_score),
        module(_module), node(_node) {}

  generated_data_t(const generated_data_t &data)
      : target(data.target), execution_plan(data.execution_plan),
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
    std::pair<bool, ep_id_t> source_execution_plan;
    std::vector<generated_data_t> data;

    pending_leaves_t() : source_execution_plan(false, 0) {}

    void reset() {
      source_execution_plan.first = false;
      data.clear();
    }

    void set(ep_id_t _source_execution_plan) {
      if (source_execution_plan.first) {
        assert(source_execution_plan.second == _source_execution_plan);
        return;
      }

      source_execution_plan.first = true;
      source_execution_plan.second = _source_execution_plan;
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
    auto target = ep.get_current_platform();
    auto data = generated_data_t(target, ep.get_id(), _hc->get_score(ep));
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

  void add_leaves(const ExecutionPlan &ep, bdd::Node_ptr node,
                  Module_ptr module,
                  const std::vector<ExecutionPlan> &next_eps) {
    assert(hc);
    assert(node);
    assert(module);

    auto target = ep.get_current_platform();
    auto source_execution_plan = ep.get_id();

    pending_leaves.set(source_execution_plan);

    auto n_elems = next_eps.size();

    for (auto i = 0u; i < n_elems; i++) {
      auto next_ep = next_eps[i].get_id();
      auto score = hc->get_score(ep);

      auto data = generated_data_t(target, next_ep, score, module, node);
      pending_leaves.add(data);
    }
  }

  void submit_leaves() {
    if (pending_leaves.data.size() == 0) {
      return;
    }

    auto ss_node_matcher = [&](ss_node_ref node) {
      return pending_leaves.source_execution_plan.first &&
             pending_leaves.source_execution_plan.second ==
                 node->data.execution_plan;
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
      return node->data.execution_plan == ep.get_id();
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
