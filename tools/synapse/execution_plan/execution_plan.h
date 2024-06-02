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

typedef uint64_t ep_id_t;

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
  const std::unordered_set<TargetType> targets;
  const std::set<ep_id_t> ancestors;

  std::unordered_map<TargetType, bdd::nodes_t> targets_roots;

  Context ctx;
  EPMeta meta;

public:
  EP(std::shared_ptr<const bdd::BDD> bdd,
     const std::vector<const Target *> &targets);

  EP(const EP &other);

  ~EP();

  EP(EP &&other) = delete;
  EP &operator=(const EP *other) = delete;

  void process_future_node(const bdd::Node *future);
  void process_leaf(EPNode *new_node, const std::vector<EPLeaf> &new_leaves,
                    bool process_node = true);
  void process_leaf(const bdd::Node *next_node);

  void replace_bdd(
      const bdd::BDD *new_bdd,
      const std::unordered_map<bdd::node_id_t, bdd::node_id_t> &leaves_mapping =
          std::unordered_map<bdd::node_id_t, bdd::node_id_t>());

  ep_id_t get_id() const;
  const bdd::BDD *get_bdd() const;
  const EPNode *get_root() const;
  const std::vector<EPLeaf> &get_leaves() const;
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
  TargetType get_current_platform() const;
  EPNode *get_node_by_id(ep_node_id_t id) const;

  void visit(EPVisitor &visitor) const;

private:
  EPLeaf *get_mutable_active_leaf();
};

} // namespace synapse