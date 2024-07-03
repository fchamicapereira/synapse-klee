#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Else : public x86Module {
public:
  Else(const bdd::Node *node) : x86Module(ModuleType::x86_Else, "Else", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Else *cloned = new Else(node);
    return cloned;
  }
};

class ElseGenerator : public x86ModuleGenerator {
public:
  ElseGenerator() : x86ModuleGenerator(ModuleType::x86_Else, "Else") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    return std::nullopt;
  }

  virtual std::vector<generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    // Never explicitly generate this module from the BDD.
    return {};
  }
};

} // namespace x86
} // namespace synapse
