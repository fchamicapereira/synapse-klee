#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class ParserExtraction : public TofinoModule {
private:
  addr_t hdr_addr;
  klee::ref<klee::Expr> hdr;
  bytes_t length;

public:
  ParserExtraction(const bdd::Node *node, addr_t _hdr_addr,
                   klee::ref<klee::Expr> _hdr, bytes_t _length)
      : TofinoModule(ModuleType::Tofino_ParserExtraction, "ParserExtraction",
                     node),
        hdr_addr(_hdr_addr), hdr(_hdr), length(_length) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ParserExtraction *cloned =
        new ParserExtraction(node, hdr_addr, hdr, length);
    return cloned;
  }

  addr_t get_hdr_addr() const { return hdr_addr; }
  klee::ref<klee::Expr> get_hdr() const { return hdr; }
  bytes_t get_length() const { return length; }
};

class ParserExtractionGenerator : public TofinoModuleGenerator {
public:
  ParserExtractionGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_ParserExtraction,
                              "ParserExtraction") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "packet_borrow_next_chunk") {
      return std::nullopt;
    }

    return current_speculative_ctx;
  }

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

    klee::ref<klee::Expr> hdr_addr_expr = call.args.at("chunk").out;
    klee::ref<klee::Expr> hdr = call.extra_vars.at("the_chunk").second;
    klee::ref<klee::Expr> length_expr = call.args.at("length").expr;

    // Relevant for IPv4 options, but left for future work.
    assert(!borrow_has_var_len(node) && "Not implemented");
    bytes_t length = kutil::solver_toolbox.value_from_expr(length_expr);

    addr_t hdr_addr = kutil::expr_addr_to_obj_addr(hdr_addr_expr);

    Module *module = new ParserExtraction(node, hdr_addr, hdr, length);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    tofino_ctx->parser_transition(ep, node, hdr);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace tofino
} // namespace synapse
