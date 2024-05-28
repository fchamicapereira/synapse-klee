#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SketchExpire : public x86Module {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> time;

public:
  SketchExpire() : x86Module(ModuleType::x86_SketchExpire, "SketchExpire") {}

  SketchExpire(const bdd::Node *node, addr_t _sketch_addr,
               klee::ref<klee::Expr> _time)
      : x86Module(ModuleType::x86_SketchExpire, "SketchExpire", node),
        sketch_addr(_sketch_addr), time(_time) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "sketch_expire") {
      return result;
    }

    assert(!call.args["sketch"].expr.isNull());
    assert(!call.args["time"].expr.isNull());

    auto _sketch = call.args["sketch"].expr;
    auto _time = call.args["time"].expr;

    auto _sketch_addr = kutil::expr_addr_to_obj_addr(_sketch);

    save_sketch(ep, _sketch_addr);

    auto new_module = std::make_shared<SketchExpire>(node, _sketch_addr, _time);
    auto new_ep = ep.process_leaf(new_module, node->get_next());

    result.module = new_module;
    result.next_eps.push_back(new_ep);

    return result;
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new SketchExpire(node, sketch_addr, time);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SketchExpire *>(other);

    if (sketch_addr != other_cast->get_sketch_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(time,
                                                      other_cast->get_time())) {
      return false;
    }

    return true;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
};

} // namespace x86
} // namespace synapse
