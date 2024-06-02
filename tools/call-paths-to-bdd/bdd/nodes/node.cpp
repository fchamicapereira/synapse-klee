#include "node.h"
#include "branch.h"
#include "call.h"
#include "manager.h"

#include "klee-util.h"

#include "llvm/Support/MD5.h"

#include <iomanip>

namespace bdd {

std::vector<node_id_t> Node::get_leaves() const {
  if (!next)
    return std::vector<node_id_t>{id};
  return next->get_leaves();
}

bool Node::is_reachable(node_id_t id) const {
  const Node *node = this;
  while (node) {
    if (node->get_id() == id)
      return true;
    node = node->get_prev();
  }
  return false;
}

size_t Node::count_children(bool recursive) const {
  NodeVisitAction action = recursive ? NodeVisitAction::VISIT_CHILDREN
                                     : NodeVisitAction::SKIP_CHILDREN;
  const Node *self = this;

  size_t total = 0;
  visit_nodes([&total, action, self](const Node *node) -> NodeVisitAction {
    if (node == self) {
      return NodeVisitAction::VISIT_CHILDREN;
    }

    total++;
    return action;
  });

  return total;
}

size_t Node::count_code_paths() const {
  size_t paths = 0;

  visit_nodes([&paths](const Node *node) -> NodeVisitAction {
    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();
      if (!on_true)
        paths++;
      if (!on_false)
        paths++;
    } break;
    case NodeType::CALL:
    case NodeType::ROUTE: {
      const Node *next = node->get_next();
      if (!next)
        paths++;
    } break;
    }
    return NodeVisitAction::VISIT_CHILDREN;
  });

  return paths;
}

std::string Node::recursive_dump(int lvl) const {
  std::stringstream result;
  std::string pad(lvl * 2, ' ');

  result << pad << dump(true) << "\n";

  switch (type) {
  case NodeType::BRANCH: {
    const Branch *branch_node = static_cast<const Branch *>(this);
    const Node *on_true = branch_node->get_on_true();
    const Node *on_false = branch_node->get_on_false();
    if (on_true)
      result << on_true->recursive_dump(lvl + 1);
    if (on_false)
      result << on_false->recursive_dump(lvl + 1);
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE: {
    if (next)
      result << next->recursive_dump(lvl);
  } break;
  }

  return result.str();
}

std::string Node::hash(bool recursive) const {
  std::stringstream input;
  std::stringstream output;
  std::vector<const Node *> nodes;

  if (recursive) {
    nodes.push_back(this);
  } else {
    input << id;
  }

  visit_nodes([&input](const Node *node) -> NodeVisitAction {
    input << ":" << node->get_id();
    return NodeVisitAction::VISIT_CHILDREN;
  });

  llvm::MD5 checksum;
  checksum.update(input.str());

  llvm::MD5::MD5Result result;
  checksum.final(result);

  output << std::hex << std::setfill('0');

  for (uint8_t byte : result) {
    output << std::hex << std::setw(2) << static_cast<int>(byte);
  }

  return output.str();
}

void Node::recursive_update_ids(node_id_t &new_id) {
  id = new_id++;
  switch (type) {
  case NodeType::BRANCH: {
    Branch *branch_node = static_cast<Branch *>(this);
    Node *on_true = branch_node->get_mutable_on_true();
    Node *on_false = branch_node->get_mutable_on_false();
    if (on_true)
      on_true->recursive_update_ids(new_id);
    if (on_false)
      on_false->recursive_update_ids(new_id);
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE: {
    if (next)
      next->recursive_update_ids(new_id);
  } break;
  }
}

const Node *Node::get_node_by_id(node_id_t _id) const {
  const Node *target = nullptr;

  visit_nodes([_id, &target](const Node *node) -> NodeVisitAction {
    if (node->get_id() == _id) {
      target = node;
      return NodeVisitAction::STOP;
    }
    return NodeVisitAction::VISIT_CHILDREN;
  });

  return target;
}

Node *Node::get_mutable_node_by_id(node_id_t _id) {
  Node *target = nullptr;
  visit_mutable_nodes([_id, &target](Node *node) -> NodeVisitAction {
    if (node->get_id() == _id) {
      target = node;
      return NodeVisitAction::STOP;
    }
    return NodeVisitAction::VISIT_CHILDREN;
  });
  return target;
}

void Node::recursive_translate_symbol(const symbol_t &old_symbol,
                                      const symbol_t &new_symbol) {
  kutil::RenameSymbols renamer;
  renamer.add_translation(old_symbol.array->name, new_symbol.array->name);

  visit_mutable_nodes([&renamer, old_symbol,
                       new_symbol](Node *node) -> NodeVisitAction {
    if (node->get_type() != NodeType::CALL)
      return NodeVisitAction::VISIT_CHILDREN;

    Call *call_node = static_cast<Call *>(node);

    const call_t &call = call_node->get_call();
    call_t new_call = renamer.rename(call);
    call_node->set_call(new_call);

    symbols_t generated_symbols = call_node->get_locally_generated_symbols();

    auto same_symbol = [old_symbol](const symbol_t &s) {
      return s.base == old_symbol.base;
    };

    size_t removed = std::erase_if(generated_symbols, same_symbol);
    if (removed > 0) {
      generated_symbols.insert(new_symbol);
      call_node->set_locally_generated_symbols(generated_symbols);
    }

    return NodeVisitAction::VISIT_CHILDREN;
  });
}

void Node::recursive_add_constraint(klee::ref<klee::Expr> constraint) {
  visit_mutable_nodes([constraint](Node *node) -> NodeVisitAction {
    node->constraints.addConstraint(constraint);
    return NodeVisitAction::VISIT_CHILDREN;
  });
}

void Node::visit_nodes(std::function<NodeVisitAction(const Node *)> fn) const {
  std::vector<const Node *> nodes{this};
  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    NodeVisitAction action = fn(node);

    if (action == NodeVisitAction::STOP)
      return;

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();
      if (on_true && action != NodeVisitAction::SKIP_CHILDREN)
        nodes.push_back(on_true);
      if (on_false && action != NodeVisitAction::SKIP_CHILDREN)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL:
    case NodeType::ROUTE: {
      const Node *next = node->get_next();
      if (next && action != NodeVisitAction::SKIP_CHILDREN)
        nodes.push_back(next);
    } break;
    }
  }
}

void Node::visit_mutable_nodes(std::function<NodeVisitAction(Node *)> fn) {
  std::vector<Node *> nodes{this};
  while (nodes.size()) {
    Node *node = nodes[0];
    nodes.erase(nodes.begin());

    NodeVisitAction action = fn(node);

    if (action == NodeVisitAction::STOP)
      return;

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      Branch *branch_node = static_cast<Branch *>(node);
      Node *on_true = branch_node->get_mutable_on_true();
      Node *on_false = branch_node->get_mutable_on_false();
      if (on_true && action != NodeVisitAction::SKIP_CHILDREN)
        nodes.push_back(on_true);
      if (on_false && action != NodeVisitAction::SKIP_CHILDREN)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL:
    case NodeType::ROUTE: {
      Node *next = node->get_mutable_next();
      if (next && action != NodeVisitAction::SKIP_CHILDREN)
        nodes.push_back(next);
    } break;
    }
  }
}

} // namespace bdd