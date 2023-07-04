#include "execution_plan.h"
#include "../log.h"
#include "bdd/bdd.h"
#include "bdd/nodes/node.h"
#include "bdd/nodes/return_process.h"
#include "execution_plan_node.h"
#include "memory_bank.h"
#include "modules/bmv2/if.h"
#include "modules/clone/memory_bank.h"
#include "modules/modules.h"
#include "parser/infrastructure.h"
#include "target.h"
#include "visitors/graphviz/graphviz.h"
#include "visitors/visitor.h"

namespace synapse {

ep_id_t ExecutionPlan::counter = 0;

// goto constructors

ExecutionPlan::leaf_t::leaf_t(BDD::Node_ptr _next) : next(_next) {
  current_platform.first = false;
  next_target = nullptr;
}

ExecutionPlan::leaf_t::leaf_t(Module_ptr _module, BDD::Node_ptr _next, Target_ptr target)
    : leaf_t(_module, _next, target, target) {}

ExecutionPlan::leaf_t::leaf_t(Module_ptr _module, BDD::Node_ptr _next, Target_ptr target, Target_ptr next_target)
    : leaf(ExecutionPlanNode::build(_module, target)), next(_next) {
  assert(_module);

  current_platform.first = true;
  current_platform.second = _module->get_next_target_type();
  this->next_target = next_target;
}

ExecutionPlan::leaf_t::leaf_t(const leaf_t &_leaf)
    : leaf(_leaf.leaf), next(_leaf.next), current_platform(_leaf.current_platform), next_target(_leaf.next_target) {}

ExecutionPlan::ExecutionPlan(const BDD::BDD &_bdd)
    : bdd(_bdd), shared_memory_bank(MemoryBank::build()), id(counter++) {
  assert(bdd.get_process());

  leaf_t leaf(bdd.get_process());
  leaves.push_back(leaf);
}

ExecutionPlan::ExecutionPlan(const ExecutionPlan &ep)
    : root(ep.root), leaves(ep.leaves), bdd(ep.bdd), bdds(ep.bdds),
      shared_memory_bank(ep.shared_memory_bank), targets(ep.targets), target_types(ep.target_types),
      processed_bdd_nodes(ep.processed_bdd_nodes), 
      infrastructure(ep.infrastructure), clone_target(ep.clone_target), clone_bdd_id(ep.clone_bdd_id),
      clone_leaves(ep.clone_leaves), depth(ep.depth), nodes(ep.nodes), 
      initial_target(ep.initial_target), current_target(ep.current_target),
      targets_bdd_starting_points(ep.targets_bdd_starting_points),
      reordered_nodes(ep.reordered_nodes), id(ep.id),
      meta(ep.meta) {}

ExecutionPlan::ExecutionPlan(const ExecutionPlan &ep,
                             ExecutionPlanNode_ptr _root)
    : root(_root), bdd(ep.bdd), bdds(ep.bdds), shared_memory_bank(ep.shared_memory_bank), targets(ep.targets), 
      target_types(ep.target_types), infrastructure(ep.infrastructure), clone_target(ep.clone_target), 
      clone_bdd_id(ep.clone_bdd_id), clone_leaves(ep.clone_leaves),
      depth(0), nodes(0), reordered_nodes(0), id(counter++) {
  if (!_root) {
    return;
  }

  Branches branches = {_root};

  while (branches.size()) {
    auto node = branches[0];
    branches.erase(branches.begin());

    meta.nodes++;

    auto next = node->get_next();
    branches.insert(branches.end(), next.begin(), next.end());
  }
}

unsigned ExecutionPlan::get_depth() const { return depth; }
unsigned ExecutionPlan::get_nodes() const { return nodes; }

const ep_meta_t &ExecutionPlan::get_meta() const { return meta; }

BDD::Node_ptr ExecutionPlan::get_bdd_root(BDD::Node_ptr node) const {
  assert(node);

  auto target = get_current_target();
  auto roots_per_target = meta.roots_per_target.at(target->id);

  auto is_root = [&](BDD::Node_ptr n) {
    auto found_it = roots_per_target.find(n->get_id());
    return found_it != roots_per_target.end();
  };

  while (!is_root(node) && node->get_prev()) {
    node = node->get_prev();
  }

  return node;
}

ep_id_t ExecutionPlan::get_id() const { return id; }

const std::vector<ExecutionPlan::leaf_t> &ExecutionPlan::get_leaves() const {
  return leaves;
}

const BDD::BDD &ExecutionPlan::get_bdd() const { return bdd; }
BDD::BDD &ExecutionPlan::get_bdd() { return bdd; }

void ExecutionPlan::inc_reordered_nodes() { meta.reordered_nodes++; }

const ExecutionPlanNode_ptr &ExecutionPlan::get_root() const { return root; }

std::vector<ExecutionPlanNode_ptr> ExecutionPlan::get_prev_nodes() const {
  std::vector<ExecutionPlanNode_ptr> prev_nodes;

  auto current = get_active_leaf();

  while (current) {
    auto prev = current->get_prev();
    prev_nodes.push_back(prev);
    current = prev;
  }

  return prev_nodes;
}

std::vector<ExecutionPlanNode_ptr>
ExecutionPlan::get_prev_nodes_of_current_target() const {
  std::vector<ExecutionPlanNode_ptr> prev_nodes;
  auto target = get_current_target();
  auto current = get_active_leaf();

  while (current) {
    auto m = current->get_module();
    assert(m);

    if (m->get_target() == target->type) {
      prev_nodes.push_back(current);
    }

    current = current->get_prev();
  }

  return prev_nodes;
}

Target_ptr ExecutionPlan::get_target(TargetType target_type, const std::string &instance) const {
  assert(target_types.find(target_type) != target_types.end());
  auto targets = target_types.at(target_type);

  if(instance == "" && targets.size() == 1) {
    return targets[0];
  }

  for(auto &target: targets) {
    if(target->instance && target->instance->name == instance) {
      return target;
    }
  }

  assert(false && "Shouldn't happen");
  return nullptr;
}

std::vector<BDD::Node_ptr> ExecutionPlan::get_incoming_bdd_nodes() const {
  std::vector<BDD::Node_ptr> incoming_nodes;
  std::vector<BDD::Node_ptr> nodes;

  auto node = get_next_node();

  if (node) {
    nodes.push_back(node);
  }

  while (nodes.size()) {
    auto node = nodes[0];
    nodes.erase(nodes.begin());

    assert(node);

    incoming_nodes.push_back(node);

    if (node->get_type() == BDD::Node::NodeType::CALL) {
      auto call_node = BDD_CAST_CALL(node);
      nodes.push_back(call_node->get_next());
    }

    else if (node->get_type() == BDD::Node::NodeType::BRANCH) {
      auto branch_node = BDD_CAST_BRANCH(node);
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
    }
  }

  return incoming_nodes;
}

void ExecutionPlan::add_target(Target_ptr target) {
  if(target_types.find(TargetType::CloNe) == target_types.end()) {
    assert(targets.find(target->id) == targets.end());
  }

  if (targets.size() == 0) {
    initial_target = target;
    current_target = target;
  }

  if(target->type == TargetType::CloNe) {
    this->clone_target = target;
  }

  targets[target->id] = target;
  target_types[target->type].push_back(target);
  meta.add_target(target);
}

const std::vector<Target_ptr>& ExecutionPlan::get_from_target_type(TargetType type) const {
  assert(target_types.find(type) != target_types.end());
  return target_types.at(type);
}

const std::map<target_id_t, Target_ptr>& ExecutionPlan::get_targets() const {
  return targets;
}

bool ExecutionPlan::has_target_type(TargetType type) const {
  return target_types.find(type) != target_types.end();
}

//
// goto CloNe
//
void ExecutionPlan::add_bdd(Target_ptr target, BDD_ptr _bdd) {
  assert(target_types.find(TargetType::CloNe) != target_types.end());
  bdds[target->id] = _bdd;
}

const std::map<target_id_t, BDD_ptr>& ExecutionPlan::get_target_bdds() const {
  assert(target_types.count(TargetType::CloNe));
  return bdds;
}

/* This function initializes the data structures that contain the origins in the BDD */
void ExecutionPlan::populate_origins() {
  auto initial_bdd = get_bdd();
  auto node = initial_bdd.get_process();
  assert(node->get_type() == BDD::Node::NodeType::BRANCH);

  auto infra = get_infrastructure();
  auto mb = get_memory_bank<targets::clone::CloneMemoryBank>(clone_target->id);

  // iterate through all meta branches and create starting points
  while(node->get_type() == BDD::Node::NodeType::BRANCH) {
    auto casted = BDD::cast_node<BDD::Branch>(node);
    unsigned value = extract_port(casted);
    auto device = infra->get_port(value)->get_device();

    auto target_type = string_to_target_type[device->get_type()];
    auto target = get_target(target_type, device->get_id());

    if(mb->get_origin_from_target(target) == nullptr) {
      BDD_ptr _bdd = BDD_ptr(new BDD::BDD(initial_bdd.get_id() + 1, initial_bdd.get_init(), node));
      add_bdd(target, _bdd);
      mb->add_origin(target, node);
    }
  
    mb->add_origin_nodes(target, node);
    node = casted->get_on_false();
  }
}

/* This function is used to replace the in the initial branches of a BDD with real ports */
void ExecutionPlan::concretize_ports() {
  auto mb = get_memory_bank<targets::clone::CloneMemoryBank>(clone_target->id);

  // replacing branch global ports with real ports
  for(auto& target_origin: mb->get_origins()) {
    auto target_id = target_origin.first;
    auto origin = target_origin.second;

    assert(origin->nodes.size());

    auto target = targets.at(target_id);
    assert(target);
    auto device = infrastructure->get_device(target->instance->name);

    auto node = origin->root;
    node->disconnect_prev();

    for(auto it = origin->nodes.begin(); it != origin->nodes.end(); ++it) {
      node = *it;
      unsigned real_port = infrastructure->get_local_port(node);
      Clone::Infrastructure::replace_branch_port(node, real_port);
    }

    // now node should be pointing to the last node, set its on_false to drop
    BDD::Branch* casted = static_cast<BDD::Branch*>(node.get());
    auto drop = Clone::Infrastructure::get_drop(node, clone_bdd_id++);
    casted->replace_on_false(drop);
    clone_leaves[target_id] = drop->get_id();
  }
}

void ExecutionPlan::setup_clone() {
  clone_bdd_id = get_bdd().get_id() + 5; // IDs are causing too many problems
  populate_origins();
  concretize_ports();
}

BDD::node_id_t ExecutionPlan::get_clone_leaf(target_id_t id) const {
  assert(clone_leaves.count(id));
  return clone_leaves.at(id);
}

Target_ptr ExecutionPlan::get_first_of_type(TargetType type) const {
  assert(has_target_type(type));
  assert(target_types.at(type).size() > 0);
  return target_types.at(type)[0];
}

MemoryBank_ptr ExecutionPlan::get_memory_bank() const {
  return shared_memory_bank;
}

void ExecutionPlan::update_roots(const std::vector<leaf_t> &new_leaves) {
  auto current_target = get_current_target();

  for (auto leaf : new_leaves) {
    if (!leaf.next) {
      continue;
    }

    assert(leaf.current_platform.first);
    auto next_target = leaf.next_target;

    if (current_target != next_target) {
      auto starting_point = leaf.next->get_id();
      meta.roots_per_target[next_target->id].insert(starting_point);
    }
  }
}

void ExecutionPlan::replace_roots(BDD::node_id_t _old, BDD::node_id_t _new) {
  auto current_target = get_current_target();
  auto found_it = meta.roots_per_target.find(current_target->id);

  if (found_it == meta.roots_per_target.end()) {
    return;
  }

  // Remove only if the _old node is found.

  auto found_node_it = found_it->second.find(_old);

  if (found_node_it == found_it->second.end()) {
    return;
  }

  found_it->second.erase(_old);
  found_it->second.insert(_new);
}

void ExecutionPlan::update_leaves(const std::vector<leaf_t> &_leaves,
                                  bool is_terminal) {
  update_roots(_leaves);

  assert(leaves.size());

  if (leaves.size()) {
    leaves.erase(leaves.begin());
  }

  for (auto leaf : _leaves) {
    if (!leaf.next && is_terminal) {
      continue;
    }

    leaves.insert(leaves.begin(), leaf);
  }
}

ExecutionPlanNode_ptr
ExecutionPlan::clone_nodes(ExecutionPlan &ep,
                           const ExecutionPlanNode *node) const {
  auto copy = ExecutionPlanNode::build(node);

  auto module = copy->get_module();
  assert(module);

  auto bdd_node = module->get_node();
  assert(bdd_node);

  // Different pointers!
  // We probably cloned the entire BDD in the past, we should update
  // this node to point to our new BDD.
  auto found_bdd_node = ep.bdd.get_node_by_id(bdd_node->get_id());
  if (found_bdd_node && found_bdd_node != bdd_node) {
    copy->replace_node(found_bdd_node);
  }

  auto old_next = node->get_next();
  Branches new_next;

  for (auto branch : old_next) {
    auto branch_copy = clone_nodes(ep, branch.get());
    new_next.push_back(branch_copy);
    branch_copy->set_prev(copy);
  }

  if (new_next.size()) {
    copy->set_next(new_next);
    return copy;
  }

  for (auto &leaf : ep.leaves) {
    if (leaf.leaf->get_id() == node->get_id()) {
      leaf.leaf = copy;
    }
  }

  return copy;
}

void ExecutionPlan::update_processed_nodes() {
  assert(leaves.size());
  auto processed_node = get_next_node();

  if (!processed_node) {
    return;
  }

  auto processed_node_id = processed_node->get_id();
  auto search = meta.processed_nodes.find(processed_node_id);
  assert(search == meta.processed_nodes.end());

  meta.processed_nodes.insert(processed_node_id);
}

BDD::Node_ptr ExecutionPlan::get_next_node() const {
  BDD::Node_ptr next;

  if (leaves.size()) {
    next = leaves[0].next;
  }

  return next;
}

ExecutionPlanNode_ptr ExecutionPlan::get_active_leaf() const {
  ExecutionPlanNode_ptr leaf;

  if (leaves.size()) {
    leaf = leaves[0].leaf;
  }

  return leaf;
}

Target_ptr ExecutionPlan::get_current_target() const {
  return current_target;
}

ExecutionPlan ExecutionPlan::replace_leaf(Module_ptr new_module,
                                          const BDD::Node_ptr &next,
                                          Target_ptr next_target,
                                          bool process_bdd_node) const {
  auto new_ep = clone();

  if(!next_target) {
    next_target = get_current_target();
  }

  if (process_bdd_node) {
    new_ep.update_processed_nodes();
  }

  auto new_leaf = ExecutionPlan::leaf_t(new_module, next, next_target);

  assert(new_ep.leaves.size());
  auto old_leaf = new_ep.leaves[0];

  if (!old_leaf.leaf->get_prev()) {
    new_ep.root = new_leaf.leaf;
  } else {
    auto prev = old_leaf.leaf->get_prev();
    prev->replace_next(old_leaf.leaf, new_leaf.leaf);
  }

  new_ep.leaves[0] = new_leaf;

  assert(old_leaf.leaf->get_module());
  assert(new_leaf.leaf->get_module());

  auto old_module = old_leaf.leaf->get_module();

  if (old_module->get_target() != new_module->get_target()) {
    new_ep.meta.nodes_per_target[old_module->get_target()]--;
    new_ep.meta.nodes_per_target[new_module->get_target()]++;
  }

  new_ep.leaves[0].current_platform.first = true;
  new_ep.leaves[0].current_platform.second = next_target->type;
  
  new_ep.current_target = next_target;

  return new_ep;
}

ExecutionPlan ExecutionPlan::ignore_leaf(const BDD::Node_ptr &next,
                                         Target_ptr next_target,
                                         bool process_bdd_node) const {
  auto new_ep = clone();

  if (!next_target) {
    next_target = get_current_target();
  }
  assert(new_ep.target_types.count(next_target->type));
  assert(new_ep.target_types[next_target->type].size() > 0);

  if (process_bdd_node) {
    new_ep.update_processed_nodes();
  }

  assert(new_ep.leaves.size());
  new_ep.leaves[0].next = next;

  new_ep.leaves[0].current_platform.first = true;
  new_ep.leaves[0].current_platform.second = next_target->type;

  new_ep.current_target = next_target;
  new_ep.meta.nodes_per_target[next_target->id]++;

  return new_ep;
}

void ExecutionPlan::force_termination() {
  assert(leaves.size());
  leaves.erase(leaves.begin());
}

ExecutionPlan ExecutionPlan::add_leaves(Module_ptr new_module,
                                        const BDD::Node_ptr &next,
                                        bool is_terminal,
                                        bool process_bdd_node, Target_ptr next_target) const {
  std::vector<ExecutionPlan::leaf_t> _leaves;

  if(!next_target) {
    next_target = get_current_target();
  }

  _leaves.emplace_back(new_module, next, get_current_target(), next_target);
  return add_leaves(_leaves, is_terminal, process_bdd_node);
}

ExecutionPlan ExecutionPlan::add_leaves(std::vector<leaf_t> _leaves,
                                        bool is_terminal,
                                        bool process_bdd_node) const {
  auto new_ep = clone();

  if (process_bdd_node) {
    new_ep.update_processed_nodes();
  }

  if (!new_ep.root) {
    assert(new_ep.leaves.size() == 1);
    assert(!new_ep.leaves[0].leaf);

    assert(_leaves.size() == 1);
    new_ep.root = _leaves[0].leaf;

    auto module = _leaves[0].leaf->get_module();
    auto target = _leaves[0].leaf->get_target();
    new_ep.meta.nodes_per_target[target->id]++;
  } else {
    assert(new_ep.root);
    assert(new_ep.leaves.size());

    Branches branches;

    for (auto leaf : _leaves) {
      branches.push_back(leaf.leaf);
      assert(!leaf.leaf->get_prev());

      leaf.leaf->set_prev(new_ep.leaves[0].leaf);
      new_ep.meta.nodes++;

      auto module = leaf.leaf->get_module();
      auto target = leaf.leaf->get_target();
      new_ep.meta.nodes_per_target[target->id]++;
    }

    new_ep.leaves[0].leaf->set_next(branches);
  }

  new_ep.meta.depth++;
  new_ep.update_leaves(_leaves, is_terminal);
  new_ep.current_target = new_ep.leaves[0].next_target;

  return new_ep;
}

void ExecutionPlan::replace_active_leaf_node(BDD::Node_ptr next,
                                             bool process_bdd_node) {
  if (process_bdd_node) {
    update_processed_nodes();
  }

  assert(leaves.size());
  leaves[0].next = next;
}

float ExecutionPlan::get_bdd_processing_progress() const {
  auto process = bdd.get_process();
  assert(process);
  auto total_nodes = process->count_children() + 1;
  return (float)meta.processed_nodes.size() / (float)total_nodes;
}

void ExecutionPlan::remove_from_processed_bdd_nodes(BDD::node_id_t id) {
  auto found_it = meta.processed_nodes.find(id);
  meta.processed_nodes.erase(found_it);
}

void ExecutionPlan::add_processed_bdd_node(BDD::node_id_t id) {
  auto found_it = meta.processed_nodes.find(id);
  if (found_it == meta.processed_nodes.end()) {
    meta.processed_nodes.insert(id);
  }

  for (auto &leaf : leaves) {
    assert(leaf.next);
    if (leaf.next->get_id() == id) {
      assert(leaf.next->get_next());
      assert(leaf.next->get_type() != BDD::Node::NodeType::BRANCH);
      leaf.next = leaf.next->get_next();
    }
  }
}

ExecutionPlan ExecutionPlan::clone(BDD::BDD new_bdd) const {
  ExecutionPlan copy = *this;

  copy.id = counter++;
  copy.bdd = new_bdd;
  copy.bdds = bdds;

  copy.shared_memory_bank = shared_memory_bank->clone();

  for (auto it = targets.begin(); it != targets.end(); it++) {
    copy.targets[it->first] = it->second->clone();
  }

  if (root) {
    copy.root = clone_nodes(copy, root.get());
  } else {
    for (auto leaf : copy.leaves) {
      assert(!leaf.leaf);
    }
  }

  for (auto &leaf : copy.leaves) {
    assert(leaf.next);
    auto new_next = copy.bdd.get_node_by_id(leaf.next->get_id());

    if (new_next) {
      leaf.next = new_next;
    }
  }

  return copy;
}

ExecutionPlan ExecutionPlan::clone(bool deep) const {
  ExecutionPlan copy = *this;

  copy.id = counter++;
  copy.bdds = bdds;

  if (deep) {
    copy.bdd = copy.bdd.clone();
  }

  copy.shared_memory_bank = shared_memory_bank->clone();
  for (auto ep_it = targets.begin(); ep_it != targets.end();
       ep_it++) {
    copy.targets[ep_it->first] = ep_it->second->clone();
  }

  if (root) {
    copy.root = clone_nodes(copy, root.get());
  } else {
    for (auto leaf : copy.leaves) {
      assert(!leaf.leaf);
    }
  }

  if (!deep) {
    return copy;
  }

  for (auto &leaf : copy.leaves) {
    assert(leaf.next);
    auto new_next = copy.bdd.get_node_by_id(leaf.next->get_id());

    if (new_next) {
      leaf.next = new_next;
    }
  }

  return copy;
}

void ExecutionPlan::visit(ExecutionPlanVisitor &visitor) const {
  visitor.visit(*this);
}

bool operator==(const ExecutionPlan &lhs, const ExecutionPlan &rhs) {
  if ((lhs.get_root() == nullptr && rhs.get_root() != nullptr) ||
      (lhs.get_root() != nullptr && rhs.get_root() == nullptr)) {
    return false;
  }

  auto lhs_leaves = lhs.get_leaves();
  auto rhs_leaves = rhs.get_leaves();

  if (lhs_leaves.size() != rhs_leaves.size()) {
    return false;
  }

  for (auto i = 0u; i < lhs_leaves.size(); i++) {
    auto lhs_leaf = lhs_leaves[i];
    auto rhs_leaf = rhs_leaves[i];

    if (lhs_leaf.current_platform != rhs_leaf.current_platform) {
      return false;
    }

    if (lhs_leaf.next->get_id() != rhs_leaf.next->get_id()) {
      return false;
    }
  }

  auto lhs_root = lhs.get_root();
  auto rhs_root = rhs.get_root();

  if ((lhs_root.get() == nullptr) != (rhs_root.get() == nullptr)) {
    return false;
  }

  auto lhs_nodes = std::vector<ExecutionPlanNode_ptr>{};
  auto rhs_nodes = std::vector<ExecutionPlanNode_ptr>{};

  if (lhs_root) {
    lhs_nodes.push_back(lhs_root);
    rhs_nodes.push_back(rhs_root);
  }

  while (lhs_nodes.size()) {
    auto lhs_node = lhs_nodes[0];
    auto rhs_node = rhs_nodes[0];

    assert(lhs_node);
    assert(rhs_node);

    lhs_nodes.erase(lhs_nodes.begin());
    rhs_nodes.erase(rhs_nodes.begin());

    auto lhs_module = lhs_node->get_module();
    auto rhs_module = rhs_node->get_module();

    assert(lhs_module);
    assert(rhs_module);

    if (!lhs_module->equals(rhs_module.get())) {
      return false;
    }

    auto lhs_branches = lhs_node->get_next();
    auto rhs_branches = rhs_node->get_next();

    if (lhs_branches.size() != rhs_branches.size()) {
      return false;
    }

    lhs_nodes.insert(lhs_nodes.end(), lhs_branches.begin(), lhs_branches.end());
    rhs_nodes.insert(rhs_nodes.end(), rhs_branches.begin(), rhs_branches.end());
  }

  // BDD comparisons but only by ID

  auto lhs_bdd = lhs.get_bdd();
  auto rhs_bdd = rhs.get_bdd();

  auto lhs_bdd_nodes = std::vector<BDD::Node_ptr>{lhs_bdd.get_process()};
  auto rhs_bdd_nodes = std::vector<BDD::Node_ptr>{rhs_bdd.get_process()};

  while (lhs_bdd_nodes.size()) {
    auto lhs_bdd_node = lhs_bdd_nodes[0];
    auto rhs_bdd_node = rhs_bdd_nodes[0];

    lhs_bdd_nodes.erase(lhs_bdd_nodes.begin());
    rhs_bdd_nodes.erase(rhs_bdd_nodes.begin());

    if (lhs_bdd_node->get_type() != rhs_bdd_node->get_type()) {
      return false;
    }

    if (lhs_bdd_node->get_id() != rhs_bdd_node->get_id()) {
      return false;
    }

    if (lhs_bdd_node->get_type() == BDD::Node::NodeType::BRANCH) {
      auto lhs_branch = static_cast<BDD::Branch *>(lhs_bdd_node.get());
      auto rhs_branch = static_cast<BDD::Branch *>(rhs_bdd_node.get());

      lhs_bdd_nodes.push_back(lhs_branch->get_on_true());
      rhs_bdd_nodes.push_back(rhs_branch->get_on_true());

      lhs_bdd_nodes.push_back(lhs_branch->get_on_false());
      rhs_bdd_nodes.push_back(rhs_branch->get_on_false());
    } else if (lhs_bdd_node->get_type() == BDD::Node::NodeType::CALL) {
      lhs_bdd_nodes.push_back(lhs_bdd_node->get_next());
      rhs_bdd_nodes.push_back(rhs_bdd_node->get_next());
    }
  }

  return true;
}

} // namespace synapse