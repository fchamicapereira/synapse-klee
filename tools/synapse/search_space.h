#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>
#include <optional>

#include "heuristics/heuristic.h"
#include "heuristics/score.h"

#include "execution_plan/execution_plan.h"

namespace synapse {

typedef int ss_node_id_t;

struct module_data_t {
  ModuleType type;
  std::string name;
  std::string description;
  bool bdd_reordered;
  double hit_rate;
};

struct bdd_node_data_t {
  bdd::node_id_t id;
  std::string description;
};

struct SSNode {
  ss_node_id_t node_id;
  ep_id_t ep_id;
  Score score;
  TargetType target;
  std::optional<module_data_t> module_data;
  std::optional<bdd_node_data_t> bdd_node_data;
  std::optional<bdd_node_data_t> next_bdd_node_data;
  std::map<std::string, std::string> metadata;
  std::vector<SSNode *> children;

  SSNode(ss_node_id_t _node_id, ep_id_t _ep_id, const Score &_score,
         TargetType _target, const module_data_t &_module_data,
         const bdd_node_data_t &_bdd_node_data,
         const std::optional<bdd_node_data_t> &_next_bdd_node_data,
         const std::map<std::string, std::string> &_metadata)
      : node_id(_node_id), ep_id(_ep_id), score(_score), target(_target),
        module_data(_module_data), bdd_node_data(_bdd_node_data),
        next_bdd_node_data(_next_bdd_node_data), metadata(_metadata) {}

  SSNode(ss_node_id_t _node_id, ep_id_t _ep_id, const Score &_score,
         TargetType _target)
      : node_id(_node_id), ep_id(_ep_id), score(_score), target(_target),
        module_data(std::nullopt), bdd_node_data(std::nullopt),
        next_bdd_node_data(std::nullopt) {}

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
  size_t size;
  const HeuristicCfg *hcfg;

  std::unordered_set<ss_node_id_t> last_eps;
  bool backtrack;

public:
  SearchSpace(const HeuristicCfg *_hcfg)
      : root(nullptr), active_leaf(nullptr), size(0), hcfg(_hcfg),
        backtrack(false) {}

  SearchSpace(const SearchSpace &) = delete;
  SearchSpace(SearchSpace &&) = delete;

  SearchSpace &operator=(const SearchSpace &) = delete;

  ~SearchSpace() { delete root; }

  void activate_leaf(const EP *ep);
  void add_to_active_leaf(const EP *ep, const bdd::Node *node,
                          const ModuleGenerator *mogden,
                          const std::vector<generator_product_t> &products);

  SSNode *get_root() const;
  size_t get_size() const;
  const HeuristicCfg *get_hcfg() const;
  bool is_backtrack() const;

  static std::string build_meta_throughput_estimate(const EP *ep);
  static std::string build_meta_throughput_speculation(const EP *ep);
};

} // namespace synapse
