#include "execution_plan.h"
#include "execution_plan_node.h"
#include "visitor.h"
#include "../targets/targets.h"
#include "../log.h"

namespace synapse {

static ep_id_t counter = 0;

EP::EP(const std::shared_ptr<bdd::BDD> &_bdd,
       const std::vector<const Target *> &_targets)
    : id(counter++), bdd(_bdd), root(nullptr), ctx(_targets), meta(bdd.get()) {
  assert(_targets.size());
  initial_target = _targets[0]->type;

  for (const Target *target : _targets) {
    targets.insert(target->type);
    targets_roots[target->type] = bdd::nodes_t();
    meta.nodes_per_target[target->type] = 0;
  }

  leaves.emplace_back(nullptr, bdd->get_root());
}

EP::EP(const EP &other)
    : id(other.id), bdd(other.bdd),
      root(other.root ? other.root->clone(true) : nullptr),
      initial_target(other.initial_target), targets(other.targets),
      targets_roots(other.targets_roots), ctx(other.ctx), meta(other.meta) {
  if (!root) {
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

EP::EP(EP &&other)
    : id(other.id), bdd(std::move(other.bdd)), root(std::move(other.root)),
      leaves(std::move(other.leaves)), initial_target(other.initial_target),
      targets(std::move(other.targets)),
      targets_roots(std::move(other.targets_roots)), ctx(std::move(other.ctx)),
      meta(std::move(other.meta)) {}

EP &EP::operator=(const EP &other) {
  if (this == &other) {
    return *this;
  }

  if (root) {
    delete root;
    root = nullptr;
  }

  id = other.id;
  bdd = other.bdd;
  root = other.root ? other.root->clone(true) : nullptr;
  initial_target = other.initial_target;
  targets = other.targets;
  targets_roots = other.targets_roots;
  ctx = other.ctx;
  meta = other.meta;

  if (!root) {
    leaves.emplace_back(nullptr, bdd->get_root());
    return *this;
  }

  for (const EPLeaf &leaf : other.leaves) {
    ep_node_id_t leaf_node_id = leaf.node->get_id();
    EPNode *leaf_node = root->get_mutable_node_by_id(leaf_node_id);
    assert(leaf_node && "Leaf node not found in the cloned tree.");
    leaves.emplace_back(leaf_node, leaf.next);
  }

  return *this;
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

const Context &EP::get_context() const { return ctx; }

const bdd::Node *EP::get_next_node() const {
  const EPLeaf *leaf = get_active_leaf();
  return leaf ? leaf->next : nullptr;
}

const EPLeaf *EP::get_active_leaf() const {
  return leaves.size() ? &leaves.at(0) : nullptr;
}

EPLeaf *EP::get_mutable_active_leaf() {
  return leaves.size() ? &leaves.at(0) : nullptr;
}

TargetType EP::get_current_platform() const {
  if (!root)
    return initial_target;

  const EPLeaf &leaf = leaves[0];
  const Module *module = leaf.node->get_module();
  return module->get_next_target();
}

void EP::process_leaf(const std::vector<EPLeaf> &new_leaves) {
  TargetType current_target = get_current_platform();
  EPLeaf *active_leaf = get_mutable_active_leaf();
  assert(active_leaf && "No active leaf");

  meta.processed_nodes.insert(active_leaf->next->get_id());

  if (!root) {
    assert(leaves.size() == 1);
    assert(new_leaves.size() == 1);

    const EPLeaf &new_leaf = new_leaves[0];
    const Module *module = new_leaf.node->get_module();
    TargetType target = module->get_target();

    root = new_leaf.node;
    meta.nodes++;
    meta.nodes_per_target[target]++;
  } else {
    assert(leaves.size());

    std::vector<EPNode *> children;
    for (const EPLeaf &new_leaf : new_leaves) {
      children.push_back(new_leaf.node);
      new_leaf.node->set_prev(active_leaf->node);

      const Module *module = new_leaf.node->get_module();
      TargetType target = module->get_target();
      TargetType next_target = module->get_next_target();
      bdd::node_id_t next_node_id = new_leaf.next->get_id();

      meta.nodes++;
      meta.nodes_per_target[target]++;

      if (next_target != current_target) {
        targets_roots[next_target].insert(next_node_id);
      }
    }

    active_leaf->node->set_children(children);
  }

  meta.depth++;

  leaves.erase(leaves.begin());
  for (const EPLeaf &leaf : new_leaves) {
    if (!leaf.next)
      continue;
    leaves.insert(leaves.begin(), leaf);
  }
}

void EP::replace_bdd(
    std::unique_ptr<bdd::BDD> &&new_bdd,
    const std::unordered_map<bdd::node_id_t, bdd::node_id_t> &leaves_mapping) {
  bdd = std::move(new_bdd);

  for (EPLeaf &leaf : leaves) {
    auto found_it = leaves_mapping.find(leaf.next->get_id());
    assert(found_it != leaves_mapping.end() && "Leaf not found in mapping.");

    const bdd::Node *new_node = bdd->get_node_by_id(found_it->second);
    assert(new_node && "New node not found in the new BDD.");
    leaf.next = new_node;
  }
}

void EP::visit(EPVisitor &visitor) const { visitor.visit(*this); }

bool operator==(const EP &lhs, const EP &rhs) {
  if ((lhs.get_root() == nullptr && rhs.get_root() != nullptr) ||
      (lhs.get_root() != nullptr && rhs.get_root() == nullptr)) {
    return false;
  }

  const std::vector<EPLeaf> &lhs_leaves = lhs.get_leaves();
  const std::vector<EPLeaf> &rhs_leaves = rhs.get_leaves();

  if (lhs_leaves.size() != rhs_leaves.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs_leaves.size(); i++) {
    const EPLeaf &lhs_leaf = lhs_leaves[i];
    const EPLeaf &rhs_leaf = rhs_leaves[i];

    const Module *lhs_module = lhs_leaf.node->get_module();
    const Module *rhs_module = rhs_leaf.node->get_module();

    TargetType lhs_next_target = lhs_module->get_next_target();
    TargetType rhs_next_target = rhs_module->get_next_target();

    if (lhs_next_target != rhs_next_target) {
      return false;
    }

    bdd::node_id_t lhs_next_id = lhs_leaf.next->get_id();
    bdd::node_id_t rhs_next_id = rhs_leaf.next->get_id();

    if (lhs_next_id != rhs_next_id) {
      return false;
    }
  }

  const EPNode *lhs_root = lhs.get_root();
  const EPNode *rhs_root = rhs.get_root();

  if ((lhs_root == nullptr) != (rhs_root == nullptr)) {
    return false;
  }

  std::vector<const EPNode *> lhs_nodes;
  std::vector<const EPNode *> rhs_nodes;

  if (lhs_root) {
    lhs_nodes.push_back(lhs_root);
    rhs_nodes.push_back(rhs_root);
  }

  while (lhs_nodes.size()) {
    const EPNode *lhs_node = lhs_nodes[0];
    const EPNode *rhs_node = rhs_nodes[0];

    assert(lhs_node);
    assert(rhs_node);

    lhs_nodes.erase(lhs_nodes.begin());
    rhs_nodes.erase(rhs_nodes.begin());

    const Module *lhs_module = lhs_node->get_module();
    const Module *rhs_module = rhs_node->get_module();

    assert(lhs_module);
    assert(rhs_module);

    if (!lhs_module->equals(rhs_module)) {
      return false;
    }

    const std::vector<EPNode *> &lhs_branches = lhs_node->get_children();
    const std::vector<EPNode *> &rhs_branches = rhs_node->get_children();

    if (lhs_branches.size() != rhs_branches.size()) {
      return false;
    }

    lhs_nodes.insert(lhs_nodes.end(), lhs_branches.begin(), lhs_branches.end());
    rhs_nodes.insert(rhs_nodes.end(), rhs_branches.begin(), rhs_branches.end());
  }

  // Quick comparison of bdds
  const bdd::BDD *lhs_bdd = lhs.get_bdd();
  const bdd::BDD *rhs_bdd = rhs.get_bdd();

  std::string lhs_bdd_hash = lhs_bdd->hash();
  std::string rhs_bdd_hash = rhs_bdd->hash();

  if (lhs_bdd_hash != rhs_bdd_hash) {
    return false;
  }

  return true;
}

} // namespace synapse