#pragma once

#include "call-paths-to-bdd.h"
#include "target.h"
#include "clone.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

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
  std::unordered_map<target_id_t, Target_ptr> targets;
  std::unordered_map<TargetType, std::vector<Target_ptr>> target_types;
  std::vector<Target_ptr> v_targets;
  std::unordered_set<BDD::node_id_t> processed_bdd_nodes;

  std::shared_ptr<Clone::Infrastructure> infrastructure;

  unsigned depth;
  unsigned nodes;
  Target_ptr initial_target;
  Target_ptr current_target;
  std::unordered_map<target_id_t, std::unordered_set<BDD::node_id_t>>
      targets_bdd_starting_points;
  std::map<TargetType, unsigned> nodes_per_target_type;
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

  const std::unordered_map<target_id_t, std::unordered_set<BDD::node_id_t>> &
  get_targets_bdd_starting_points() const;

  std::unordered_set<BDD::node_id_t>
  get_current_target_bdd_starting_points() const;

  BDD::Node_ptr get_bdd_root(BDD::Node_ptr node) const;

  const std::map<TargetType, unsigned> &get_nodes_per_target_type() const;

  unsigned get_id() const;
  const std::vector<leaf_t> &get_leaves() const;
  const BDD::BDD &get_bdd() const;
  BDD::BDD &get_bdd();
  unsigned get_reordered_nodes() const;

  std::vector<ExecutionPlanNode_ptr> get_prev_nodes() const;
  std::vector<ExecutionPlanNode_ptr> get_prev_nodes_of_current_target() const;

  Target_ptr get_target(TargetType target_type, const std::string &instance) const;

  std::vector<BDD::Node_ptr> get_incoming_bdd_nodes() const;

  void inc_reordered_nodes();
  const ExecutionPlanNode_ptr &get_root() const;

  void add_target(Target_ptr target);
  const std::vector<Target_ptr>& get_from_target_type(TargetType type) const;
  const std::vector<Target_ptr> & get_targets() const;
  bool has_target_type(TargetType type) const;

  MemoryBank_ptr get_memory_bank() const;

  Target_ptr get_first_of_type(TargetType type) const;

  // TODO: remove at one point
  template <class MB> MB *get_memory_bank(TargetType type) const {
    static_assert(std::is_base_of<TargetMemoryBank, MB>::value,
                  "MB not derived from TargetMemoryBank");
    return static_cast<MB *>(get_first_of_type(type)->memory_bank.get());
  }

  template <class MB> MB *get_memory_bank(target_id_t id) const {
    static_assert(std::is_base_of<TargetMemoryBank, MB>::value,
                  "MB not derived from TargetMemoryBank");
    assert(targets.find(id) != targets.end());
    return static_cast<MB *>(targets.at(id)->memory_bank.get());
  }

  const std::unordered_set<BDD::node_id_t> &get_processed_bdd_nodes() const;

  BDD::Node_ptr get_next_node() const;
  ExecutionPlanNode_ptr get_active_leaf() const;
  Target_ptr get_current_target() const;

  ExecutionPlan replace_leaf(Module_ptr new_module, const BDD::Node_ptr &next,
                             bool process_bdd_node = true) const;

  ExecutionPlan ignore_leaf(const BDD::Node_ptr &next, TargetType next_target,
                            bool process_bdd_node = true) const;

  ExecutionPlan ignore_leaf(const BDD::Node_ptr &next, Target_ptr next_target) const;

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

  inline void set_infrastructure(std::shared_ptr<Clone::Infrastructure> _infrastructure) {
    infrastructure = _infrastructure;
  }

  inline const std::shared_ptr<Clone::Infrastructure>& get_infrastructure() const {
    return infrastructure;
  }

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