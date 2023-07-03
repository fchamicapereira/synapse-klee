#pragma once

#include "clone_module.h"
#include "else.h"

namespace synapse {
namespace targets {
namespace clone {

class Then : public CloneModule {
private:
public:
  Then() : CloneModule(ModuleType::Clone_Then, "Then"){
  }
  Then(BDD::Node_ptr node): CloneModule(ModuleType::Clone_Then, "Then", node) {

  }

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    if(node->get_type() != BDD::Node::NodeType::BRANCH) {
      return result;
    }

    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);
    auto active_leaf = ep.get_active_leaf();

    if(active_leaf == nullptr) {
      return result;
    }

    auto module = active_leaf->get_module();
    if(module->get_type() == Module::ModuleType::Clone_Then) {
      Target_ptr target = mb->get_origin_from_node(node)->target;

      auto new_module = std::make_shared<Then>(node);
      auto new_ep = ep.ignore_leaf(node, target, false);

      BDD_ptr _bdd = BDD_ptr(new BDD::BDD(new_ep.get_clone_bdd_id(), new_ep.get_bdd().get_init(), node));
      new_ep.get_bdd().set_process(node);
      new_ep.get_bdd().set_id(new_ep.get_clone_bdd_id());

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