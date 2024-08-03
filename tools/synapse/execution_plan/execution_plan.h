#pragma once

#include "call-paths-to-bdd.h"

#include "node.h"
#include "meta.h"
#include "../targets/target.h"
#include "../targets/context.h"

#include <unordered_map>
#include <unordered_set>
#include <set>

namespace synapse {

class EPVisitor;
class Profiler;

typedef uint64_t ep_id_t;
typedef std::unordered_map<bdd::node_id_t, bdd::node_id_t> translator_t;

struct EPLeaf {
  EPNode *node;
  const bdd::Node *next;

  EPLeaf(EPNode *_node, const bdd::Node *_next) : node(_node), next(_next) {}
  EPLeaf(const EPLeaf &other) : node(other.node), next(other.next) {}
};

class EP {
private:
  ep_id_t id;
  std::shared_ptr<const bdd::BDD> bdd;

  EPNode *root;
  std::vector<EPLeaf> leaves;

  const TargetType initial_target;
  const targets_t targets;
  const std::set<ep_id_t> ancestors;

  std::unordered_map<TargetType, bdd::nodes_t> targets_roots;

  Context ctx;
  EPMeta meta;

public:
  EP(std::shared_ptr<const bdd::BDD> bdd, const targets_t &targets,
     std::shared_ptr<Profiler> profiler);

  EP(const EP &other, bool is_ancestor = true);

  ~EP();

  EP(EP &&other) = delete;
  EP &operator=(const EP *other) = delete;

  void process_leaf(EPNode *new_node, const std::vector<EPLeaf> &new_leaves,
                    bool process_node = true);
  void process_leaf(const bdd::Node *next_node);

  void
  replace_bdd(const bdd::BDD *new_bdd,
              const translator_t &next_nodes_translator = translator_t(),
              const translator_t &processed_nodes_translator = translator_t());

  ep_id_t get_id() const;
  const bdd::BDD *get_bdd() const;
  const EPNode *get_root() const;
  const std::vector<EPLeaf> &get_leaves() const;
  const targets_t &get_targets() const;
  const bdd::nodes_t &get_target_roots(TargetType target) const;
  const std::set<ep_id_t> &get_ancestors() const;
  const Context &get_ctx() const;
  const EPMeta &get_meta() const;

  EPNode *get_mutable_root();
  Context &get_mutable_ctx();
  EPNode *get_mutable_node_by_id(ep_node_id_t id);

  std::vector<const EPNode *> get_prev_nodes() const;
  std::vector<const EPNode *> get_prev_nodes_of_current_target() const;

  bool has_target(TargetType type) const;
  const bdd::Node *get_next_node() const;
  const EPLeaf *get_active_leaf() const;
  bool has_active_leaf() const;
  TargetType get_current_platform() const;
  EPNode *get_node_by_id(ep_node_id_t id) const;

  // TODO: Improve the performance of this method.
  // Currently it goes through all the leaf's parents, extracts the generated
  // conditions, and traverses the hit rate tree (using the solver).
  double get_active_leaf_hit_rate() const;

  // Estimation is relative to the parent node.
  // E.g. if the parent node has a hit rate of 0.5, and the estimation_rel is
  // 0.1, the hit rate of the current node will be 0.05.
  // WARNING: this should be called before processing the leaf.
  void add_hit_rate_estimation(klee::ref<klee::Expr> condition,
                               double estimation_rel);

  // WARNING: this should be called before processing the leaf.
  void update_node_constraints(const EPNode *on_true_node,
                               const EPNode *on_false_node,
                               klee::ref<klee::Expr> new_constraint);

  void remove_hit_rate_node(const constraints_t &constraints);

  uint64_t estimate_throughput_pps() const;
  uint64_t speculate_throughput_pps() const;

  void visit(EPVisitor &visitor) const;

  void log_debug_placements() const;
  void log_debug_hit_rate() const;
  void inspect() const;

private:
  EPLeaf *get_mutable_active_leaf();
  constraints_t get_active_leaf_constraints() const;
};

} // namespace synapse