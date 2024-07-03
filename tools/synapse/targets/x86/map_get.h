#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class MapGet : public x86Module {
private:
  addr_t map_addr;
  addr_t key_addr;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> value_out;
  klee::ref<klee::Expr> success;
  symbol_t map_has_this_key;

public:
  MapGet(const bdd::Node *node, addr_t _map_addr, addr_t _key_addr,
         klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _value_out,
         klee::ref<klee::Expr> _success, const symbol_t &_map_has_this_key)
      : x86Module(ModuleType::x86_MapGet, "MapGet", node), map_addr(_map_addr),
        key_addr(_key_addr), key(_key), value_out(_value_out),
        success(_success), map_has_this_key(_map_has_this_key) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new MapGet(node, map_addr, key_addr, key, value_out,
                                success, map_has_this_key);
    return cloned;
  }

  addr_t get_map_addr() const { return map_addr; }
  addr_t get_key_addr() const { return key_addr; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_value_out() const { return value_out; }
  klee::ref<klee::Expr> get_success() const { return success; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }
};

class MapGetGenerator : public x86ModuleGenerator {
public:
  MapGetGenerator() : x86ModuleGenerator(ModuleType::x86_MapGet, "MapGet") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    if (!bdd_node_match_pattern(node)) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place(ep, call_node, "map", PlacementDecision::x86_Map)) {
      return std::nullopt;
    }

    return current_speculative_ctx;
  }

  virtual std::vector<generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<generator_product_t> products;

    if (!bdd_node_match_pattern(node)) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (!can_place(ep, call_node, "map", PlacementDecision::x86_Map)) {
      return products;
    }

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key_addr_expr = call.args.at("key").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    klee::ref<klee::Expr> success = call.ret;
    klee::ref<klee::Expr> value_out = call.args.at("value_out").out;

    symbols_t symbols = call_node->get_locally_generated_symbols();

    symbol_t map_has_this_key;
    bool found = get_symbol(symbols, "map_has_this_key", map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);
    addr_t key_addr = kutil::expr_addr_to_obj_addr(key_addr_expr);

    Module *module = new MapGet(node, map_addr, key_addr, key, value_out,
                                success, map_has_this_key);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, map_addr, PlacementDecision::x86_Map);

    return products;
  }
};

} // namespace x86
} // namespace synapse
