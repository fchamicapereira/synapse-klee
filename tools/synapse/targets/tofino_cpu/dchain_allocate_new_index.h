#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class DchainAllocateNewIndex : public TofinoCPUModule {
private:
  addr_t dchain_addr;
  klee::ref<klee::Expr> time;
  klee::ref<klee::Expr> index_out;
  std::optional<symbol_t> out_of_space;

public:
  DchainAllocateNewIndex(const bdd::Node *node, addr_t _dchain_addr,
                         klee::ref<klee::Expr> _time,
                         klee::ref<klee::Expr> _index_out,
                         const symbol_t &_out_of_space)
      : TofinoCPUModule(ModuleType::TofinoCPU_DchainAllocateNewIndex,
                        "DchainAllocate", node),
        dchain_addr(_dchain_addr), time(_time), index_out(_index_out),
        out_of_space(_out_of_space) {}

  DchainAllocateNewIndex(const bdd::Node *node, addr_t _dchain_addr,
                         klee::ref<klee::Expr> _time,
                         klee::ref<klee::Expr> _index_out)
      : TofinoCPUModule(ModuleType::TofinoCPU_DchainAllocateNewIndex,
                        "DchainAllocate", node),
        dchain_addr(_dchain_addr), time(_time), index_out(_index_out),
        out_of_space(std::nullopt) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned;

    if (out_of_space.has_value()) {
      cloned = new DchainAllocateNewIndex(node, dchain_addr, time, index_out,
                                          *out_of_space);
    } else {
      cloned = new DchainAllocateNewIndex(node, dchain_addr, time, index_out);
    }

    return cloned;
  }

  const addr_t &get_dchain_addr() const { return dchain_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
  klee::ref<klee::Expr> get_index_out() const { return index_out; }

  const std::optional<symbol_t> &get_out_of_space() const {
    return out_of_space;
  }
};

class DchainAllocateNewIndexGenerator : public TofinoCPUModuleGenerator {
public:
  DchainAllocateNewIndexGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_DchainAllocateNewIndex,
                                 "DchainAllocateNewIndex") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "dchain_allocate_new_index") {
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

    if (call.function_name != "dchain_allocate_new_index") {
      return products;
    }

    if (!can_place(ep, call_node, "chain",
                   PlacementDecision::TofinoCPU_Dchain)) {
      return products;
    }

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> time = call.args.at("time").expr;
    klee::ref<klee::Expr> index_out = call.args.at("index_out").out;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t out_of_space;
    bool found = get_symbol(symbols, "out_of_space", out_of_space);

    Module *module;
    if (found) {
      module = new DchainAllocateNewIndex(node, dchain_addr, time, index_out,
                                          out_of_space);
    } else {
      module = new DchainAllocateNewIndex(node, dchain_addr, time, index_out);
    }

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
