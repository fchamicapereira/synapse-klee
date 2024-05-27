#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class MapPut : public x86Module {
private:
  addr_t map_addr;
  addr_t key_addr;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> value;

public:
  MapPut() : x86Module(ModuleType::x86_MapPut, "MapPut") {}

  MapPut(const bdd::Node *node, addr_t _map_addr, addr_t _key_addr,
         klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _value)
      : x86Module(ModuleType::x86_MapPut, "MapPut", node), map_addr(_map_addr),
        key_addr(_key_addr), key(_key), value(_value) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "map_put") {
      assert(!call.args["map"].expr.isNull());
      assert(!call.args["key"].expr.isNull());
      assert(!call.args["key"].in.isNull());
      assert(!call.args["value"].expr.isNull());

      auto _map = call.args["map"].expr;
      auto _key_addr_expr = call.args["key"].expr;
      auto _key = call.args["key"].in;
      auto _value = call.args["value"].expr;

      auto _map_addr = kutil::expr_addr_to_obj_addr(_map);
      auto _key_addr = kutil::expr_addr_to_obj_addr(_key_addr_expr);

      save_map(ep, _map_addr);

      auto new_module =
          std::make_shared<MapPut>(node, _map_addr, _key_addr, _key, _value);
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
    auto cloned = new MapPut(node, map_addr, key_addr, key, value);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const MapPut *>(other);

    if (map_addr != other_cast->get_map_addr()) {
      return false;
    }

    if (key_addr != other_cast->get_key_addr()) {
      return false;
    }
    if (!kutil::solver_toolbox.are_exprs_always_equal(key,
                                                      other_cast->get_key())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            value, other_cast->get_value())) {
      return false;
    }

    return true;
  }

  addr_t get_map_addr() const { return map_addr; }
  addr_t get_key_addr() const { return key_addr; }
  const klee::ref<klee::Expr> &get_key() const { return key; }
  const klee::ref<klee::Expr> &get_value() const { return value; }
};

} // namespace x86
} // namespace synapse
