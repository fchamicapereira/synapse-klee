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
  MapPut(const bdd::Node *node, addr_t _map_addr, addr_t _key_addr,
         klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _value)
      : x86Module(ModuleType::x86_MapPut, "MapPut", node), map_addr(_map_addr),
        key_addr(_key_addr), key(_key), value(_value) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new MapPut(node, map_addr, key_addr, key, value);
    return cloned;
  }

  addr_t get_map_addr() const { return map_addr; }
  addr_t get_key_addr() const { return key_addr; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_value() const { return value; }
};

class MapPutGenerator : public x86ModuleGenerator {
public:
  MapPutGenerator() : x86ModuleGenerator(ModuleType::x86_MapPut, "MapPut") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_put") {
      return new_eps;
    }

    if (!can_place(ep, call_node, "map", PlacementDecision::x86Map)) {
      return new_eps;
    }

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key_addr_expr = call.args.at("key").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    klee::ref<klee::Expr> value = call.args.at("value").expr;

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);
    addr_t key_addr = kutil::expr_addr_to_obj_addr(key_addr_expr);

    Module *module = new MapPut(node, map_addr, key_addr, key, value);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, map_addr, PlacementDecision::x86Map);

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
