#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class DchainFreeIndex : public TofinoCPUModule {
private:
  addr_t dchain_addr;
  klee::ref<klee::Expr> index;

public:
  DchainFreeIndex(const bdd::Node *node, addr_t _dchain_addr,
                  klee::ref<klee::Expr> _index)
      : TofinoCPUModule(ModuleType::TofinoCPU_DchainFreeIndex,
                        "DchainFreeIndex", node),
        dchain_addr(_dchain_addr), index(_index) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new DchainFreeIndex(node, dchain_addr, index);
    return cloned;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  klee::ref<klee::Expr> get_index() const { return index; }
};

class DchainFreeIndexGenerator : public TofinoCPUModuleGenerator {
public:
  DchainFreeIndexGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_DchainFreeIndex,
                                 "DchainFreeIndex") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "dchain_free_index") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "chain",
                   PlacementDecision::TofinoCPU_Dchain)) {
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

    if (call.function_name != "dchain_free_index") {
      return products;
    }

    if (!can_place(ep, call_node, "chain",
                   PlacementDecision::TofinoCPU_Dchain)) {
      return products;
    }

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    Module *module = new DchainFreeIndex(node, dchain_addr, index);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, dchain_addr, PlacementDecision::TofinoCPU_Dchain);

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
