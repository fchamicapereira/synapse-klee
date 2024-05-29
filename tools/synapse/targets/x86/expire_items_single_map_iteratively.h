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

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
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
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "expire_items_single_map_iteratively") {
      return new_eps;
    }

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
