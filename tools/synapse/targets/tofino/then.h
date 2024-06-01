#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class Then : public TofinoModule {
public:
  Then(const bdd::Node *node)
      : TofinoModule(ModuleType::Tofino_Then, "Then", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Then *cloned = new Then(node);
    return cloned;
  }
};

class ThenGenerator : public TofinoModuleGenerator {
public:
  ThenGenerator() : TofinoModuleGenerator(ModuleType::Tofino_Then, "Then") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    // Never explicitly generate this module from the BDD.
    return std::vector<const EP *>();
  }
};

} // namespace tofino
} // namespace synapse
