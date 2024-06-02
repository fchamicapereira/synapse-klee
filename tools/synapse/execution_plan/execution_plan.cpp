#include "execution_plan.h"
#include "node.h"
#include "visitor.h"
#include "../targets/targets.h"
#include "../log.h"

namespace synapse {

static ep_id_t counter = 0;

static std::unordered_set<TargetType>
get_target_types(const std::vector<const Target *> &targets) {
  assert(targets.size());
  std::unordered_set<TargetType> targets_types;

  for (const Target *target : targets) {
    targets_types.insert(target->type);
  }

  return targets_types;
}

static TargetType
get_initial_target(const std::vector<const Target *> &targets) {
  assert(targets.size());
  return targets[0]->type;
}

EP::EP(std::shared_ptr<const bdd::BDD> _bdd,
       const std::vector<const Target *> &_targets)
    : id(counter++), bdd(_bdd), root(nullptr),
      initial_target(get_initial_target(_targets)),
      targets(get_target_types(_targets)), ctx(_bdd.get(), _targets),
      meta(bdd.get()) {
  targets_roots[initial_target] = bdd::nodes_t({bdd->get_root()->get_id()});

  for (const Target *target : _targets) {
    meta.nodes_per_target[target->type] = 0;

    if (target->type != initial_target) {
      targets_roots[target->type] = bdd::nodes_t();
    }
  }

  leaves.emplace_back(nullptr, bdd->get_root());
}

static std::set<ep_id_t> update_ancestors(const EP &other) {
  std::set<ep_id_t> ancestors = other.get_ancestors();
  ancestors.insert(other.get_id());
  return ancestors;
}

EP::EP(const EP &other)
    : id(counter++), bdd(other.bdd),
      root(other.root ? other.root->clone(true) : nullptr),
      initial_target(other.initial_target), targets(other.targets),
      ancestors(update_ancestors(other)), targets_roots(other.targets_roots),
      ctx(other.ctx), meta(other.meta) {
  if (!root) {
    assert(other.leaves.size() == 1);
    leaves.emplace_back(nullptr, bdd->get_root());
    return;
  }

  for (const EPLeaf &leaf : other.leaves) {
    ep_node_id_t leaf_node_id = leaf.node->get_id();
    EPNode *leaf_node = root->get_mutable_node_by_id(leaf_node_id);
    assert(leaf_node && "Leaf node not found in the cloned tree.");
    leaves.emplace_back(leaf_node, leaf.next);
  }
}

EP::~EP() {
  if (root) {
    delete root;
    root = nullptr;
  }
}

const EPMeta &EP::get_meta() const { return meta; }

ep_id_t EP::get_id() const { return id; }

const EPNode *EP::get_root() const { return root; }

EPNode *EP::get_mutable_root() { return root; }

const std::vector<EPLeaf> &EP::get_leaves() const { return leaves; }

const bdd::nodes_t &EP::get_target_roots(TargetType target) const {
  assert(targets_roots.find(target) != targets_roots.end() &&
         "Target not found in the roots map.");
  return targets_roots.at(target);
}

const std::set<ep_id_t> &EP::get_ancestors() const { return ancestors; }

const bdd::BDD *EP::get_bdd() const { return bdd.get(); }

std::vector<const EPNode *> EP::get_prev_nodes() const {
  std::vector<const EPNode *> prev_nodes;

  const EPLeaf *current = get_active_leaf();
  const EPNode *node = current->node;

  while (node) {
    prev_nodes.push_back(node);
    node = node->get_prev();
  }

  return prev_nodes;
}

std::vector<const EPNode *> EP::get_prev_nodes_of_current_target() const {
  std::vector<const EPNode *> prev_nodes;

  TargetType target = get_current_platform();
  const EPLeaf *current = get_active_leaf();
  const EPNode *node = current->node;

  while (node) {
    const Module *module = node->get_module();

    if (module->get_target() != target) {
      break;
    }

    prev_nodes.push_back(node);
    node = node->get_prev();
  }

  return prev_nodes;
}

bool EP::has_target(TargetType type) const {
  return targets.find(type) != targets.end();
}

const Context &EP::get_ctx() const { return ctx; }

Context &EP::get_mutable_ctx() { return ctx; }

const bdd::Node *EP::get_next_node() const {
  const EPLeaf *active_leaf = get_active_leaf();

  if (!active_leaf) {
    return nullptr;
  }

  const bdd::Node *next_node = active_leaf->next;
  assert(!meta.is_processed_node(next_node) && "Next node already processed.");

  while (next_node && meta.is_processed_node(next_node)) {
    assert(next_node->get_type() != bdd::NodeType::BRANCH);
    next_node = next_node->get_next();
  }

  return next_node;
}

const EPLeaf *EP::get_active_leaf() const {
  return leaves.size() ? &leaves.at(0) : nullptr;
}

EPLeaf *EP::get_mutable_active_leaf() {
  return leaves.size() ? &leaves.at(0) : nullptr;
}

TargetType EP::get_current_platform() const {
  if (!root) {
    return initial_target;
  }

  const EPLeaf *active_leaf = get_active_leaf();
  assert(active_leaf && "No active leaf");

  const EPNode *node = active_leaf->node;
  const Module *module = node->get_module();

  return module->get_next_target();
}

void EP::process_leaf(const bdd::Node *next_node) {
  EPLeaf *active_leaf = get_mutable_active_leaf();
  assert(active_leaf && "No active leaf");

  if (next_node) {
    active_leaf->next = next_node;
  } else {
    leaves.erase(leaves.begin());
  }
}

void EP::process_leaf(EPNode *new_node, const std::vector<EPLeaf> &new_leaves,
                      bool process_node) {
  TargetType current_target = get_current_platform();

  EPLeaf *active_leaf = get_mutable_active_leaf();
  assert(active_leaf && "No active leaf");

  if (!root) {
    root = new_node;
  } else {
    active_leaf->node->set_children({new_node});
    new_node->set_prev(active_leaf->node);
  }

  meta.update(active_leaf, new_node, process_node);
  meta.depth++;

  for (const EPLeaf &new_leaf : new_leaves) {
    if (!new_leaf.next)
      continue;

    meta.update(active_leaf, new_leaf.node, process_node);

    const Module *module = new_leaf.node->get_module();
    TargetType next_target = module->get_next_target();
    bdd::node_id_t next_node_id = new_leaf.next->get_id();

    if (next_target != current_target) {
      targets_roots[next_target].insert(next_node_id);
    }

    if (new_leaf.node != new_node) {
      new_leaf.node->set_prev(new_node);
      new_node->add_child(new_leaf.node);
    }
  }

  leaves.erase(leaves.begin());
  for (const EPLeaf &new_leaf : new_leaves) {
    if (!new_leaf.next)
      continue;
    leaves.insert(leaves.begin(), new_leaf);
  }
}

void EP::replace_bdd(
    const bdd::BDD *new_bdd,
    const std::unordered_map<bdd::node_id_t, bdd::node_id_t> &leaves_mapping) {
  for (EPLeaf &leaf : leaves) {
    bdd::node_id_t old_id = leaf.next->get_id();
    auto found_it = leaves_mapping.find(old_id);
    bdd::node_id_t new_id =
        (found_it != leaves_mapping.end()) ? found_it->second : old_id;

    const bdd::Node *new_node = new_bdd->get_node_by_id(new_id);
    assert(new_node && "New node not found in the new BDD.");

    leaf.next = new_node;
  }

  if (!root) {
    return;
  }

  root->visit_mutable_nodes([&new_bdd](EPNode *node) {
    Module *module = node->get_mutable_module();

    const bdd::Node *node_bdd = module->get_node();
    bdd::node_id_t target_id = node_bdd->get_id();

    const bdd::Node *new_node = new_bdd->get_node_by_id(target_id);
    assert(new_node && "New node not found in the new BDD.");

    module->set_node(new_node);

    return EPNodeVisitAction::VISIT_CHILDREN;
  });

  // Reset the BDD only here, because we might lose the final reference to it
  // and we needed the old nodes to find the new ones.
  bdd.reset(new_bdd);
  meta.update_total_bdd_nodes(new_bdd);
}

void EP::visit(EPVisitor &visitor) const { visitor.visit(this); }

void EP::process_future_node(const bdd::Node *future) {
  meta.processed_nodes.insert(future->get_id());
}

} // namespace synapse