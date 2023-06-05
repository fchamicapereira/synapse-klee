#pragma once

#include "clone_module.h"

namespace synapse {
namespace targets {
namespace clone {

class Else : public CloneModule {
public:
  Else() : CloneModule(ModuleType::Clone_Else, "Else") {}
  Else(BDD::Node_ptr node) : CloneModule(ModuleType::Clone_Else, "Else", node) {}

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
} // namespace clone
} // namespace targets
} // namespace synapse