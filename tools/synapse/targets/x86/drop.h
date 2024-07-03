#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Drop : public x86Module {
public:
  Drop(const bdd::Node *node) : x86Module(ModuleType::x86_Drop, "Drop", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Drop *cloned = new Drop(node);
    return cloned;
  }
};

class DropGenerator : public x86ModuleGenerator {
public:
  DropGenerator() : x86ModuleGenerator(ModuleType::x86_Drop, "Drop") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::ROUTE) {
      return false;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::DROP) {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    if (bdd_node_match_pattern(node))
      return current_speculative_ctx;
    return std::nullopt;
  }

  virtual std::vector<generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<generator_product_t> products;

    if (!bdd_node_match_pattern(node)) {
      return products;
    }

    Module *module = new Drop(node);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace x86
} // namespace synapse
