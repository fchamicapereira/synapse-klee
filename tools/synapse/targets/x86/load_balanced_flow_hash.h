#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class LoadBalancedFlowHash : public x86Module {
private:
  klee::ref<klee::Expr> obj;
  symbol_t hash;

public:
  LoadBalancedFlowHash()
      : x86Module(ModuleType::x86_LoadBalancedFlowHash,
                  "LoadBalancedFlowHash") {}

  LoadBalancedFlowHash(const bdd::Node *node, klee::ref<klee::Expr> _obj,
                       symbol_t _hash)
      : x86Module(ModuleType::x86_LoadBalancedFlowHash, "LoadBalancedFlowHash",
                  node),
        obj(_obj), hash(_hash) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

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
    symbol_t _hash;
    auto success =
        get_symbol(_generated_symbols, "LoadBalancedFlow_hash", _hash);
    assert(success && "Symbol LoadBalancedFlow_hash not found");

    auto new_module = std::make_shared<LoadBalancedFlowHash>(node, _obj, _hash);
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

    if (hash.array->name != other_cast->get_hash().array->name) {
      return false;
    }

    return true;
  }

  klee::ref<klee::Expr> get_obj() const { return obj; }
  const symbol_t &get_hash() const { return hash; }
};

} // namespace x86
} // namespace synapse
