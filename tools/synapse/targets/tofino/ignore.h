#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class Ignore : public TofinoModule {
public:
  Ignore(const bdd::Node *node)
      : TofinoModule(ModuleType::Tofino_Ignore, "Ignore", node) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Ignore *cloned = new Ignore(node);
    return cloned;
  }
};

class IgnoreGenerator : public TofinoModuleGenerator {
private:
  std::unordered_set<std::string> functions_to_always_ignore = {
      "expire_items_single_map",
      "nf_set_rte_ipv4_udptcp_checksum",
  };

public:
  IgnoreGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_Ignore, "Ignore") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (!should_ignore(ep, node)) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (!should_ignore(ep, node)) {
      return products;
    }

    Module *module = new Ignore(node);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }

private:
  bool should_ignore(const EP *ep, const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (functions_to_always_ignore.find(call.function_name) !=
        functions_to_always_ignore.end()) {
      return true;
    }

    const Context &ctx = ep->get_ctx();
    if (call.function_name == "dchain_rejuvenate_index") {
      return can_ignore_dchain_op(ctx, call);
    }

    if (call.function_name == "vector_return") {
      return is_vector_return_without_modifications(ep, call_node);
    }

    return false;
  }

  // We can ignore dchain_rejuvenate_index if the dchain is only used for
  // linking a map with vectors. It doesn't even matter if the data structures
  // are coalesced or not, we can freely ignore it regardless.
  bool can_ignore_dchain_op(const Context &ctx, const call_t &call) const {
    assert(call.function_name == "dchain_rejuvenate_index");

    klee::ref<klee::Expr> chain = call.args.at("chain").expr;
    addr_t chain_addr = kutil::expr_addr_to_obj_addr(chain);

    std::optional<map_coalescing_data_t> coalescing_data =
        ctx.get_coalescing_data(chain_addr);

    if (!coalescing_data.has_value()) {
      return false;
    }

    // These are the data structures that can perform rejuvenations.
    if (!ctx.check_placement(coalescing_data->map,
                             PlacementDecision::Tofino_SimpleTable) &&
        !ctx.check_placement(coalescing_data->map,
                             PlacementDecision::Tofino_TTLCachedTable)) {
      return false;
    }

    return true;
  }
};

} // namespace tofino
} // namespace synapse
