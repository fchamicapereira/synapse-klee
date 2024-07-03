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
                    bool should_process_node) {
  ep_node_id_t node_id = new_node->get_id();
  if (visited_ep_nodes.find(node_id) != visited_ep_nodes.end()) {
    return;
  }

  visited_ep_nodes.insert(node_id);

  const Module *module = new_node->get_module();
  TargetType target = module->get_target();

  if (should_process_node) {
    process_node(leaf->next, target);
  }

  nodes++;
  nodes_per_target[target]++;
}

double EPMeta::get_bdd_progress() const {
  return processed_nodes.size() / static_cast<double>(total_bdd_nodes);
}

void EPMeta::update_total_bdd_nodes(const bdd::BDD *bdd) {
  total_bdd_nodes = bdd->size();
}

} // namespace synapse