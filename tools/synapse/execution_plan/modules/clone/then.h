#pragma once

#include "clone_module.h"
#include "else.h"

namespace synapse {
namespace targets {
namespace clone {

class Then : public CloneModule {
public:
  Then() : CloneModule(ModuleType::Clone_Then, "Then") {
    next_target = TargetType::x86;
  }
  Then(BDD::Node_ptr node) : CloneModule(ModuleType::Clone_Then, "Then", node) {
    next_target = TargetType::x86;
  }

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
    auto cloned = new Then(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace clone
} // namespace targets
} // namespace synapse