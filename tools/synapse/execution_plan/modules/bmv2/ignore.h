#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace bmv2 {

class Ignore : public Module {
private:
  std::vector<std::string> functions_to_ignore;

public:
  Ignore() : Module(ModuleType::BMv2_Ignore, TargetType::BMv2, "Ignore") {
    functions_to_ignore = std::vector<std::string>{
        BDD::symbex::FN_CURRENT_TIME,
        BDD::symbex::FN_ETHER_HASH,
        BDD::symbex::FN_DCHAIN_REJUVENATE,
    };
  }

  Ignore(BDD::Node_ptr node)
      : Module(ModuleType::BMv2_Ignore, TargetType::BMv2, "Ignore", node) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    auto found_it = std::find(functions_to_ignore.begin(),
                              functions_to_ignore.end(), call.function_name);

    if (found_it != functions_to_ignore.end()) {
      auto new_module = std::make_shared<Ignore>(node);
      auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::BMv2);

      result.module = new_module;
      result.next_eps.push_back(new_ep);
    }

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Ignore(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace bmv2
} // namespace targets
} // namespace synapse
