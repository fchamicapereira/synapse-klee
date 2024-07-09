#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class ParseHeader : public TofinoCPUModule {
private:
  addr_t chunk_addr;
  klee::ref<klee::Expr> chunk;
  klee::ref<klee::Expr> length;

public:
  ParseHeader(const bdd::Node *node, addr_t _chunk_addr,
              klee::ref<klee::Expr> _chunk, klee::ref<klee::Expr> _length)
      : TofinoCPUModule(ModuleType::TofinoCPU_ParseHeader, "ParseHeader", node),
        chunk_addr(_chunk_addr), chunk(_chunk), length(_length) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ParseHeader *cloned = new ParseHeader(node, chunk_addr, chunk, length);
    return cloned;
  }

  addr_t get_chunk_addr() const { return chunk_addr; }
  klee::ref<klee::Expr> get_chunk() const { return chunk; }
  klee::ref<klee::Expr> get_length() const { return length; }
};

class ParseHeaderGenerator : public TofinoCPUModuleGenerator {
public:
  ParseHeaderGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_ParseHeader,
                                 "ParseHeader") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "packet_borrow_next_chunk") {
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

    if (call.function_name != "packet_borrow_next_chunk") {
      return products;
    }

    klee::ref<klee::Expr> chunk = call.args.at("chunk").out;
    klee::ref<klee::Expr> out_chunk = call.extra_vars.at("the_chunk").second;
    klee::ref<klee::Expr> length = call.args.at("length").expr;

    addr_t chunk_addr = kutil::expr_addr_to_obj_addr(chunk);

    Module *module = new ParseHeader(node, chunk_addr, out_chunk, length);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
