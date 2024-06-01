#include "node.h"
#include "visitor.h"
#include "../targets/targets.h"
#include "../targets/module.h"
#include "../log.h"

namespace synapse {

static ep_node_id_t counter = 0;

EPNode::EPNode(const Module *_module)
    : id(counter++), module(_module), prev(nullptr) {}

EPNode::~EPNode() {
  if (module) {
    delete module;
    module = nullptr;
  }

  for (EPNode *child : children) {
    if (child) {
      delete child;
      child = nullptr;
    }
  }
}

void EPNode::set_children(const std::vector<EPNode *> &_children) {
  children = _children;
}

void EPNode::add_child(EPNode *child) { children.push_back(child); }

void EPNode::set_prev(EPNode *_prev) { prev = _prev; }

const Module *EPNode::get_module() const { return module; }

const std::vector<EPNode *> &EPNode::get_children() const { return children; }
EPNode *EPNode::get_prev() const { return prev; }

ep_node_id_t EPNode::get_id() const { return id; }
void EPNode::set_id(ep_node_id_t _id) { id = _id; }

const EPNode *EPNode::get_node_by_id(ep_node_id_t target_id) const {
  const EPNode *target = nullptr;

  visit_nodes([target_id, &target](const EPNode *node) {
    if (node->get_id() == target_id) {
      target = node;
      return EPNodeVisitAction::STOP;
    }
    return EPNodeVisitAction::VISIT_CHILDREN;
  });

  return target;
}

EPNode *EPNode::get_mutable_node_by_id(ep_node_id_t target_id) {
  EPNode *target = nullptr;

  visit_mutable_nodes([target_id, &target](EPNode *node) {
    if (node->get_id() == target_id) {
      target = node;
      return EPNodeVisitAction::STOP;
    }
    return EPNodeVisitAction::VISIT_CHILDREN;
  });

  return target;
}

bool EPNode::is_terminal_node() const { return children.size() == 0; }

EPNode *EPNode::clone(bool recursive) const {
  Module *cloned_module = module->clone();
  EPNode *cloned_node = new EPNode(cloned_module);

  // The constructor increments the ID, let's fix that
  cloned_node->set_id(id);

  if (recursive) {
    std::vector<EPNode *> children_clones;

    for (const EPNode *child : children) {
      EPNode *cloned_children = child->clone(true);
      cloned_children->set_prev(cloned_node);
      children_clones.push_back(cloned_children);
    }

    cloned_node->set_children(children_clones);
  }

  return cloned_node;
}

void EPNode::visit_nodes(
    std::function<EPNodeVisitAction(const EPNode *)> fn) const {
  std::vector<const EPNode *> nodes{this};
  while (nodes.size()) {
    const EPNode *node = nodes[0];
    nodes.erase(nodes.begin());

    EPNodeVisitAction action = fn(node);

    if (action == EPNodeVisitAction::STOP)
      return;

    if (action == EPNodeVisitAction::SKIP_CHILDREN)
      continue;

    const std::vector<EPNode *> &children = node->get_children();
    nodes.insert(nodes.end(), children.begin(), children.end());
  }
}

void EPNode::visit_mutable_nodes(
    std::function<EPNodeVisitAction(EPNode *)> fn) {
  std::vector<EPNode *> nodes{this};
  while (nodes.size()) {
    EPNode *node = nodes[0];
    nodes.erase(nodes.begin());

    EPNodeVisitAction action = fn(node);

    if (action == EPNodeVisitAction::STOP)
      return;

    if (action == EPNodeVisitAction::SKIP_CHILDREN)
      continue;

    const std::vector<EPNode *> &children = node->get_children();
    nodes.insert(nodes.end(), children.begin(), children.end());
  }
}

void EPNode::visit(EPVisitor &visitor, const EP *ep) const {
  visitor.visit(ep, this);
}

} // namespace synapse