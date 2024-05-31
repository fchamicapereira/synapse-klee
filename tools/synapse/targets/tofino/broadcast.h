#pragma once

#include "tofino_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace tofino {

class Broadcast : public TofinoModule {
public:
  Broadcast(const bdd::Node *node)
      : TofinoModule(ModuleType::Tofino_Broadcast, "Broadcast", node) {}

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const {
    Broadcast *cloned = new Broadcast(node);
    return cloned;
  }
};

class BroadcastGenerator : public TofinoModuleGenerator {
public:
  BroadcastGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_Broadcast, "Broadcast") {}

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

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    TNA &tna = get_mutable_tna(new_ep);
    tna.update_parser_accept(ep);

    Module *module = new Broadcast(node);
    EPNode *ep_node = new EPNode(module);

    if (node->get_next()) {
      EPLeaf leaf(ep_node, node->get_next());
      new_ep->process_leaf(ep_node, {leaf});
    } else {
      new_ep->process_leaf(ep_node, {});
    }

    return new_eps;
  }
};

} // namespace tofino
} // namespace synapse
