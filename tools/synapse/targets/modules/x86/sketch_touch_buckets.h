#pragma once

#include "x86_module.h"

namespace synapse {
namespace targets {
namespace x86 {

class SketchTouchBuckets : public x86Module {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> time;
  bdd::symbol_t success;

public:
  SketchTouchBuckets()
      : x86Module(ModuleType::x86_SketchTouchBuckets, "SketchTouchBuckets") {}

  SketchTouchBuckets(bdd::Node_ptr node, addr_t _sketch_addr,
                     klee::ref<klee::Expr> _time, bdd::symbol_t _success)
      : x86Module(ModuleType::x86_SketchTouchBuckets, "SketchTouchBuckets",
                  node),
        sketch_addr(_sketch_addr), time(_time), success(_success) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "sketch_touch_buckets") {
      return result;
    }

    assert(!call.args["sketch"].expr.isNull());
    assert(!call.args["time"].expr.isNull());

    auto _sketch = call.args["sketch"].expr;
    auto _time = call.args["time"].expr;

    auto _sketch_addr = kutil::expr_addr_to_obj_addr(_sketch);

    auto _generated_symbols = casted->get_locally_generated_symbols();
    auto _success = bdd::get_symbol(_generated_symbols, "success");

    save_sketch(ep, _sketch_addr);

    auto new_module = std::make_shared<SketchTouchBuckets>(node, _sketch_addr,
                                                           _time, _success);
    auto new_ep = ep.add_leaves(new_module, node->get_next());

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
    auto cloned = new SketchTouchBuckets(node, sketch_addr, time, success);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SketchTouchBuckets *>(other);

    if (sketch_addr != other_cast->get_sketch_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(time,
                                                      other_cast->get_time())) {
      return false;
    }

    if (success.label != other_cast->get_success().label) {
      return false;
    }

    return true;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
  const bdd::symbol_t &get_success() const { return success; }
};
} // namespace x86
} // namespace targets
} // namespace synapse
