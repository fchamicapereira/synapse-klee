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
       const std::vector<const Target *> &_targets,
       std::shared_ptr<Profiler> _profiler)
    : id(counter++), bdd(_bdd), root(nullptr),
      initial_target(get_initial_target(_targets)), targets(_targets),
      profiler(_profiler), ctx(_bdd.get(), _targets, initial_target),
      meta(bdd.get(), get_target_types(targets), initial_target) {
  targets_roots[initial_target] = bdd::nodes_t({bdd->get_root()->get_id()});

  for (const Target *target : _targets) {
    if (target->type != initial_target) {
      targets_roots[target->type] = bdd::nodes_t();
    }
  }

  leaves.emplace_back(nullptr, bdd->get_root());
}

static std::set<ep_id_t> update_ancestors(const EP &other, bool is_ancestor) {
  std::set<ep_id_t> ancestors = other.get_ancestors();

  if (is_ancestor) {
    ancestors.insert(other.get_id());
  }

  return ancestors;
}

EP::EP(const EP &other, bool is_ancestor)
    : id(counter++), bdd(other.bdd),
      root(other.root ? other.root->clone(true) : nullptr),
      initial_target(other.initial_target), targets(other.targets),
      ancestors(update_ancestors(other, is_ancestor)),
      targets_roots(other.targets_roots), profiler(other.profiler),
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

const std::vector<const Target *> &EP::get_targets() const { return targets; }

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
  auto found_it = std::find_if(
      targets.begin(), targets.end(),
      [type](const Target *target) { return target->type == type; });

  return found_it != targets.end();
}

const Profiler *EP::get_profiler() const { return profiler.get(); }

const Context &EP::get_ctx() const { return ctx; }

Context &EP::get_mutable_ctx() { return ctx; }

const bdd::Node *EP::get_next_node() const {
  const EPLeaf *active_leaf = get_active_leaf();

  if (!active_leaf) {
    return nullptr;
  }

  return active_leaf->next;
}

const EPLeaf *EP::get_active_leaf() const {
  return leaves.size() ? &leaves.at(0) : nullptr;
}

bool EP::has_active_leaf() const { return !leaves.empty(); }

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

  TargetType current_target = get_current_platform();

  if (next_node) {
    meta.process_node(active_leaf->next, current_target);
    active_leaf->next = next_node;
  } else {
    leaves.erase(leaves.begin());
  }

  ctx.update_throughput_estimates(this);
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

  meta.update(active_leaf, new_node, profiler.get(), process_node);
  ctx.update_traffic_fractions(new_node, profiler.get());

  meta.depth++;

  for (const EPLeaf &new_leaf : new_leaves) {
    if (!new_leaf.next)
      continue;

    meta.update(active_leaf, new_leaf.node, profiler.get(), process_node);
    ctx.update_traffic_fractions(new_node, profiler.get());

    const Module *module = new_leaf.node->get_module();
    TargetType next_target = module->get_next_target();
    bdd::node_id_t next_node_id = new_leaf.next->get_id();

    if (next_target != current_target) {
      targets_roots[next_target].insert(next_node_id);
    }
  }

  leaves.erase(leaves.begin());
  for (const EPLeaf &new_leaf : new_leaves) {
    if (!new_leaf.next)
      continue;

    const Module *module = new_leaf.node->get_module();
    TargetType next_target = module->get_next_target();

    // Prioritize leaves that don't change the current target.
    if (next_target != current_target) {
      leaves.push_back(new_leaf);
    } else {
      leaves.insert(leaves.begin(), new_leaf);
    }
  }

  ctx.update_throughput_estimates(this);
}

void EP::replace_bdd(const bdd::BDD *new_bdd,
                     const translator_t &next_nodes_translator,
                     const translator_t &processed_nodes_translator) {
  auto translate_next_node = [&next_nodes_translator](bdd::node_id_t id) {
    auto found_it = next_nodes_translator.find(id);
    return (found_it != next_nodes_translator.end()) ? found_it->second : id;
  };

  auto translate_processed_node =
      [&processed_nodes_translator](bdd::node_id_t id) {
        auto found_it = processed_nodes_translator.find(id);
        return (found_it != processed_nodes_translator.end()) ? found_it->second
                                                              : id;
      };

  for (EPLeaf &leaf : leaves) {
    assert(leaf.next);

    bdd::node_id_t new_id = translate_next_node(leaf.next->get_id());
    const bdd::Node *new_node = new_bdd->get_node_by_id(new_id);
    assert(new_node && "New node not found in the new BDD.");

    leaf.next = new_node;
  }

  if (!root) {
    return;
  }

  root->visit_mutable_nodes([&new_bdd, translate_processed_node](EPNode *node) {
    Module *module = node->get_mutable_module();

    const bdd::Node *node_bdd = module->get_node();
    bdd::node_id_t target_id = translate_processed_node(node_bdd->get_id());

    const bdd::Node *new_node = new_bdd->get_node_by_id(target_id);
    assert(new_node && "New node not found in the new BDD.");

    module->set_node(new_node);

    return EPNodeVisitAction::VISIT_CHILDREN;
  });

  // Reset the BDD only here, because we might lose the final reference to it
  // and we needed the old nodes to find the new ones.
  bdd.reset(new_bdd);
  meta.update_total_bdd_nodes(new_bdd);

  ctx.update_throughput_estimates(this);
}

