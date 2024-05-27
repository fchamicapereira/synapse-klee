#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class MapErase : public x86Module {
private:
  addr_t map_addr;
  klee::ref<klee::Expr> key;
  addr_t trash;

public:
  MapErase() : x86Module(ModuleType::x86_MapErase, "MapErase") {}

  MapErase(const bdd::Node *node, addr_t _map_addr, klee::ref<klee::Expr> _key,
           addr_t _trash)
      : x86Module(ModuleType::x86_MapErase, "MapErase", node),
        map_addr(_map_addr), key(_key), trash(_trash) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "map_erase") {
      assert(!call.args["map"].expr.isNull());
      assert(!call.args["key"].in.isNull());
      assert(!call.args["trash"].out.isNull());

      auto _map = call.args["map"].expr;
      auto _key = call.args["key"].in;
      auto _trash = call.args["trash"].out;

      auto _map_addr = kutil::expr_addr_to_obj_addr(_map);
      auto _trash_addr = kutil::expr_addr_to_obj_addr(_trash);

      save_map(ep, _map_addr);

      auto new_module =
          std::make_shared<MapErase>(node, _map_addr, _key, _trash_addr);
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
    auto cloned = new MapErase(node, map_addr, key, trash);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const MapErase *>(other);

    if (map_addr != other_cast->get_map_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(key,
                                                      other_cast->get_key())) {
      return false;
    }

    if (trash != other_cast->get_trash()) {
      return false;
    }

    return true;
  }

  addr_t get_map_addr() const { return map_addr; }
  klee::ref<klee::Expr> get_key() const { return key; }
  addr_t get_trash() const { return trash; }
};

} // namespace x86
} // namespace synapse
