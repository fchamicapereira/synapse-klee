#pragma once

#include "ignore.h"
#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class RegisterRead : public TofinoModule {
public:
  RegisterRead()
      : TofinoModule(ModuleType::Tofino_RegisterRead, "RegisterRead") {}

  RegisterRead(BDD::Node_ptr node)
      : TofinoModule(ModuleType::Tofino_RegisterRead, "RegisterRead", node) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    // TODO: implement
    processing_result_t result;
    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new RegisterRead(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    return true;
  }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
