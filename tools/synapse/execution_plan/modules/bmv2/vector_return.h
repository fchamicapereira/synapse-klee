#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace bmv2 {

class VectorReturn : public Module {
public:
  VectorReturn()
      : Module(ModuleType::BMv2_VectorReturn, TargetType::BMv2,
               "VectorReturn") {}

  VectorReturn(BDD::Node_ptr node)
      : Module(ModuleType::BMv2_VectorReturn, TargetType::BMv2, "VectorReturn",
               node) {}

private:
  call_t get_previous_vector_borrow(const BDD::Node *node,
                                    klee::ref<klee::Expr> wanted_vector) const {
    while (node->get_prev()) {
      node = node->get_prev().get();

      if (node->get_type() != BDD::Node::NodeType::CALL) {
        continue;
      }

      auto call_node = static_cast<const BDD::Call *>(node);
      auto call = call_node->get_call();

      if (call.function_name != BDD::symbex::FN_VECTOR_BORROW) {
        continue;
      }

      auto vector = call.args[BDD::symbex::FN_VECTOR_ARG_VECTOR].expr;
      auto eq =
          kutil::solver_toolbox.are_exprs_always_equal(vector, wanted_vector);

      if (eq) {
        return call;
      }
    }

    assert(false);
    exit(1);
  }

  bool modifies_cell(const BDD::Call *node) const {
    auto call = node->get_call();
    assert(call.function_name == BDD::symbex::FN_VECTOR_RETURN);

    assert(!call.args[BDD::symbex::FN_VECTOR_ARG_VECTOR].expr.isNull());
    assert(!call.args[BDD::symbex::FN_VECTOR_ARG_VALUE].in.isNull());
    auto vector = call.args[BDD::symbex::FN_VECTOR_ARG_VECTOR].expr;
    auto cell_after = call.args[BDD::symbex::FN_VECTOR_ARG_VALUE].in;

    auto vector_borrow = get_previous_vector_borrow(node, vector);
    auto cell_before = vector_borrow.extra_vars[BDD::symbex::FN_VECTOR_EXTRA].second;

    assert(cell_before->getWidth() == cell_after->getWidth());
    auto eq =
        kutil::solver_toolbox.are_exprs_always_equal(cell_before, cell_after);

    return !eq;
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != BDD::symbex::FN_VECTOR_RETURN) {
      return result;
    }

    if (modifies_cell(casted)) {
      return result;
    }

    auto new_module = std::make_shared<VectorReturn>(node);
    auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::BMv2);

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
    auto cloned = new VectorReturn(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace bmv2
} // namespace targets
} // namespace synapse
