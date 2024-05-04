#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace x86_tofino {

class Else : public Module {
public:
  Else()
      : Module(ModuleType::x86_Tofino_Else, TargetType::x86_Tofino, "Else") {}
  Else(BDD::Node_ptr node)
      : Module(ModuleType::x86_Tofino_Else, TargetType::x86_Tofino, "Else",
               node) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    return processing_result_t();
  }

public:
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
} // namespace x86_tofino
} // namespace targets
} // namespace synapse
