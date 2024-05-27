#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class ExpireItemsSingleMap : public x86Module {
private:
  addr_t dchain_addr;
  addr_t vector_addr;
  addr_t map_addr;
  klee::ref<klee::Expr> time;
  klee::ref<klee::Expr> number_of_freed_flows;

public:
  ExpireItemsSingleMap()
      : x86Module(ModuleType::x86_ExpireItemsSingleMap,
                  "ExpireItemsSingleMap") {}

  ExpireItemsSingleMap(const bdd::Node *node, addr_t _dchain_addr,
                       addr_t _vector_addr, addr_t _map_addr,
                       klee::ref<klee::Expr> _time,
                       klee::ref<klee::Expr> _number_of_freed_flows)
      : x86Module(ModuleType::x86_ExpireItemsSingleMap, "ExpireItemsSingleMap",
                  node),
        dchain_addr(_dchain_addr), vector_addr(_vector_addr),
        map_addr(_map_addr), time(_time),
        number_of_freed_flows(_number_of_freed_flows) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "expire_items_single_map") {
      assert(!call.args["chain"].expr.isNull());
      assert(!call.args["vector"].expr.isNull());
      assert(!call.args["map"].expr.isNull());
      assert(!call.args["time"].expr.isNull());
      assert(!call.ret.isNull());

      auto _dchain = call.args["chain"].expr;
      auto _vector = call.args["vector"].expr;
      auto _map = call.args["map"].expr;
      auto _time = call.args["time"].expr;
      auto _number_of_freed_flows = call.ret;

      auto _map_addr = kutil::expr_addr_to_obj_addr(_map);
      auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);
      auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);

      auto new_module = std::make_shared<ExpireItemsSingleMap>(
          node, _dchain_addr, _vector_addr, _map_addr, _time,
          _number_of_freed_flows);
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
    auto cloned = new ExpireItemsSingleMap(
        node, dchain_addr, map_addr, vector_addr, time, number_of_freed_flows);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const ExpireItemsSingleMap *>(other);

    if (dchain_addr != other_cast->get_dchain_addr()) {
      return false;
    }

    if (vector_addr != other_cast->get_vector_addr()) {
      return false;
    }

    if (map_addr != other_cast->get_map_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(time,
                                                      other_cast->get_time())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            number_of_freed_flows, other_cast->get_number_of_freed_flows())) {
      return false;
    }

    return true;
  }

  addr_t get_dchain_addr() const { return dchain_addr; }
  addr_t get_vector_addr() const { return vector_addr; }
  addr_t get_map_addr() const { return map_addr; }
  const klee::ref<klee::Expr> &get_time() const { return time; }
  const klee::ref<klee::Expr> &get_number_of_freed_flows() const {
    return number_of_freed_flows;
  }
};

} // namespace x86
} // namespace synapse
