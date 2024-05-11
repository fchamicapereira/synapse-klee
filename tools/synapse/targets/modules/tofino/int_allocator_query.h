#pragma once

#include "int_allocator_operation.h"

namespace synapse {
namespace targets {
namespace tofino {

class IntegerAllocatorQuery : public IntegerAllocatorOperation {
private:
  klee::ref<klee::Expr> index;
  bdd::symbol_t is_allocated;

public:
  IntegerAllocatorQuery()
      : IntegerAllocatorOperation(ModuleType::Tofino_IntegerAllocatorQuery,
                                  "IntegerAllocatorQuery") {}

  IntegerAllocatorQuery(bdd::Node_ptr node, IntegerAllocatorRef _int_allocator,
                        klee::ref<klee::Expr> _index,
                        const bdd::symbol_t &_is_allocated)
      : IntegerAllocatorOperation(ModuleType::Tofino_IntegerAllocatorQuery,
                                  "IntegerAllocatorQuery", node,
                                  _int_allocator),
        index(_index), is_allocated(_is_allocated) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "dchain_is_index_allocated") {
      return result;
    }

    assert(!call.args["chain"].expr.isNull());
    assert(!call.args["index"].expr.isNull());
    assert(!call.ret.isNull());

    auto _dchain = call.args["chain"].expr;
    auto _index = call.args["index"].expr;
    auto _generated_symbols = casted->get_locally_generated_symbols();

    auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);
    auto _is_allocated =
        get_symbol(_generated_symbols, "dchain_is_index_allocated");

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
      auto dchain_config = bdd::get_dchain_config(ep.get_bdd(), _dchain_addr);
      auto _capacity = dchain_config.index_range;
      _int_allocator =
          IntegerAllocator::build(_capacity, _dchain_addr, {node->get_id()});
    } else {
      _int_allocator->add_nodes({node->get_id()});
    }

    auto new_module = std::make_shared<IntegerAllocatorQuery>(
        node, _int_allocator, _index, _is_allocated);
    auto new_ep = ep.add_leaves(new_module, node->get_next());

    save_decision(new_ep, _int_allocator);

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
    auto cloned =
        new IntegerAllocatorQuery(node, int_allocator, index, is_allocated);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (!IntegerAllocatorOperation::equals(other)) {
      return false;
    }

    auto other_cast = static_cast<const IntegerAllocatorQuery *>(other);

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            index, other_cast->get_index())) {
      return false;
    }

    if (is_allocated.label != other_cast->get_is_allocated().label) {
      return false;
    }

    return true;
  }

  klee::ref<klee::Expr> get_index() const { return index; }
  const bdd::symbol_t &get_is_allocated() const { return is_allocated; }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
