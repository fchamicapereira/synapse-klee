#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class ModifyHeader : public TofinoModule {
private:
  addr_t hdr_addr;
  klee::ref<klee::Expr> original_hdr;
  std::vector<modification_t> changes;

public:
  ModifyHeader(const bdd::Node *node, addr_t _hdr_addr,
               klee::ref<klee::Expr> _original_hdr,
               const std::vector<modification_t> &_changes)
      : TofinoModule(ModuleType::Tofino_ModifyHeader, "ModifyHeader", node),
        hdr_addr(_hdr_addr), original_hdr(_original_hdr), changes(_changes) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ModifyHeader *cloned =
        new ModifyHeader(node, hdr_addr, original_hdr, changes);
    return cloned;
  }

  addr_t get_hdr_addr() const { return hdr_addr; }
  klee::ref<klee::Expr> get_original_hdr() const { return original_hdr; }
  const std::vector<modification_t> &get_changes() const { return changes; }
};

class ModifyHeaderGenerator : public TofinoModuleGenerator {
public:
  ModifyHeaderGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_ModifyHeader, "ModifyHeader") {
  }

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "packet_return_chunk") {
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

    if (call.function_name != "packet_return_chunk") {
      return products;
    }

    klee::ref<klee::Expr> hdr = call.args.at("the_chunk").expr;
    klee::ref<klee::Expr> current_hdr = call.args.at("the_chunk").in;
    klee::ref<klee::Expr> original_hdr = chunk_borrow_from_return(ep, node);

    addr_t hdr_addr = kutil::expr_addr_to_obj_addr(hdr);

    std::vector<modification_t> changes =
        build_modifications(original_hdr, current_hdr);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    if (changes.size() == 0) {
      new_ep->process_leaf(node->get_next());
      return products;
    }

    Module *module = new ModifyHeader(node, hdr_addr, original_hdr, changes);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace tofino
} // namespace synapse