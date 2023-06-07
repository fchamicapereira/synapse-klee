#pragma once

#include "clone_module.h"

namespace synapse {
namespace targets {
namespace clone {

class Drop : public CloneModule {
public:
  Drop() : CloneModule(ModuleType::Clone_Drop, "Drop") {
  }
  Drop(BDD::Node_ptr node) : CloneModule(ModuleType::Clone_Drop, "Drop", node) {

  }

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::ReturnProcess>(node);
    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);

    if (!casted) {
      return result;
    }

    if (casted->get_return_operation() != BDD::ReturnProcess::Operation::DROP) {
      return result;
    }

    auto next_node = mb->get_next_node();
    
    auto new_module = std::make_shared<Drop>(node);
    auto new_ep = ep.add_leaves(new_module, nullptr, true, false);
    result.module = new_module;
    result.next_eps.push_back(new_ep);
    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Drop(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace clone
} // namespace targets
} // namespace synapse
