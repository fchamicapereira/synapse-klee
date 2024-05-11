#pragma once

#include "node.h"

#include <memory>
#include <vector>

namespace bdd {

class NodeManager {
private:
  std::vector<std::unique_ptr<Node>> nodes;

public:
  NodeManager() {}
  NodeManager(NodeManager &&other) = default;

  void add_node(Node *node) { nodes.emplace_back(node); }

  void free_node(Node *node) {
    auto it = std::find_if(
        nodes.begin(), nodes.end(),
        [node](const std::unique_ptr<Node> &n) { return n.get() == node; });

    if (it != nodes.end()) {
      assert((*it)->get_id() == node->get_id());
      nodes.erase(it);
    }
  }

  NodeManager operator+(const NodeManager &other) const {
    NodeManager manager;
    for (const std::unique_ptr<Node> &node : nodes)
      node->clone(manager);
    for (const std::unique_ptr<Node> &node : other.nodes)
      node->clone(manager);
    return manager;
  }

  bool operator==(const NodeManager &other) const {
    return nodes == other.nodes;
  }
};

} // namespace bdd