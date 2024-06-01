#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class ParserExtraction : public x86Module {
private:
  addr_t chunk_addr;
  klee::ref<klee::Expr> chunk;
  klee::ref<klee::Expr> length;

public:
  ParserExtraction(const bdd::Node *node, addr_t _chunk_addr,
                   klee::ref<klee::Expr> _chunk, klee::ref<klee::Expr> _length)
      : x86Module(ModuleType::x86_ParserExtraction, "ParserExtraction", node),
        chunk_addr(_chunk_addr), chunk(_chunk), length(_length) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ParserExtraction *cloned =
        new ParserExtraction(node, chunk_addr, chunk, length);
    return cloned;
  }

  addr_t get_chunk_addr() const { return chunk_addr; }
  klee::ref<klee::Expr> get_chunk() const { return chunk; }
  klee::ref<klee::Expr> get_length() const { return length; }
};

class ParserExtractionGenerator : public x86ModuleGenerator {
public:
  ParserExtractionGenerator()
      : x86ModuleGenerator(ModuleType::x86_ParserExtraction,
                           "ParserExtraction") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "packet_borrow_next_chunk") {
      return new_eps;
    }

    klee::ref<klee::Expr> chunk = call.args.at("chunk").out;
    klee::ref<klee::Expr> out_chunk = call.extra_vars.at("the_chunk").second;
    klee::ref<klee::Expr> length = call.args.at("length").expr;

    addr_t chunk_addr = kutil::expr_addr_to_obj_addr(chunk);

    Module *module = new ParserExtraction(node, chunk_addr, out_chunk, length);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    if (node->get_next()) {
      EPLeaf leaf(ep_node, node->get_next());
      new_ep->process_leaf(ep_node, {leaf});
    } else {
      new_ep->process_leaf(ep_node, {});
    }

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
