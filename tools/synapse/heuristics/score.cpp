#include "score.h"
#include "../log.h"

namespace synapse {

std::ostream &operator<<(std::ostream &os, const Score &score) {
  os << "<";

  bool first = true;
  for (size_t i = 0u; i < score.values.size(); i++) {
    if (!first)
      os << ",";
    first = false;

    ScoreCategory category = score.categories[i].first;
    ScoreObjective objective = score.categories[i].second;
    int64_t value = score.values[i];

    switch (objective) {
    case ScoreObjective::MIN:
      os << "[-]";
      value *= -1;
      break;
    case ScoreObjective::MAX:
      os << "[+]";
      break;
    }
    os << category;
    os << "=";
    os << value;
  }

  os << ">";
  return os;
}

std::ostream &operator<<(std::ostream &os, ScoreCategory score_category) {
  switch (score_category) {
  case ScoreCategory::SendToControllerNodes:
    os << "#SendToController";
    break;
  case ScoreCategory::Recirculations:
    os << "#Recirculations";
    break;
  case ScoreCategory::ReorderedNodes:
    os << "#Reordered";
    break;
  case ScoreCategory::SwitchNodes:
    os << "#SwitchNodes";
    break;
  case ScoreCategory::SwitchProgressionNodes:
    os << "#SNodes";
    break;
  case ScoreCategory::SwitchLeaves:
    os << "#SwitchLeaves";
    break;
  case ScoreCategory::Nodes:
    os << "#Nodes";
    break;
  case ScoreCategory::ControllerNodes:
    os << "#Controller";
    break;
  case ScoreCategory::Depth:
    os << "Depth";
    break;
  case ScoreCategory::ConsecutiveObjectOperationsInSwitch:
    os << "#ConsecutiveSwitchObjOp";
    break;
  case ScoreCategory::HasNextStatefulOperationInSwitch:
    os << "HasNextStatefulOpInSwitch";
    break;
  case ScoreCategory::ProcessedBDD:
    os << "BDDProgress";
    break;
  case ScoreCategory::ProcessedBDDPercentage:
    os << "Progress";
    break;
  case ScoreCategory::SwitchDataStructures:
    os << "#SwitchDS";
    break;
  case ScoreCategory::Throughput:
    os << "T(pps)";
    break;
  case ScoreCategory::SpeculativeThroughput:
    os << "T*(pps)";
    break;
  }
  return os;
}

int64_t Score::get_nr_nodes(const EP *ep) const {
  const EPMeta &meta = ep->get_meta();
  return meta.nodes;
}

std::vector<const EPNode *>
Score::get_nodes_with_type(const EP *ep,
                           const std::vector<ModuleType> &types) const {
  std::vector<const EPNode *> found;

  const EPNode *root = ep->get_root();

  if (!root) {
    return found;
  }

  root->visit_nodes([&found, &types](const EPNode *node) {
    const Module *module = node->get_module();

    auto found_it = std::find(types.begin(), types.end(), module->get_type());
    if (found_it != types.end()) {
      found.push_back(node);
    }

    return EPNodeVisitAction::VISIT_CHILDREN;
  });

  return found;
}

int64_t Score::get_depth(const EP *ep) const {
  const EPMeta &meta = ep->get_meta();
  return meta.depth;
}

int64_t Score::get_nr_switch_nodes(const EP *ep) const {
  int64_t switch_nodes = 0;

  const EPMeta &meta = ep->get_meta();
  auto tofino_nodes_it = meta.bdd_nodes_per_target.find(TargetType::Tofino);

  if (tofino_nodes_it != meta.bdd_nodes_per_target.end()) {
    switch_nodes += tofino_nodes_it->second;
  }

  return switch_nodes;
}

int64_t Score::get_nr_switch_progression_nodes(const EP *ep) const {
  int64_t switch_nodes = get_nr_switch_nodes(ep);
  int64_t recirc_nodes = get_nr_recirculations(ep);
  int64_t send_to_controller_nodes = get_nr_send_to_controller(ep);
  return switch_nodes - recirc_nodes - send_to_controller_nodes;
}

int64_t Score::get_nr_controller_nodes(const EP *ep) const {
  int64_t controller_nodes = 0;

  const EPMeta &meta = ep->get_meta();
  auto tofino_controller_nodes_it =
      meta.nodes_per_target.find(TargetType::TofinoCPU);

  if (tofino_controller_nodes_it != meta.nodes_per_target.end()) {
    controller_nodes += tofino_controller_nodes_it->second;
  }

  return controller_nodes;
}

int64_t Score::get_nr_send_to_controller(const EP *ep) const {
  std::vector<const EPNode *> send_to_controller =
      get_nodes_with_type(ep, {ModuleType::Tofino_SendToController});
  return send_to_controller.size();
}

int64_t Score::get_nr_reordered_nodes(const EP *ep) const {
  const EPMeta &meta = ep->get_meta();
  return meta.reordered_nodes;
}

int64_t Score::get_nr_switch_leaves(const EP *ep) const {
  int64_t switch_leaves = 0;

  const std::vector<EPLeaf> &leaves = ep->get_leaves();
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

int64_t Score::next_op_same_obj_in_switch(const EP *ep) const {
  TargetType target = ep->get_current_platform();

  if (target != TargetType::Tofino) {
    return 0;
  }

  const bdd::Node *next = ep->get_next_node();

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

int64_t Score::next_op_is_stateful_in_switch(const EP *ep) const {
  TargetType target = ep->get_current_platform();

  if (target != TargetType::Tofino) {
    return 0;
  }

  const bdd::Node *next = ep->get_next_node();

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

int64_t Score::get_processed_bdd(const EP *ep) const {
  const EPMeta &meta = ep->get_meta();
  return meta.processed_nodes.size();
}

int64_t Score::get_percentage_of_processed_bdd(const EP *ep) const {
  const EPMeta &meta = ep->get_meta();
  return 100 * meta.get_bdd_progress();
}

int64_t Score::get_nr_switch_data_structures(const EP *ep) const {
  int64_t switch_data_structures = 0;

  const Context &ctx = ep->get_ctx();
  const std::unordered_map<addr_t, PlacementDecision> &placements =
      ctx.get_placements();

  std::unordered_set<PlacementDecision> switch_placements{
      PlacementDecision::Tofino_SimpleTable,
      PlacementDecision::Tofino_TTLCachedTable,
      PlacementDecision::Tofino_VectorRegister,
  };

  for (const auto &[obj, decision] : placements) {
    if (switch_placements.find(decision) != switch_placements.end()) {
      switch_data_structures++;
    }
  }

  return switch_data_structures;
}

int64_t Score::get_nr_recirculations(const EP *ep) const {
  std::vector<const EPNode *> recirculate =
      get_nodes_with_type(ep, {ModuleType::Tofino_Recirculate});
  return recirculate.size();
}

int64_t Score::get_throughput_prediction(const EP *ep) const {
  return ep->estimate_throughput_pps();
}

int64_t Score::get_throughput_speculation(const EP *ep) const {
  return ep->speculate_throughput_pps();
}

} // namespace synapse
