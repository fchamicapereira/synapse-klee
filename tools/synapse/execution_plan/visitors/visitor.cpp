#include "visitor.h"
#include "../../log.h"
#include "../execution_plan.h"
#include "../execution_plan_node.h"
#include "../modules/module.h"

#include <vector>

namespace synapse {

void ExecutionPlanVisitor::visit(ExecutionPlan ep) {
  auto root = ep.get_root();

  if (root) {
    root->visit(*this);
  }
}

void ExecutionPlanVisitor::visit(const ExecutionPlanNode *ep_node) {
  auto mod = ep_node->get_module();
  auto next = ep_node->get_next();

  log(ep_node);

  mod->visit(*this);

  for (auto branch : next) {
    branch->visit(*this);
  }
}

void ExecutionPlanVisitor::log(const ExecutionPlanNode *ep_node) const {
  auto mod = ep_node->get_module();

  Log::dbg() << "[visitor::" << mod->get_target_name() << "] "
             << mod->get_name() << "\n";
}

} // namespace synapse
