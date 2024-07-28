#include "visitor.h"
#include "execution_plan.h"
#include "node.h"
#include "../targets/module.h"
#include "../log.h"

#include <vector>

namespace synapse {

void EPVisitor::visit(const EP *ep) {
  const EPNode *root = ep->get_root();

  if (root) {
    root->visit(*this, ep);
  }
}

void EPVisitor::visit(const EP *ep, const EPNode *node) {
  log(node);
  const Module *module = node->get_module();
  module->visit(*this, ep, node);
}

void EPVisitor::log(const EPNode *node) const {
  const Module *module = node->get_module();
  const std::string &name = module->get_name();
  TargetType target = module->get_target();
  Log::dbg() << "[" << target << "] " << name << "\n";
}

} // namespace synapse
