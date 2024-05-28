#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class DchainFreeIndex : public x86Module {
private:
  addr_t dchain_addr;
  klee::ref<klee::Expr> index;

public:
  DchainFreeIndex()
      : x86Module(ModuleType::x86_DchainFreeIndex, "DchainFreeIndex") {}

  DchainFreeIndex(const bdd::Node *node, addr_t _dchain_addr,
                  klee::ref<klee::Expr> _index)
      : x86Module(ModuleType::x86_DchainFreeIndex, "DchainFreeIndex", node),
        dchain_addr(_dchain_addr), index(_index) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "dchain_free_index") {
      assert(!call.args["chain"].expr.isNull());
      assert(!call.args["index"].expr.isNull());

      auto _dchain = call.args["chain"].expr;
      auto _index = call.args["index"].expr;

      auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);
      save_dchain(ep, _dchain_addr);

      auto new_module =
          std::make_shared<DchainFreeIndex>(node, _dchain_addr, _index);
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
    auto cloned = new DchainFreeIndex(node, dchain_addr, index);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const DchainFreeIndex *>(other);

    if (dchain_addr != other_cast->get_dchain_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            index, other_cast->get_index())) {
      return false;
    }

    return true;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  const klee::ref<klee::Expr> &get_index() const { return index; }
};

} // namespace x86
} // namespace synapse
