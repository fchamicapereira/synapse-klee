#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class ModifyHeader : public TofinoCPUModule {
private:
  addr_t chunk_addr;
  klee::ref<klee::Expr> original_chunk;
  std::vector<modification_t> changes;

public:
  ModifyHeader(const bdd::Node *node, addr_t _chunk_addr,
               const std::vector<modification_t> &_changes)
      : TofinoCPUModule(ModuleType::TofinoCPU_ModifyHeader, "ModifyHeader",
                        node),
        chunk_addr(_chunk_addr), changes(_changes) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ModifyHeader *cloned = new ModifyHeader(node, chunk_addr, changes);
    return cloned;
  }

  addr_t get_chunk_addr() const { return chunk_addr; }
  const std::vector<modification_t> &get_changes() const { return changes; }
};

class ModifyHeaderGenerator : public TofinoCPUModuleGenerator {
public:
  ModifyHeaderGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_ModifyHeader,
                                 "ModifyHeader") {}

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

    const bdd::Call *packet_return_chunk = static_cast<const bdd::Call *>(node);
    const call_t &call = packet_return_chunk->get_call();

    if (call.function_name != "packet_return_chunk") {
      return products;
    }

    const bdd::Call *packet_borrow_chunk =
        packet_borrow_from_return(ep, packet_return_chunk);
    assert(packet_borrow_chunk &&
           "Failed to find packet_borrow_next_chunk from packet_return_chunk");

    klee::ref<klee::Expr> hdr = call.args.at("the_chunk").expr;
    addr_t hdr_addr = kutil::expr_addr_to_obj_addr(hdr);

    std::vector<modification_t> changes =
        build_hdr_modifications(packet_borrow_chunk, packet_return_chunk);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    if (changes.size() == 0) {
      new_ep->process_leaf(node->get_next());
      return products;
    }

    Module *module = new ModifyHeader(node, hdr_addr, changes);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
