#pragma once

#include "call-paths-to-bdd.h"
#include "target.h"

#include <unordered_map>
#include <unordered_set>

namespace synapse {

class Module;
typedef std::shared_ptr<Module> Module_ptr;

class MemoryBank;
typedef std::shared_ptr<MemoryBank> MemoryBank_ptr;

class ExecutionPlanNode;
typedef std::shared_ptr<ExecutionPlanNode> ExecutionPlanNode_ptr;

class ExecutionPlanVisitor;

class ExecutionPlan {
  friend class ExecutionPlanNode;

public:
  struct leaf_t {
    ExecutionPlanNode_ptr leaf;
    BDD::Node_ptr next;
    std::pair<bool, TargetType> current_platform;

    leaf_t(BDD::Node_ptr _next);
    leaf_t(Module_ptr _module, BDD::Node_ptr _next);
    leaf_t(const leaf_t &_leaf);

    leaf_t &operator=(const leaf_t &) = default;
  };

private:
  ExecutionPlanNode_ptr root;
  std::vector<leaf_t> leaves;
  BDD::BDD bdd;

  MemoryBank_ptr shared_memory_bank;
  std::unordered_map<TargetType, TargetMemoryBank_ptr> memory_banks;
  std::unordered_set<BDD::node_id_t> processed_bdd_nodes;

  unsigned depth;
  unsigned nodes;
  TargetType initial_target;
  std::unordered_set<TargetType> targets;
  std::unordered_map<TargetType, std::unordered_set<BDD::node_id_t>>
      targets_bdd_starting_points;
  std::map<TargetType, unsigned> nodes_per_target;
  unsigned reordered_nodes;
  unsigned id;

  static int counter;

public:
  ExecutionPlan(const BDD::BDD &_bdd);
  ExecutionPlan(const ExecutionPlan &ep);
  ExecutionPlan(const ExecutionPlan &ep, ExecutionPlanNode_ptr _root);

  ExecutionPlan &operator=(const ExecutionPlan &) = default;

  unsigned get_depth() const;
  unsigned get_nodes() const;

  const std::unordered_map<TargetType, std::unordered_set<BDD::node_id_t>> &
  get_targets_bdd_starting_points() const;

  std::unordered_set<BDD::node_id_t>
  get_current_target_bdd_starting_points() const;

  BDD::Node_ptr get_bdd_root(BDD::Node_ptr node) const;

  const std::map<TargetType, unsigned> &get_nodes_per_target() const;

  unsigned get_id() const;
  const std::vector<leaf_t> &get_leaves() const;
  const BDD::BDD &get_bdd() const;
  BDD::BDD &get_bdd();
  unsigned get_reordered_nodes() const;

  std::vector<ExecutionPlanNode_ptr> get_prev_nodes() const;
  std::vector<ExecutionPlanNode_ptr> get_prev_nodes_of_current_target() const;

  std::vector<BDD::Node_ptr> get_incoming_bdd_nodes() const;

  void inc_reordered_nodes();
  const ExecutionPlanNode_ptr &get_root() const;

  void add_target(TargetType type, TargetMemoryBank_ptr mb);
  bool has_target(TargetType type) const;

  MemoryBank_ptr get_memory_bank() const;

  template <class MB> MB *get_memory_bank(TargetType type) const {
    static_assert(std::is_base_of<TargetMemoryBank, MB>::value,
                  "MB not derived from TargetMemoryBank");
    assert(memory_banks.find(type) != memory_banks.end());
    return static_cast<MB *>(memory_banks.at(type).get());
  }

  const std::unordered_set<BDD::node_id_t> &get_processed_bdd_nodes() const;

  BDD::Node_ptr get_next_node() const;
  ExecutionPlanNode_ptr get_active_leaf() const;
  TargetType get_current_platform() const;

  ExecutionPlan replace_leaf(Module_ptr new_module, const BDD::Node_ptr &next,
                             bool process_bdd_node = true) const;

  ExecutionPlan ignore_leaf(const BDD::Node_ptr &next, TargetType next_target,
                            bool process_bdd_node = true) const;

  ExecutionPlan add_leaves(Module_ptr new_module, const BDD::Node_ptr &next,
                           bool is_terminal = false,
                           bool process_bdd_node = true) const;

  // Order matters!
  // The active leaf will correspond to the first branch in the branches
  ExecutionPlan add_leaves(std::vector<leaf_t> _leaves,
                           bool is_terminal = false,
                           bool process_bdd_node = true) const;

  void replace_active_leaf_node(BDD::Node_ptr next,
                                bool process_bdd_node = true);

  void force_termination();

  float get_bdd_processing_progress() const;
  void remove_from_processed_bdd_nodes(BDD::node_id_t id);
  void add_processed_bdd_node(BDD::node_id_t id);
  void replace_current_target_starting_points(BDD::node_id_t _old,
                                              BDD::node_id_t _new);

  void visit(ExecutionPlanVisitor &visitor) const;

  ExecutionPlan clone(BDD::BDD new_bdd) const;
  ExecutionPlan clone(bool deep = false) const;

private:
  void update_targets_starting_points(std::vector<leaf_t> new_leaves);
  void update_leaves(std::vector<leaf_t> _leaves, bool is_terminal);
  void update_processed_nodes();

  ExecutionPlanNode_ptr clone_nodes(ExecutionPlan &ep,
                                    const ExecutionPlanNode *node) const;
};

bool operator==(const ExecutionPlan &lhs, const ExecutionPlan &rhs);

} // namespace synapse