void EP::visit(EPVisitor &visitor) const { visitor.visit(this); }

void EP::log_debug_placements() const {
  Log::dbg() << "Placements:\n";
  const std::unordered_map<addr_t, PlacementDecision> &placements =
      ctx.get_placements();
  for (const auto &[obj, decision] : placements) {
    Log::dbg() << "  " << obj << " -> " << decision << "\n";
  }
}

void EP::log_debug_hit_rate() const { profiler->log_debug(); }

void EP::inspect() const {
  std::vector<const EPNode *> nodes{root};

  while (nodes.size()) {
    const EPNode *node = nodes.back();
    nodes.pop_back();

    assert(node);
    assert(node->get_module());

    const Module *module = node->get_module();
    const bdd::Node *bdd_node = module->get_node();
    assert(bdd_node);

    const bdd::Node *found_bdd_node = bdd->get_node_by_id(bdd_node->get_id());
    assert(bdd_node == found_bdd_node);

    for (const EPNode *child : node->get_children()) {
      assert(child);
      assert(child->get_prev() == node);
      nodes.push_back(child);
    }
  }

  for (const auto &[target, roots] : targets_roots) {
    for (const bdd::node_id_t root_id : roots) {
      const bdd::Node *bdd_node = bdd->get_node_by_id(root_id);
      assert(bdd_node);

      const bdd::Node *found_bdd_node = bdd->get_node_by_id(bdd_node->get_id());
      assert(bdd_node == found_bdd_node);
    }
  }

  for (const EPLeaf &leaf : leaves) {
    const bdd::Node *next = leaf.next;
    assert(next);

    const bdd::Node *found_next = bdd->get_node_by_id(next->get_id());
    assert(next == found_next);
  }
}

constraints_t EP::get_active_leaf_constraints() const {
  const EPLeaf *active_leaf = get_active_leaf();
  assert(active_leaf && "No active leaf");

  const EPNode *node = active_leaf->node;

  if (!node) {
    return {};
  }

  return ctx.get_node_constraints(node);
}

float EP::get_active_leaf_hit_rate() const {
  constraints_t constraints = get_active_leaf_constraints();

  if (constraints.empty()) {
    return 1.0;
  }

  std::optional<float> fraction = profiler->get_fraction(constraints);
  assert(fraction.has_value());

  return *fraction;
}

void EP::add_hit_rate_estimation(klee::ref<klee::Expr> new_constraint,
                                 float estimation_rel) {
  Profiler *new_profiler = new Profiler(*profiler);
  profiler.reset(new_profiler);

  constraints_t constraints = get_active_leaf_constraints();

  Log::dbg() << "Adding hit rate estimation " << estimation_rel << "\n";
  for (const klee::ref<klee::Expr> &constraint : constraints) {
    Log::dbg() << "   " << kutil::expr_to_string(constraint, true) << "\n";
  }

  profiler->insert_relative(constraints, new_constraint, estimation_rel);

  Log::dbg() << "\n";
  Log::dbg() << "Resulting Hit rate:\n";
  profiler->log_debug();
  Log::dbg() << "\n";
}

void EP::remove_hit_rate_node(const constraints_t &constraints) {
  profiler->remove(constraints);
}

void EP::update_node_constraints(const EPNode *on_true_node,
                                 const EPNode *on_false_node,
                                 klee::ref<klee::Expr> new_constraint) {
  const EPLeaf *active_leaf = get_active_leaf();
  assert(active_leaf && "No active leaf");

  constraints_t constraints;

  const EPNode *active_node = active_leaf->node;

  if (active_node) {
    constraints = ctx.get_node_constraints(active_node);
  }

  klee::ref<klee::Expr> new_constraint_not =
      kutil::solver_toolbox.exprBuilder->Not(new_constraint);

  constraints_t on_true_constraints = constraints;
  constraints_t on_false_constraints = constraints;

  on_true_constraints.push_back(new_constraint);
  on_false_constraints.push_back(new_constraint_not);

  ep_node_id_t on_true_id = on_true_node->get_id();
  ep_node_id_t on_false_id = on_false_node->get_id();

  ctx.update_constraints_per_node(on_true_id, on_true_constraints);
  ctx.update_constraints_per_node(on_false_id, on_false_constraints);
}

uint64_t EP::estimate_throughput_pps() const {
  return ctx.get_throughput_estimate_pps();
}

uint64_t EP::speculate_throughput_pps() const {
  return ctx.get_throughput_speculation_pps();
}

} // namespace synapse