#pragma once

#include "./tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class Then : public TofinoModule {
public:
  Then() : TofinoModule(ModuleType::Tofino_Then, "Then") {}

  Then(BDD::Node_ptr node)
      : TofinoModule(ModuleType::Tofino_Then, "Then", node) {}

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
} // namespace tofino
} // namespace targets
} // namespace synapse
