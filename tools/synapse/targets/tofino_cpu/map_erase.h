#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class MapErase : public TofinoCPUModule {
private:
  addr_t map_addr;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> trash;

public:
  MapErase(const bdd::Node *node, addr_t _map_addr, klee::ref<klee::Expr> _key,
           klee::ref<klee::Expr> _trash)
      : TofinoCPUModule(ModuleType::TofinoCPU_MapErase, "MapErase", node),
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

class MapEraseGenerator : public TofinoCPUModuleGenerator {
public:
  MapEraseGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_MapErase, "MapErase") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_erase") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "map", PlacementDecision::TofinoCPU_Map)) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_erase") {
      return products;
    }

    if (!can_place(ep, call_node, "map", PlacementDecision::TofinoCPU_Map)) {
      return products;
    }

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    klee::ref<klee::Expr> trash = call.args.at("trash").out;

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);

    Module *module = new MapErase(node, map_addr, key, trash);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, map_addr, PlacementDecision::TofinoCPU_Map);

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
