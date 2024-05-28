#include "meta.h"

#include "execution_plan.h"
#include "../targets/module.h"

namespace synapse {

void EPMeta::update(const EPLeaf *leaf, const EPNode *new_node) {
  ep_node_id_t node_id = new_node->get_id();
  if (visited_nodes.find(node_id) != visited_nodes.end()) {
    return;
  }

  visited_nodes.insert(node_id);

  const Module *module = new_node->get_module();
  TargetType target = module->get_target();

  processed_nodes.insert(leaf->next->get_id());
  nodes++;
  nodes_per_target[target]++;
}

} // namespace synapse