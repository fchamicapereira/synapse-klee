#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class Forward : public TofinoCPUModule {
private:
  int dst_device;

public:
  Forward(const bdd::Node *node, int _dst_device)
      : TofinoCPUModule(ModuleType::TofinoCPU_Forward, "Forward", node),
        dst_device(_dst_device) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Forward *cloned = new Forward(node, dst_device);
    return cloned;
  }

  int get_dst_device() const { return dst_device; }
};

class ForwardGenerator : public TofinoCPUModuleGenerator {
public:
  ForwardGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_Forward, "Forward") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return new_eps;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::FWD) {
      return new_eps;
    }

    int dst_device = route_node->get_dst_device();

    Module *module = new Forward(node, dst_device);
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
