#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Then : public x86Module {
public:
  Then(const bdd::Node *node) : x86Module(ModuleType::x86_Then, "Then", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Then *cloned = new Then(node);
    return cloned;
  }
};

class ThenGenerator : public x86ModuleGenerator {
public:
  ThenGenerator() : x86ModuleGenerator(ModuleType::x86_Then, "Then") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    return std::nullopt;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    // Never explicitly generate this module from the BDD.
    return {};
  }
};

} // namespace x86
} // namespace synapse
