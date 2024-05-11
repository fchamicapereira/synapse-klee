#include "node.h"
#include "branch.h"
#include "call.h"
#include "manager.h"

#include "llvm/Support/MD5.h"

#include <iomanip>

namespace bdd {

// Get generated symbols, but no further than this node
symbols_t Node::get_generated_symbols(
    const std::unordered_set<node_id_t> &furthest_back_nodes) const {
  symbols_t symbols;
  const Node *node = this;

  while (node) {
    auto found_it = std::find(furthest_back_nodes.begin(),
                              furthest_back_nodes.end(), node->get_id());
    if (found_it != furthest_back_nodes.end())
      break;

    if (node->get_type() == Node::NodeType::CALL) {
      auto call_node = static_cast<const Call *>(node);
      symbols_t more_symbols = call_node->get_locally_generated_symbols();
      symbols.insert(more_symbols.begin(), more_symbols.end());
    }

    node = node->get_prev();
  }

  return symbols;
}

symbols_t Node::get_generated_symbols() const {
  std::unordered_set<node_id_t> furthest_back_nodes;
  return get_generated_symbols(furthest_back_nodes);
}

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

unsigned Node::count_children(bool recursive) const {
  std::vector<const Node *> children;
  const Node *node = this;

  if (node->get_type() == Node::NodeType::BRANCH) {
    auto branch_node = static_cast<const Branch *>(node);

    children.push_back(branch_node->get_on_true());
    children.push_back(branch_node->get_on_false());
  } else if (node->get_next()) {
    children.push_back(node->get_next());
  }

  unsigned n = children.size();

  while (recursive && children.size()) {
    node = children[0];
    children.erase(children.begin());

    if (node->get_type() == Node::NodeType::BRANCH) {
      auto branch_node = static_cast<const Branch *>(node);

      children.push_back(branch_node->get_on_true());
      children.push_back(branch_node->get_on_false());

      n += 2;
    } else if (node->get_next()) {
      children.push_back(node->get_next());
      n++;
    }
  }

  return n;
}

unsigned Node::count_code_paths() const {
  std::vector<const Node *> nodes{this};
  const Node *node;
  unsigned paths = 0;

  while (nodes.size()) {
    node = nodes[0];
    nodes.erase(nodes.begin());

    switch (node->get_type()) {
    case Node::NodeType::BRANCH: {
      auto branch_node = static_cast<const Branch *>(node);
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
      break;
    }
    case Node::NodeType::CALL: {
      nodes.push_back(node->get_next());
      break;
    }
    case Node::NodeType::ROUTE:
      paths++;
      break;
    }
  }

  return paths;
}

std::string Node::dump_recursive(int lvl) const {
  std::stringstream result;
  std::string pad(lvl * 2, ' ');

  result << pad << dump(true) << "\n";

  switch (type) {
  case NodeType::BRANCH: {
    const Branch *branch_node = static_cast<const Branch *>(this);
    result << branch_node->get_on_true()->dump_recursive(lvl + 1);
    result << branch_node->get_on_false()->dump_recursive(lvl + 1);
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    if (next)
      result << next->dump_recursive(lvl + 1);
    break;
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
    case Node::NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
    } break;
    case Node::NodeType::ROUTE:
    case Node::NodeType::CALL: {
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

} // namespace bdd