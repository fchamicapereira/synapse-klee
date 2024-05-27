#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "../targets/target.h"

#include <unordered_map>

namespace synapse {

struct EPMeta {
  const size_t total_bdd_nodes;

  size_t depth;
  size_t nodes;
  size_t reordered_nodes;

  std::unordered_map<TargetType, size_t> nodes_per_target;
  bdd::nodes_t processed_nodes;

  EPMeta(const bdd::BDD *bdd)
      : total_bdd_nodes(bdd->size()), depth(0), nodes(0), reordered_nodes(0) {}

  EPMeta(const EPMeta &other)
      : total_bdd_nodes(other.total_bdd_nodes), depth(other.depth),
        nodes(other.nodes), reordered_nodes(other.reordered_nodes),
        nodes_per_target(other.nodes_per_target),
        processed_nodes(other.processed_nodes) {}

  EPMeta(EPMeta &&other)
      : total_bdd_nodes(other.total_bdd_nodes), depth(other.depth),
        nodes(other.nodes), reordered_nodes(other.reordered_nodes),
        nodes_per_target(std::move(other.nodes_per_target)),
        processed_nodes(std::move(other.processed_nodes)) {}

  EPMeta &operator=(const EPMeta &other) {
    if (this == &other) {
      return *this;
    }

    depth = other.depth;
    nodes = other.nodes;
    reordered_nodes = other.reordered_nodes;
    nodes_per_target = other.nodes_per_target;
    processed_nodes = other.processed_nodes;

    return *this;
  }

  float get_bdd_progress() const {
    return processed_nodes.size() / static_cast<float>(total_bdd_nodes);
  }
};

} // namespace synapse
