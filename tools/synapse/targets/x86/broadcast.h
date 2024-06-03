#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Broadcast : public x86Module {
public:
  Broadcast(const bdd::Node *node)
      : x86Module(ModuleType::x86_Broadcast, "Broadcast", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Broadcast *cloned = new Broadcast(node);
    return cloned;
  }
};

class BroadcastGenerator : public x86ModuleGenerator {
public:
  BroadcastGenerator()
      : x86ModuleGenerator(ModuleType::x86_Broadcast, "Broadcast") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return new_eps;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::BCAST) {
      return new_eps;
    }

    Module *module = new Broadcast(node);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
