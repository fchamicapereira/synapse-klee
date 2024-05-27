#include "score.h"
#include "../log.h"

namespace synapse {

int Score::get_nr_nodes(const EP &ep) const {
  const EPMeta &meta = ep.get_meta();
  return meta.nodes;
}

std::vector<const EPNode *>
Score::get_nodes_with_type(const EP &ep,
                           const std::vector<ModuleType> &types) const {
  std::vector<const EPNode *> found;

  const EPNode *root = ep.get_root();

  if (!root) {
    return found;
  }

  std::vector<const EPNode *> nodes{root};

  while (nodes.size()) {
    const EPNode *node = nodes[0];
    nodes.erase(nodes.begin());

    const Module *module = node->get_module();

    auto found_it = std::find(types.begin(), types.end(), module->get_type());
    if (found_it != types.end()) {
      found.push_back(node);
    }

    const std::vector<EPNode *> &children = node->get_children();
    for (const EPNode *child : children) {
      nodes.push_back(child);
    }
  }

  return found;
}

int Score::get_nr_counters(const EP &ep) const {
  std::vector<const EPNode *> nodes =
      get_nodes_with_type(ep, {ModuleType::Tofino_CounterRead,
                               ModuleType::Tofino_CounterIncrement});
  return nodes.size();
}

int Score::get_nr_simple_tables(const EP &ep) const {
  std::vector<const EPNode *> nodes =
      get_nodes_with_type(ep, {ModuleType::Tofino_TableLookup});
  return nodes.size();
}

int Score::get_nr_int_allocator_ops(const EP &ep) const {
  std::vector<const EPNode *> nodes =
      get_nodes_with_type(ep, {
                                  ModuleType::Tofino_IntegerAllocatorAllocate,
                                  ModuleType::Tofino_IntegerAllocatorQuery,
                                  ModuleType::Tofino_IntegerAllocatorRejuvenate,
                              });

  return nodes.size();
}

int Score::get_depth(const EP &ep) const {
  const EPMeta &meta = ep.get_meta();
  return meta.depth;
}

int Score::get_nr_switch_nodes(const EP &ep) const {
  int switch_nodes = 0;

  const EPMeta &meta = ep.get_meta();
  auto tofino_nodes_it = meta.nodes_per_target.find(TargetType::Tofino);

  if (tofino_nodes_it != meta.nodes_per_target.end()) {
    switch_nodes += tofino_nodes_it->second;
  }

  std::vector<const EPNode *> send_to_controller =
      get_nodes_with_type(ep, {ModuleType::Tofino_SendToController});

  // Let's ignore the SendToController nodes
  return switch_nodes - send_to_controller.size();
}

int Score::get_nr_controller_nodes(const EP &ep) const {
  int controller_nodes = 0;

  const EPMeta &meta = ep.get_meta();
  auto tofino_controller_nodes_it =
      meta.nodes_per_target.find(TargetType::TofinoCPU);

  if (tofino_controller_nodes_it != meta.nodes_per_target.end()) {
    controller_nodes += tofino_controller_nodes_it->second;
  }

  return controller_nodes;
}

int Score::get_nr_reordered_nodes(const EP &ep) const {
  const EPMeta &meta = ep.get_meta();
  return meta.reordered_nodes;
}

int Score::get_nr_switch_leaves(const EP &ep) const {
  int switch_leaves = 0;

  const std::vector<EPLeaf> &leaves = ep.get_leaves();
  std::vector<TargetType> switch_types{TargetType::Tofino};

  for (const EPLeaf &leaf : leaves) {
    const Module *module = leaf.node->get_module();
    TargetType target = module->get_target();

    if (target == TargetType::TofinoCPU) {
      switch_leaves++;
    }
  }

  return switch_leaves;
}

int Score::next_op_same_obj_in_switch(const EP &ep) const {
  TargetType target = ep.get_current_platform();

  if (target != TargetType::Tofino) {
    return 0;
  }

  const bdd::Node *next = ep.get_next_node();

  if (!next) {
    return 0;
  }

  const bdd::Node *prev = next->get_prev();

  if (!prev) {
    return 0;
  }

  if (next->get_type() != bdd::NodeType::CALL ||
      prev->get_type() != bdd::NodeType::CALL) {
    return 0;
  }

  const bdd::Call *next_call = static_cast<const bdd::Call *>(next);
  const bdd::Call *prev_call = static_cast<const bdd::Call *>(prev);

  std::optional<addr_t> next_obj = bdd::get_obj_from_call(next_call);
  std::optional<addr_t> prev_obj = bdd::get_obj_from_call(prev_call);

  if (!next_obj.has_value() || !prev_obj.has_value()) {
    return 0;
  }

  if (*next_obj == *prev_obj) {
    return 1;
  }

  return 0;
}

int Score::next_op_is_stateful_in_switch(const EP &ep) const {
  TargetType target = ep.get_current_platform();

  if (target != TargetType::Tofino) {
    return 0;
  }

  const bdd::Node *next = ep.get_next_node();

  if (!next) {
    return 0;
  }

  if (next->get_type() != bdd::NodeType::CALL) {
    return 0;
  }

  const bdd::Call *next_call = static_cast<const bdd::Call *>(next);
  const call_t &call = next_call->get_call();

  std::vector<std::string> stateful_ops{
      "map_get",
      "map_put",
      "vector_borrow",
      "vector_return",
      "dchain_allocate_new_index",
      "dchain_rejuvenate_index",
      "dchain_is_index_allocated",
  };

  auto found_it =
      std::find(stateful_ops.begin(), stateful_ops.end(), call.function_name);

  if (found_it != stateful_ops.end()) {
    return 1;
  }

  return 0;
}

int Score::get_percentage_of_processed_bdd(const EP &ep) const {
  const EPMeta &meta = ep.get_meta();
  return 100 * meta.get_bdd_progress();
}

std::ostream &operator<<(std::ostream &os, const Score &score) {
  os << "<";

  bool first = true;
  for (auto i = 0u; i < score.values.size(); i++) {
    auto value = score.values[i];

    if (!first) {
      os << ",";
    }

    os << value;

    first &= false;
  }

  os << ">";
  return os;
}

std::ostream &operator<<(std::ostream &os, ScoreCategory score_category) {
  switch (score_category) {
  case ScoreCategory::NumberOfReorderedNodes:
    os << "NumberOfReorderedNodes";
    break;
  case ScoreCategory::NumberOfSwitchNodes:
    os << "NumberOfSwitchNodes";
    break;
  case ScoreCategory::NumberOfSwitchLeaves:
    os << "NumberOfSwitchLeaves";
    break;
  case ScoreCategory::NumberOfNodes:
    os << "NumberOfNodes";
    break;
  case ScoreCategory::NumberOfControllerNodes:
    os << "NumberOfControllerNodes";
    break;
  case ScoreCategory::NumberOfCounters:
    os << "NumberOfCounters";
    break;
  case ScoreCategory::NumberOfSimpleTables:
    os << "NumberOfSimpleTables";
    break;
  case ScoreCategory::NumberOfIntAllocatorOps:
    os << "NumberOfIntAllocatorOps";
    break;
  case ScoreCategory::Depth:
    os << "Depth";
    break;
  case ScoreCategory::ConsecutiveObjectOperationsInSwitch:
    os << "ConsecutiveObjectOperationsInSwitch";
    break;
  case ScoreCategory::HasNextStatefulOperationInSwitch:
    os << "HasNextStatefulOperationInSwitch";
    break;
  case ScoreCategory::ProcessedBDDPercentage:
    os << "ProcessedBDDPercentage";
    break;
  }
  return os;
}

} // namespace synapse
