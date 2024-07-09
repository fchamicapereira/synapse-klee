#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class VectorWrite : public x86Module {
private:
  addr_t vector_addr;
  klee::ref<klee::Expr> index;
  addr_t value_addr;
  std::vector<modification_t> modifications;

public:
  VectorWrite(const bdd::Node *node, addr_t _vector_addr,
              klee::ref<klee::Expr> _index, addr_t _value_addr,
              const std::vector<modification_t> &_modifications)
      : x86Module(ModuleType::x86_VectorWrite, "VectorWrite", node),
        vector_addr(_vector_addr), index(_index), value_addr(_value_addr),
        modifications(_modifications) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new VectorWrite(node, vector_addr, index, value_addr, modifications);
    return cloned;
  }

  addr_t get_vector_addr() const { return vector_addr; }
  klee::ref<klee::Expr> get_index() const { return index; }
  addr_t get_value_addr() const { return value_addr; }

  const std::vector<modification_t> &get_modifications() const {
    return modifications;
  }
};

class VectorWriteGenerator : public x86ModuleGenerator {
public:
  VectorWriteGenerator()
      : x86ModuleGenerator(ModuleType::x86_VectorWrite, "VectorWrite") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_return") {
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
    klee::ref<klee::Expr> value_addr_expr = call.args.at("value").expr;
    klee::ref<klee::Expr> value = call.args.at("value").in;

    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);
    addr_t value_addr = kutil::expr_addr_to_obj_addr(value_addr_expr);

    klee::ref<klee::Expr> original_value =
        get_original_vector_value(ep, node, vector_addr);
    std::vector<modification_t> changes =
        build_modifications(original_value, value);

    // Check the Ignore module.
    if (changes.empty()) {
      return products;
    }

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    Module *module =
        new VectorWrite(node, vector_addr, index, value_addr, changes);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, vector_addr, PlacementDecision::x86_Vector);

    return products;
  }
};

} // namespace x86
} // namespace synapse
