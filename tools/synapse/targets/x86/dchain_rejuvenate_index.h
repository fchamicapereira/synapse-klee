#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class DchainRejuvenateIndex : public x86Module {
private:
  addr_t dchain_addr;
  klee::ref<klee::Expr> index;
  klee::ref<klee::Expr> time;

public:
  DchainRejuvenateIndex(const bdd::Node *node, addr_t _dchain_addr,
                        klee::ref<klee::Expr> _index,
                        klee::ref<klee::Expr> _time)
      : x86Module(ModuleType::x86_DchainRejuvenateIndex, "DchainRejuvenate",
                  node),
        dchain_addr(_dchain_addr), index(_index), time(_time) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new DchainRejuvenateIndex(node, dchain_addr, index, time);
    return cloned;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  klee::ref<klee::Expr> get_index() const { return index; }
  klee::ref<klee::Expr> get_time() const { return time; }
};

class DchainRejuvenateIndexGenerator : public x86ModuleGenerator {
public:
  DchainRejuvenateIndexGenerator()
      : x86ModuleGenerator(ModuleType::x86_DchainRejuvenateIndex,
                           "DchainRejuvenateIndex") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "dchain_rejuvenate_index") {
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
    klee::ref<klee::Expr> time = call.args.at("time").expr;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    Module *module = new DchainRejuvenateIndex(node, dchain_addr, index, time);
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
