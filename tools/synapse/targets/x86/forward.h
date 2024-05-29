#pragma once

#include "x86_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace x86 {

class Forward : public x86Module {
private:
  int dst_device;

public:
  Forward(const bdd::Node *node, int _dst_device)
      : x86Module(ModuleType::x86_Forward, "Forward", node),
        dst_device(_dst_device) {}

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const {
    Forward *cloned = new Forward(node, dst_device);
    return cloned;
  }

  int get_dst_device() const { return dst_device; }
};

class ForwardGenerator : public x86ModuleGenerator {
public:
  ForwardGenerator() : x86ModuleGenerator(ModuleType::x86_Forward, "Forward") {}

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

    if (node->get_next()) {
      EPLeaf leaf(ep_node, node->get_next());
      new_ep->process_leaf(ep_node, {leaf});
    } else {
      new_ep->process_leaf(ep_node, {});
    }

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
