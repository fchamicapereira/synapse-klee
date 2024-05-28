#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class DchainAllocateNewIndex : public x86Module {
private:
  addr_t dchain_addr;
  klee::ref<klee::Expr> time;
  klee::ref<klee::Expr> index_out;
  symbol_t out_of_space;

public:
  DchainAllocateNewIndex()
      : x86Module(ModuleType::x86_DchainAllocateNewIndex, "DchainAllocate") {}

  DchainAllocateNewIndex(const bdd::Node *node, addr_t _dchain_addr,
                         klee::ref<klee::Expr> _time,
                         klee::ref<klee::Expr> _index_out,
                         const symbol_t &_out_of_space)
      : x86Module(ModuleType::x86_DchainAllocateNewIndex, "DchainAllocate",
                  node),
        dchain_addr(_dchain_addr), time(_time), index_out(_index_out),
        out_of_space(_out_of_space) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "dchain_allocate_new_index") {
      assert(!call.args["chain"].expr.isNull());
      assert(!call.args["time"].expr.isNull());
      assert(!call.args["index_out"].out.isNull());
      assert(!call.ret.isNull());

      auto _dchain = call.args["chain"].expr;
      auto _time = call.args["time"].expr;
      auto _index_out = call.args["index_out"].out;

      auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);

      auto _generated_symbols = casted->get_locally_generated_symbols();
      symbol_t _out_of_space;
      auto success =
          get_symbol(_generated_symbols, "out_of_space", _out_of_space);
      assert(success && "Symbol out_of_space not found");

      save_dchain(ep, _dchain_addr);

      auto new_module = std::make_shared<DchainAllocateNewIndex>(
          node, _dchain_addr, _time, _index_out, _out_of_space);
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
    auto cloned = new DchainAllocateNewIndex(node, dchain_addr, time, index_out,
                                             out_of_space);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const DchainAllocateNewIndex *>(other);

    if (dchain_addr != other_cast->get_dchain_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(time,
                                                      other_cast->get_time())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            index_out, other_cast->get_index_out())) {
      return false;
    }

    if (out_of_space.array->name !=
        other_cast->get_out_of_space().array->name) {
      return false;
    }

    return true;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  const klee::ref<klee::Expr> &get_time() const { return time; }
  const klee::ref<klee::Expr> &get_index_out() const { return index_out; }
  const symbol_t &get_out_of_space() const { return out_of_space; }
};

} // namespace x86
} // namespace synapse
