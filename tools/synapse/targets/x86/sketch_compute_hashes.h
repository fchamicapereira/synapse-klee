#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SketchComputeHashes : public x86Module {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> key;

public:
  SketchComputeHashes()
      : x86Module(ModuleType::x86_SketchComputeHashes, "SketchComputeHashes") {}

  SketchComputeHashes(const bdd::Node *node, addr_t _sketch_addr,
                      klee::ref<klee::Expr> _key)
      : x86Module(ModuleType::x86_SketchComputeHashes, "SketchComputeHashes",
                  node),
        sketch_addr(_sketch_addr), key(_key) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "sketch_compute_hashes") {
      return result;
    }

    assert(!call.args["sketch"].expr.isNull());
    assert(!call.args["key"].expr.isNull());

    auto _sketch = call.args["sketch"].expr;
    auto _key = call.args["key"].expr;

    auto _sketch_addr = kutil::expr_addr_to_obj_addr(_sketch);

    save_sketch(ep, _sketch_addr);

    auto new_module =
        std::make_shared<SketchComputeHashes>(node, _sketch_addr, _key);
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
    auto cloned = new SketchComputeHashes(node, sketch_addr, key);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SketchComputeHashes *>(other);

    if (sketch_addr != other_cast->get_sketch_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(key,
                                                      other_cast->get_key())) {
      return false;
    }

    return true;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_key() const { return key; }
};

} // namespace x86
} // namespace synapse
