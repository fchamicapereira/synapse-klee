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
  DchainIsIndexAllocated(const bdd::Node *node, addr_t _dchain_addr,
                         klee::ref<klee::Expr> _index,
                         const symbol_t &_is_allocated)
      : x86Module(ModuleType::x86_DchainIsIndexAllocated,
                  "DchainIsIndexAllocated", node),
        dchain_addr(_dchain_addr), index(_index), is_allocated(_is_allocated) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new DchainIsIndexAllocated(node, dchain_addr, index, is_allocated);
    return cloned;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  klee::ref<klee::Expr> get_index() const { return index; }
  const symbol_t &get_is_allocated() const { return is_allocated; }
};

class DchainIsIndexAllocatedGenerator : public x86ModuleGenerator {
public:
  DchainIsIndexAllocatedGenerator()
      : x86ModuleGenerator(ModuleType::x86_DchainIsIndexAllocated,
                           "DchainIsIndexAllocated") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "dchain_is_index_allocated") {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (!bdd_node_match_pattern(node)) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place(ep, call_node, "chain", PlacementDecision::x86_Dchain)) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (!bdd_node_match_pattern(node)) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (!can_place(ep, call_node, "chain", PlacementDecision::x86_Dchain)) {
      return products;
    }

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);
    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t is_allocated;
    bool found = get_symbol(symbols, "dchain_is_index_allocated", is_allocated);
    assert(found && "Symbol dchain_is_index_allocated not found");

    Module *module =
        new DchainIsIndexAllocated(node, dchain_addr, index, is_allocated);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, dchain_addr, PlacementDecision::x86_Dchain);

    return products;
  }
};

} // namespace x86
} // namespace synapse
