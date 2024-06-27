#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class DchainAllocateNewIndex : public x86Module {
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
      : x86Module(ModuleType::x86_DchainAllocateNewIndex, "DchainAllocate",
                  node),
        dchain_addr(_dchain_addr), time(_time), index_out(_index_out),
        out_of_space(_out_of_space) {}

  DchainAllocateNewIndex(const bdd::Node *node, addr_t _dchain_addr,
                         klee::ref<klee::Expr> _time,
                         klee::ref<klee::Expr> _index_out)
      : x86Module(ModuleType::x86_DchainAllocateNewIndex, "DchainAllocate",
                  node),
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

class DchainAllocateNewIndexGenerator : public x86ModuleGenerator {
public:
  DchainAllocateNewIndexGenerator()
      : x86ModuleGenerator(ModuleType::x86_DchainAllocateNewIndex,
                           "DchainAllocateNewIndex") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "dchain_allocate_new_index") {
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

    if (!can_place(ep, call_node, "chain", PlacementDecision::x86_Dchain)) {
      return std::nullopt;
    }

    return current_speculative_ctx;
  }

  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (!bdd_node_match_pattern(node)) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (!can_place(ep, call_node, "chain", PlacementDecision::x86_Dchain)) {
      return new_eps;
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
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, dchain_addr, PlacementDecision::x86_Dchain);

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
