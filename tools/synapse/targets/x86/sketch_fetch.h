#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SketchFetch : public x86Module {
private:
  addr_t sketch_addr;
  symbol_t overflow;

public:
  SketchFetch() : x86Module(ModuleType::x86_SketchFetch, "SketchFetch") {}

  SketchFetch(const bdd::Node *node, addr_t _sketch_addr, symbol_t _overflow)
      : x86Module(ModuleType::x86_SketchFetch, "SketchFetch", node),
        sketch_addr(_sketch_addr), overflow(_overflow) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "sketch_fetch") {
      return result;
    }

    assert(!call.args["sketch"].expr.isNull());
    auto _sketch = call.args["sketch"].expr;
    auto _sketch_addr = kutil::expr_addr_to_obj_addr(_sketch);

    auto _generated_symbols = casted->get_locally_generated_symbols();
    symbol_t _overflow;
    auto success = get_symbol(_generated_symbols, "overflow", _overflow);
    assert(success && "Symbol overflow not found");

    save_sketch(ep, _sketch_addr);

    auto new_module =
        std::make_shared<SketchFetch>(node, _sketch_addr, _overflow);
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
    auto cloned = new SketchFetch(node, sketch_addr, overflow);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SketchFetch *>(other);

    if (sketch_addr != other_cast->get_sketch_addr()) {
      return false;
    }

    if (overflow.array->name != other_cast->get_overflow().array->name) {
      return false;
    }

    return true;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  const symbol_t &get_overflow() const { return overflow; }
};

} // namespace x86
} // namespace synapse
