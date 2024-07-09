#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class VectorRead : public x86Module {
private:
  addr_t vector_addr;
  klee::ref<klee::Expr> index;
  addr_t value_addr;
  klee::ref<klee::Expr> value;

public:
  VectorRead(const bdd::Node *node, addr_t _vector_addr,
             klee::ref<klee::Expr> _index, addr_t _value_addr,
             klee::ref<klee::Expr> _value)
      : x86Module(ModuleType::x86_VectorRead, "VectorRead", node),
        vector_addr(_vector_addr), index(_index), value_addr(_value_addr),
        value(_value) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new VectorRead(node, vector_addr, index, value_addr, value);
    return cloned;
  }

  addr_t get_vector_addr() const { return vector_addr; }
  klee::ref<klee::Expr> get_index() const { return index; }
  addr_t get_value_addr() const { return value_addr; }
  klee::ref<klee::Expr> get_value() const { return value; }
};

class VectorReadGenerator : public x86ModuleGenerator {
public:
  VectorReadGenerator()
      : x86ModuleGenerator(ModuleType::x86_VectorRead, "VectorRead") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_borrow") {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (!bdd_node_match_pattern(node)) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place(ep, call_node, "vector", PlacementDecision::x86_Vector)) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (!bdd_node_match_pattern(node)) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (!can_place(ep, call_node, "vector", PlacementDecision::x86_Vector)) {
      return products;
    }

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value_addr_expr = call.args.at("val_out").out;
    klee::ref<klee::Expr> value = call.extra_vars.at("borrowed_cell").second;

    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);
    addr_t value_addr = kutil::expr_addr_to_obj_addr(value_addr_expr);

    Module *module =
        new VectorRead(node, vector_addr, index, value_addr, value);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, vector_addr, PlacementDecision::x86_Vector);

    return products;
  }
};

} // namespace x86
} // namespace synapse
