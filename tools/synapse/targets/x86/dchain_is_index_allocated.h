#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class DchainIsIndexAllocated : public x86Module {
private:
  addr_t dchain_addr;
  klee::ref<klee::Expr> index;
  symbol_t is_allocated;

public:
  DchainIsIndexAllocated()
      : x86Module(ModuleType::x86_DchainIsIndexAllocated,
                  "DchainIsIndexAllocated") {}

  DchainIsIndexAllocated(const bdd::Node *node, addr_t _dchain_addr,
                         klee::ref<klee::Expr> _index,
                         const symbol_t &_is_allocated)
      : x86Module(ModuleType::x86_DchainIsIndexAllocated,
                  "DchainIsIndexAllocated", node),
        dchain_addr(_dchain_addr), index(_index), is_allocated(_is_allocated) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "dchain_is_index_allocated") {
      assert(!call.args["chain"].expr.isNull());
      assert(!call.args["index"].expr.isNull());
      assert(!call.ret.isNull());

      auto _dchain = call.args["chain"].expr;
      auto _index = call.args["index"].expr;

      auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);
      auto _generated_symbols = casted->get_locally_generated_symbols();
      symbol_t _is_allocated;
      auto success = get_symbol(_generated_symbols, "dchain_is_index_allocated",
                                _is_allocated);
      assert(success && "Symbol dchain_is_index_allocated not found");

      save_dchain(ep, _dchain_addr);

      auto new_module = std::make_shared<DchainIsIndexAllocated>(
          node, _dchain_addr, _index, _is_allocated);
      auto new_ep = ep.process_leaf(new_module, node->get_next());

      result.module = new_module;
      result.next_eps.push_back(new_ep);
    }

    return result;
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned =
        new DchainIsIndexAllocated(node, dchain_addr, index, is_allocated);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const DchainIsIndexAllocated *>(other);

    if (dchain_addr != other_cast->get_dchain_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            index, other_cast->get_index())) {
      return false;
    }

    if (is_allocated.array->name !=
        other_cast->get_is_allocated().array->name) {
      return false;
    }

    return true;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  const klee::ref<klee::Expr> &get_index() const { return index; }
  const symbol_t &get_is_allocated() const { return is_allocated; }
};

} // namespace x86
} // namespace synapse
