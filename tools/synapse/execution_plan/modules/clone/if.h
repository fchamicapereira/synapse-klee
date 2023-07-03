#pragma once


#include "call-paths-to-bdd.h"
#include "clone_module.h"
#include "klee/Expr.h"
#include "models/port.h"
#include "parser/infrastructure.h"
#include "then.h"
#include "else.h"
#include "memory_bank.h"
#include <map>
#include <cstdio>

namespace synapse {
namespace targets {
namespace clone {
class If: public CloneModule {
private:

public:
  If() : CloneModule(ModuleType::Clone_If, "If") {}

  If(BDD::Node_ptr node)
    : CloneModule(ModuleType::Clone_If, "If", node) {}

private: 
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    if(node->get_type() != BDD::Node::NodeType::BRANCH) {
      return result;
    }

    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);

    if(mb->get_next_origin() == nullptr) {
      return result;
    }

    mb->pop_origin();
    auto origin = mb->get_next_origin();
    
    BDD::Node_ptr next = origin != nullptr ? origin->root : nullptr;
    if(next == nullptr) {
      next = Clone::Infrastructure::get_drop(nullptr, -2); //TODO: fix ids
    }

    auto new_if_module = std::make_shared<If>(node);
    auto new_then_module = std::make_shared<Then>(node);
    auto new_else_module = std::make_shared<Else>(node);

    auto if_leaf = ExecutionPlan::leaf_t(new_if_module, nullptr, ep.get_current_target());
    auto then_leaf =
        ExecutionPlan::leaf_t(new_then_module, node, ep.get_current_target());
    auto else_leaf =
        ExecutionPlan::leaf_t(new_else_module, next, ep.get_current_target());

    std::vector<ExecutionPlan::leaf_t> if_leaves{if_leaf};
    std::vector<ExecutionPlan::leaf_t> then_else_leaves{then_leaf, else_leaf};

    auto ep_if = ep.add_leaves(if_leaves, false, false);
    auto ep_if_then_else = ep_if.add_leaves(then_else_leaves, false, false);

    result.module = new_if_module;
    result.next_eps.push_back(ep_if_then_else);

    return result;
  }

public:
 virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new If(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() != type;
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse