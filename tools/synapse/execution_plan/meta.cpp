#include "meta.h"

#include "execution_plan.h"
#include "../targets/module.h"

namespace synapse {

void EPMeta::process_node(const bdd::Node *node, TargetType target) {
  if (processed_nodes.find(node->get_id()) == processed_nodes.end()) {
    processed_nodes.insert(node->get_id());
    bdd_nodes_per_target[target]++;
  }
}

void EPMeta::update(const EPLeaf *leaf, const EPNode *new_node,
                    const HitRateTree *hit_rate_tree,
                    bool should_process_node) {
  ep_node_id_t node_id = new_node->get_id();
  if (visited_ep_nodes.find(node_id) != visited_ep_nodes.end()) {
    return;
  }

  visited_ep_nodes.insert(node_id);

  const Module *module = new_node->get_module();
  TargetType target = module->get_target();

  TargetType next_target = module->get_next_target();
  if (next_target != target) {
    constraints_t constraints = get_node_constraints(new_node);
    std::optional<float> fraction = hit_rate_tree->get_fraction(constraints);
    assert(fraction.has_value());
    transfer_traffic(target, next_target, *fraction);
  }

  if (should_process_node) {
    process_node(leaf->next, target);
  }

  nodes++;
  nodes_per_target[target]++;
}

float EPMeta::get_bdd_progress() const {
  return processed_nodes.size() / static_cast<float>(total_bdd_nodes);
}

bool EPMeta::is_processed_node(const bdd::Node *node) const {
  return processed_nodes.find(node->get_id()) != processed_nodes.end();
}

void EPMeta::update_total_bdd_nodes(const bdd::BDD *bdd) {
  total_bdd_nodes = bdd->size();
}

void EPMeta::update_constraints_per_node(ep_node_id_t node,
                                         const constraints_t &constraints) {
  assert(constraints_per_node.find(node) == constraints_per_node.end());
  constraints_per_node[node] = constraints;
}

constraints_t EPMeta::get_node_constraints(const EPNode *node) const {
  assert(node);

  while (node) {
    ep_node_id_t node_id = node->get_id();
    auto found_it = constraints_per_node.find(node_id);

    if (found_it != constraints_per_node.end()) {
      return found_it->second;
    }

    node = node->get_prev();

    if (node) {
      assert(node->get_children().size() == 1 && "Ambiguous constraints");
    }
  }

  return {};
}

void EPMeta::transfer_traffic(TargetType old_target, TargetType new_target,
                              float fraction) {
  float &old_fraction = traffic_fraction_per_target[old_target];
  float &new_fraction = traffic_fraction_per_target[new_target];

  old_fraction -= fraction;
  new_fraction += fraction;

  assert(old_fraction >= 0);
  assert(old_fraction <= 1);

  assert(new_fraction <= 1);
  assert(new_fraction <= 1);
}

} // namespace synapse