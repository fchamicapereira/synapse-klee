#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class Then : public TofinoCPUModule {
public:
  Then(const bdd::Node *node)
      : TofinoCPUModule(ModuleType::TofinoCPU_Then, "Then", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Then *cloned = new Then(node);
    return cloned;
  }
};

class ThenGenerator : public TofinoCPUModuleGenerator {
public:
  ThenGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_Then, "Then") {}

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

} // namespace tofino_cpu
} // namespace synapse
