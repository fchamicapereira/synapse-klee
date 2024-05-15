#include "node.h"
#include "branch.h"
#include "call.h"
#include "manager.h"

#include "klee-util.h"

#include "llvm/Support/MD5.h"

#include <iomanip>

namespace bdd {

std::vector<node_id_t> Node::get_terminating_node_ids() const {
  if (!next)
    return std::vector<node_id_t>{id};
  return next->get_terminating_node_ids();
}

bool Node::is_reachable_by_node(node_id_t id) const {
  const Node *node = this;
  while (node) {
    if (node->get_id() == id)
      return true;
    node = node->get_prev();
  }
  return false;
}

size_t Node::count_children(bool recursive) const {
  std::vector<const Node *> children;
  const Node *node = this;

  if (node->get_type() == NodeType::BRANCH) {
    auto branch_node = static_cast<const Branch *>(node);

    children.push_back(branch_node->get_on_true());
    children.push_back(branch_node->get_on_false());
  } else if (node->get_next()) {
    children.push_back(node->get_next());
  }

  size_t n = children.size();

  while (recursive && children.size()) {
    node = children[0];
    children.erase(children.begin());

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();

      if (on_true) {
        children.push_back(on_true);
        n++;
      }
      if (on_false) {
        children.push_back(on_false);
        n++;
      }
    } break;
    case NodeType::ROUTE:
    case NodeType::CALL: {
      const Node *next = node->get_next();
      if (next) {
        children.push_back(next);
        n++;
      }
    } break;
    }
  }

  return n;
}

size_t Node::count_code_paths() const {
  std::vector<const Node *> nodes{this};
  size_t paths = 0;

  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();

      if (on_true) {
        nodes.push_back(on_true);
      } else {
        paths++;
      }

      if (on_false) {
        nodes.push_back(on_false);
      } else {
        paths++;
      }
    } break;
    case NodeType::ROUTE:
    case NodeType::CALL: {
      const Node *next = node->get_next();
      if (next) {
        nodes.push_back(next);
      } else {
        paths++;
      }
    } break;
    }
  }

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
      result << next->recursive_dump(lvl + 1);
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
    input << id << ":";
  }

  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    input << node->get_id() << ":";

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();
      if (on_true)
        nodes.push_back(on_true);
      if (on_false)
        nodes.push_back(on_false);
    } break;
    case NodeType::ROUTE:
    case NodeType::CALL: {
      const Node *next = node->get_next();
      if (next)
        nodes.push_back(next);
    } break;
    }
  }

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
  std::vector<const Node *> nodes{this};

  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    if (node->get_id() == _id)
      return node;

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();
      if (on_true)
        nodes.push_back(on_true);
      if (on_false)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL:
    case NodeType::ROUTE: {
      const Node *next = node->get_next();
      if (next)
        nodes.push_back(next);
    } break;
    }
  }

  return nullptr;
}

Node *Node::get_mutable_node_by_id(node_id_t _id) {
  std::vector<Node *> nodes{this};

  while (nodes.size()) {
    Node *node = nodes[0];
    nodes.erase(nodes.begin());

    if (node->get_id() == _id)
      return node;

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      Branch *branch_node = static_cast<Branch *>(node);
      Node *on_true = branch_node->get_mutable_on_true();
      Node *on_false = branch_node->get_mutable_on_false();
      if (on_true)
        nodes.push_back(on_true);
      if (on_false)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL:
    case NodeType::ROUTE: {
      Node *next = node->get_mutable_next();
      if (next)
        nodes.push_back(next);
    } break;
    }
  }

  return nullptr;
}

void Node::recursive_translate_symbol(const symbol_t &old_symbol,
                                      const symbol_t &new_symbol) {
  kutil::RenameSymbols renamer;
  renamer.add_translation(old_symbol.array->name, new_symbol.array->name);

  std::vector<Node *> nodes{this};
  while (nodes.size()) {
    Node *node = nodes[0];
    nodes.erase(nodes.begin());

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      Branch *branch_node = static_cast<Branch *>(node);

      klee::ref<klee::Expr> condition = branch_node->get_condition();
      klee::ref<klee::Expr> new_condition = renamer.rename(condition);
      branch_node->set_condition(new_condition);

      Node *on_true = branch_node->get_mutable_on_true();
      Node *on_false = branch_node->get_mutable_on_false();
      if (on_true)
        nodes.push_back(on_true);
      if (on_false)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL: {
      Call *call_node = static_cast<Call *>(node);

      const call_t &call = call_node->get_call();
      call_t new_call = renamer.rename(call);
      call_node->set_call(new_call);

      symbols_t generated_symbols = call_node->get_locally_generated_symbols();

      auto same_symbol = [&old_symbol](const symbol_t &s) {
        return s.base == old_symbol.base;
      };

      std::erase_if(generated_symbols, same_symbol);
      generated_symbols.insert(new_symbol);
      call_node->set_locally_generated_symbols(generated_symbols);

      Node *next = node->get_mutable_next();
      if (next)
        nodes.push_back(next);
    } break;
    case NodeType::ROUTE: {
      Node *next = node->get_mutable_next();
      if (next)
        nodes.push_back(next);
    } break;
    }
  }
}

} // namespace bdd