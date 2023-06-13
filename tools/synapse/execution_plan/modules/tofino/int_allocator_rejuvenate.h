#pragma once

#include "int_allocator_operation.h"

namespace synapse {
namespace targets {
namespace tofino {

class IntegerAllocatorRejuvenate : public IntegerAllocatorOperation {
private:
  klee::ref<klee::Expr> index;

public:
  IntegerAllocatorRejuvenate()
      : IntegerAllocatorOperation(ModuleType::Tofino_IntegerAllocatorRejuvenate,
                                  "IntegerAllocatorRejuvenate") {}

  IntegerAllocatorRejuvenate(BDD::Node_ptr node,
                             IntegerAllocatorRef _int_allocator,
                             klee::ref<klee::Expr> _index)
      : IntegerAllocatorOperation(ModuleType::Tofino_IntegerAllocatorRejuvenate,
                                  "IntegerAllocatorRejuvenate", node,
                                  _int_allocator),
        index(_index) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != BDD::symbex::FN_DCHAIN_REJUVENATE) {
      return result;
    }

    assert(!call.args[BDD::symbex::FN_DCHAIN_ARG_CHAIN].expr.isNull());
    assert(!call.args[BDD::symbex::FN_DCHAIN_ARG_INDEX].expr.isNull());
    assert(!call.args[BDD::symbex::FN_DCHAIN_ARG_TIME].expr.isNull());

    auto _dchain = call.args[BDD::symbex::FN_DCHAIN_ARG_CHAIN].expr;
    auto _index = call.args[BDD::symbex::FN_DCHAIN_ARG_INDEX].expr;
    auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);

    IntegerAllocatorRef _int_allocator;

    if (!can_place(ep, _dchain_addr, _int_allocator)) {
      return result;
    }

    if (operations_already_done(
            ep, _int_allocator,
            {ModuleType::Tofino_IntegerAllocatorQuery,
             ModuleType::Tofino_IntegerAllocatorRejuvenate})) {
      return result;
    }

    if (!_int_allocator) {
      auto dchain_config =
          BDD::symbex::get_dchain_config(ep.get_bdd(), _dchain_addr);
      auto _capacity = dchain_config.index_range;
      _int_allocator =
          IntegerAllocator::build(_capacity, _dchain_addr, {node->get_id()});
    } else {
      _int_allocator->add_nodes({node->get_id()});
    }

    auto new_module = std::make_shared<IntegerAllocatorRejuvenate>(
        node, _int_allocator, _index);

    result = postpone(ep, node, new_module);
    assert(result.next_eps.size() == 1);

    save_decision(*result.next_eps.begin(), _int_allocator);

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new IntegerAllocatorRejuvenate(node, int_allocator, index);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (!IntegerAllocatorOperation::equals(other)) {
      return false;
    }

    auto other_cast = static_cast<const IntegerAllocatorRejuvenate *>(other);

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            index, other_cast->get_index())) {
      return false;
    }

    return true;
  }

  klee::ref<klee::Expr> get_index() const { return index; }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
