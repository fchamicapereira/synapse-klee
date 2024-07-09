#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class Broadcast : public TofinoModule {
public:
  Broadcast(const bdd::Node *node)
      : TofinoModule(ModuleType::Tofino_Broadcast, "Broadcast", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
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
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::ROUTE) {
      return std::nullopt;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::BCAST) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return products;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::BCAST) {
      return products;
    }

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    tofino_ctx->parser_accept(ep, node);

    Module *module = new Broadcast(node);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace tofino
} // namespace synapse
