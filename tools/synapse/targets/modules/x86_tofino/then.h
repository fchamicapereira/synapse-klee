#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace x86_tofino {

class Then : public Module {
public:
  Then()
      : Module(ModuleType::x86_Tofino_Then, TargetType::x86_Tofino, "Then") {}

  Then(BDD::Node_ptr node)
      : Module(ModuleType::x86_Tofino_Then, TargetType::x86_Tofino, "Then",
               node) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;
    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Then(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace x86_tofino
} // namespace targets
} // namespace synapse
