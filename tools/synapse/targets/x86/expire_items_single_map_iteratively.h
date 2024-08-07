#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class ExpireItemsSingleMapIteratively : public x86Module {
private:
  addr_t vector_addr;
  addr_t map_addr;
  klee::ref<klee::Expr> start;
  klee::ref<klee::Expr> n_elems;

public:
  ExpireItemsSingleMapIteratively(const bdd::Node *node, addr_t _vector_addr,
                                  addr_t _map_addr,
                                  klee::ref<klee::Expr> _start,
                                  klee::ref<klee::Expr> _n_elems)
      : x86Module(ModuleType::x86_ExpireItemsSingleMapIteratively,
                  "ExpireItemsSingleMapIteratively", node),
        vector_addr(_vector_addr), map_addr(_map_addr), start(_start),
        n_elems(_n_elems) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new ExpireItemsSingleMapIteratively(
        node, map_addr, vector_addr, start, n_elems);
    return cloned;
  }

  addr_t get_vector_addr() const { return vector_addr; }
  addr_t get_map_addr() const { return map_addr; }
  klee::ref<klee::Expr> get_start() const { return start; }
  klee::ref<klee::Expr> get_n_elems() const { return n_elems; }
};

class ExpireItemsSingleMapIterativelyGenerator : public x86ModuleGenerator {
public:
  ExpireItemsSingleMapIterativelyGenerator()
      : x86ModuleGenerator(ModuleType::x86_ExpireItemsSingleMapIteratively,
                           "ExpireItemsSingleMapIteratively") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "expire_items_single_map_iteratively") {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (bdd_node_match_pattern(node))
      return ctx;
    return std::nullopt;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (!bdd_node_match_pattern(node)) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> start = call.args.at("start").expr;
    klee::ref<klee::Expr> n_elems = call.args.at("n_elems").expr;

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);
    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);

    Module *module = new ExpireItemsSingleMapIteratively(
        node, map_addr, vector_addr, start, n_elems);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace x86
} // namespace synapse
