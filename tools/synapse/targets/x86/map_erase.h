#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class MapErase : public x86Module {
private:
  addr_t map_addr;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> trash;

public:
  MapErase(const bdd::Node *node, addr_t _map_addr, klee::ref<klee::Expr> _key,
           klee::ref<klee::Expr> _trash)
      : x86Module(ModuleType::x86_MapErase, "MapErase", node),
        map_addr(_map_addr), key(_key), trash(_trash) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new MapErase(node, map_addr, key, trash);
    return cloned;
  }

  addr_t get_map_addr() const { return map_addr; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_trash() const { return trash; }
};

class MapEraseGenerator : public x86ModuleGenerator {
public:
  MapEraseGenerator()
      : x86ModuleGenerator(ModuleType::x86_MapErase, "MapErase") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_erase") {
      return new_eps;
    }

    if (!can_place(ep, call_node, "map", PlacementDecision::x86_Map)) {
      return new_eps;
    }

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    klee::ref<klee::Expr> trash = call.args.at("trash").out;

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);

    Module *module = new MapErase(node, map_addr, key, trash);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, map_addr, PlacementDecision::x86_Map);

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
