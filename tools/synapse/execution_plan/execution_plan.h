#pragma once

#include "call-paths-to-bdd.h"

#include "node.h"
#include "meta.h"
#include "../targets/target.h"
#include "../targets/context.h"

#include <unordered_map>
#include <unordered_set>

namespace synapse {

class EPVisitor;

typedef uint64_t ep_id_t;

struct EPLeaf {
  EPNode *node;
  const bdd::Node *next;

  EPLeaf(EPNode *_node, const bdd::Node *_next) : node(_node), next(_next) {
    assert(next && "Next node is null.");
  }

  EPLeaf(const EPLeaf &other) : node(other.node), next(other.next) {
    assert(next && "Next node is null.");
  }
};

class EP {
private:
  ep_id_t id;
  std::shared_ptr<bdd::BDD> bdd;

  EPNode *root;
  std::vector<EPLeaf> leaves;

  TargetType initial_target;
  std::unordered_set<TargetType> targets;
  std::unordered_map<TargetType, bdd::nodes_t> targets_roots;

  Context ctx;
  EPMeta meta;

public:
  EP(const std::shared_ptr<bdd::BDD> &bdd,
     const std::vector<const Target *> &targets);

  EP(const EP &other);

  ~EP();

  EP(EP &&other) = delete;
  EP &operator=(const EP *other) = delete;

  void process_future_node(const bdd::Node *future);
  void process_leaf(EPNode *new_node, const std::vector<EPLeaf> &new_leaves);
  void process_leaf(const bdd::Node *next_node);

  void replace_bdd(
      std::unique_ptr<bdd::BDD> &&new_bdd,
      const std::unordered_map<bdd::node_id_t, bdd::node_id_t> &leaves_mapping);

  ep_id_t get_id() const;
  const bdd::BDD *get_bdd() const;
  const EPNode *get_root() const;
  const std::vector<EPLeaf> &get_leaves() const;
  const bdd::nodes_t &get_target_roots(TargetType target) const;
  const Context &get_context() const;
  const EPMeta &get_meta() const;

  EPNode *get_mutable_root();
  Context &get_mutable_context();
  EPNode *get_mutable_node_by_id(ep_node_id_t id);

  std::vector<const EPNode *> get_prev_nodes() const;
  std::vector<const EPNode *> get_prev_nodes_of_current_target() const;

  bool has_target(TargetType type) const;
  const bdd::Node *get_next_node() const;
  const EPLeaf *get_active_leaf() const;
  TargetType get_current_platform() const;
  EPNode *get_node_by_id(ep_node_id_t id) const;

  void visit(EPVisitor &visitor) const;

  EP clone(const bdd::BDD &new_bdd) const;
  EP clone(bool deep = false) const;

private:
  EPLeaf *get_mutable_active_leaf();
};

} // namespace synapse