#pragma once

#include "int_allocator_operation.h"

namespace synapse {
namespace targets {
namespace tofino {

class IntegerAllocatorAllocate : public IntegerAllocatorOperation {
public:
  IntegerAllocatorAllocate()
      : IntegerAllocatorOperation(ModuleType::Tofino_IntegerAllocatorAllocate,
                                  "IntegerAllocatorAllocate") {}

  IntegerAllocatorAllocate(bdd::Node_ptr node,
                           IntegerAllocatorRef _int_allocator)
      : IntegerAllocatorOperation(ModuleType::Tofino_IntegerAllocatorAllocate,
                                  "IntegerAllocatorAllocate", node,
                                  _int_allocator) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "dchain_allocate_new_index") {
      return result;
    }

    assert(!call.args["chain"].expr.isNull());
    assert(!call.args["time"].expr.isNull());
    assert(!call.args["index_out"].out.isNull());
    assert(!call.ret.isNull());

    auto _dchain = call.args["chain"].expr;
    auto _time = call.args["time"].expr;
    auto _index_out = call.args["index_out"].out;
    auto _success = call.ret;

    auto _generated_symbols = casted->get_locally_generated_symbols();
    auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);

    auto _out_of_space = get_symbol(_generated_symbols, "out_of_space");

    IntegerAllocatorRef _int_allocator;

    if (!can_place(ep, _dchain_addr, _int_allocator)) {
      return result;
    }

    if (operations_already_done(
            ep, _int_allocator,
            {ModuleType::Tofino_IntegerAllocatorAllocate})) {
      return result;
    }

    if (!_int_allocator) {
      auto dchain_config = bdd::get_dchain_config(ep.get_bdd(), _dchain_addr);
      auto _capacity = dchain_config.index_range;
      _int_allocator = IntegerAllocator::build(
          _index_out, _out_of_space, _capacity, _dchain_addr, {node->get_id()});
    } else {
      _int_allocator->add_integer(_index_out);
      _int_allocator->add_out_of_space(_out_of_space);
      _int_allocator->add_nodes({node->get_id()});
    }

    auto new_module =
        std::make_shared<IntegerAllocatorAllocate>(node, _int_allocator);
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
    auto cloned = new IntegerAllocatorAllocate(node, int_allocator);
    return std::shared_ptr<Module>(cloned);
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
