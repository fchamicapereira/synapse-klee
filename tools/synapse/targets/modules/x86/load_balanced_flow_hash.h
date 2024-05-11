#pragma once

#include "x86_module.h"

namespace synapse {
namespace targets {
namespace x86 {

class LoadBalancedFlowHash : public x86Module {
private:
  klee::ref<klee::Expr> obj;
  bdd::symbol_t hash;

public:
  LoadBalancedFlowHash()
      : x86Module(ModuleType::x86_LoadBalancedFlowHash,
                  "LoadBalancedFlowHash") {}

  LoadBalancedFlowHash(bdd::Node_ptr node, klee::ref<klee::Expr> _obj,
                       bdd::symbol_t _hash)
      : x86Module(ModuleType::x86_LoadBalancedFlowHash, "LoadBalancedFlowHash",
                  node),
        obj(_obj), hash(_hash) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "LoadBalancedFlow_hash") {
      return result;
    }

    assert(!call.args["obj"].in.isNull());
    auto _obj = call.args["obj"].in;

    auto _generated_symbols = casted->get_locally_generated_symbols();
    auto _hash = bdd::get_symbol(_generated_symbols, "LoadBalancedFlow_hash");

    auto new_module = std::make_shared<LoadBalancedFlowHash>(node, _obj, _hash);
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
    auto cloned = new LoadBalancedFlowHash(node, obj, hash);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const LoadBalancedFlowHash *>(other);

    if (obj != other_cast->get_obj()) {
      return false;
    }

    if (hash.label != other_cast->get_hash().label) {
      return false;
    }

    return true;
  }

  klee::ref<klee::Expr> get_obj() const { return obj; }
  const bdd::symbol_t &get_hash() const { return hash; }
};
} // namespace x86
} // namespace targets
} // namespace synapse
