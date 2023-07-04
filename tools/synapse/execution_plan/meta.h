#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "target.h"

#include <unordered_map>

namespace synapse {

typedef std::unordered_set<BDD::node_id_t> root_nodes_t;

struct ep_meta_t {
  unsigned depth;
  unsigned nodes;
  unsigned reordered_nodes;

  std::unordered_map<TargetType, root_nodes_t> roots_per_target;
  std::unordered_map<TargetType, unsigned> nodes_per_target;

  std::unordered_set<BDD::node_id_t> processed_nodes;

  ep_meta_t() : depth(0), nodes(0), reordered_nodes(0) {}

  ep_meta_t(const ep_meta_t &meta)
      : depth(meta.depth), nodes(meta.nodes),
        reordered_nodes(meta.reordered_nodes),
        roots_per_target(meta.roots_per_target),
        nodes_per_target(meta.nodes_per_target),
        processed_nodes(meta.processed_nodes) {}

  void add_target(TargetType type) {
    roots_per_target[type].emplace();
    nodes_per_target[type] = 0;
  }

  ep_meta_t &operator=(const ep_meta_t &) = default;
  ep_meta_t &operator=(ep_meta_t &&) = default;
};

} // namespace synapse
