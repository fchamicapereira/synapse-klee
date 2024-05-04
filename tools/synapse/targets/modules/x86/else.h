#pragma once

#include "x86_module.h"

namespace synapse {
namespace targets {
namespace x86 {

class Else : public x86Module {
public:
  Else() : x86Module(ModuleType::x86_Else, "Else") {}
  Else(BDD::Node_ptr node) : x86Module(ModuleType::x86_Else, "Else", node) {}

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
} // namespace x86
} // namespace targets
} // namespace synapse
