#pragma once

#include "bdd/nodes/node.h"
#include "execution_plan.h"
#include "execution_plan_node.h"
#include "modules/x86/forward.h"
#include "target.h"

#include <vector>
#include <map>

namespace synapse {
class CloneUtil {
  public:
  static ExecutionPlanNode_ptr get_leaf(const ExecutionPlan& ep, Target_ptr target) {
    auto ep_node = ep.get_root();
    BDD::node_id_t node_id = ep.get_clone_leaf(target->id);

    auto nodes = ep_node->get_next();

    while(nodes.size()) {
      auto ep_node = nodes.at(0);
      nodes.erase(nodes.begin());

      auto module = ep_node->get_module();
      auto target = ep_node->get_target();
      auto node = module->get_node();

      if(ep_node->get_target()->id == target->id && node->get_id() == node_id) {
        return ep_node;
      }

      auto next = ep_node->get_next();
      nodes.insert(nodes.end(), next.begin(), next.end());
    }

    assert(false && "Could not find leaf, malformed CloNe EP");
  }
};
}
