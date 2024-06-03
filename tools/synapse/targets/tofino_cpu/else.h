#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class Else : public TofinoCPUModule {
public:
  Else(const bdd::Node *node)
      : TofinoCPUModule(ModuleType::TofinoCPU_Else, "Else", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Else *cloned = new Else(node);
    return cloned;
  }
};

class ElseGenerator : public TofinoCPUModuleGenerator {
public:
  ElseGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_Else, "Else") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    // Never explicitly generate this module from the BDD.
    return std::vector<const EP *>();
  }
};

} // namespace tofino_cpu
} // namespace synapse
