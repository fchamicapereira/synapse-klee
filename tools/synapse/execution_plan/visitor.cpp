#include "visitor.h"
#include "execution_plan.h"
#include "execution_plan_node.h"
#include "../targets/module.h"
#include "../log.h"

#include <vector>

namespace synapse {

void EPVisitor::visit(EP ep) {
  auto root = ep.get_root();

  if (root) {
    root->visit(*this);
  }
}

void EPVisitor::visit(const EPNode *node) {
  const Module *module = node->get_module();
  const std::vector<EPNode *> &children = node->get_children();

  log(node);

  module->visit(*this, node);

  for (EPNode *child : children) {
    child->visit(*this);
  }
}

void EPVisitor::log(const EPNode *node) const {
  const Module *module = node->get_module();
  const std::string &name = module->get_name();
  TargetType target = module->get_target();
  Log::dbg() << "[" << target << "] " << name << "\n";
}

} // namespace synapse
