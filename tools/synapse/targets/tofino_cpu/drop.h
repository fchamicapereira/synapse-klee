#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class Drop : public TofinoCPUModule {
public:
  Drop(const bdd::Node *node)
      : TofinoCPUModule(ModuleType::TofinoCPU_Drop, "Drop", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Drop *cloned = new Drop(node);
    return cloned;
  }
};

class DropGenerator : public TofinoCPUModuleGenerator {
public:
  DropGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_Drop, "Drop") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return new_eps;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::DROP) {
      return new_eps;
    }

    Module *module = new Drop(node);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace tofino_cpu
} // namespace synapse
