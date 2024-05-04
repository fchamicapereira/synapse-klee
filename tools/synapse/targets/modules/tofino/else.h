#pragma once

#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class Else : public TofinoModule {
public:
  Else() : TofinoModule(ModuleType::Tofino_Else, "Else") {}

  Else(BDD::Node_ptr node)
      : TofinoModule(ModuleType::Tofino_Else, "Else", node) {}

public:
  // We expect to generate this module only manually.
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;
    return result;
  }

  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Else(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
