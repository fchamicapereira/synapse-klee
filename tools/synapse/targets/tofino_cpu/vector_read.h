#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class VectorRead : public TofinoCPUModule {
private:
  addr_t vector_addr;
  klee::ref<klee::Expr> index;
  addr_t value_addr;
  klee::ref<klee::Expr> value;

public:
  VectorRead(const bdd::Node *node, addr_t _vector_addr,
             klee::ref<klee::Expr> _index, addr_t _value_addr,
             klee::ref<klee::Expr> _value)
      : TofinoCPUModule(ModuleType::TofinoCPU_VectorRead, "VectorRead", node),
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

class VectorReadGenerator : public TofinoCPUModuleGenerator {
public:
  VectorReadGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_VectorRead,
                                 "VectorRead") {}

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

    if (call.function_name != "vector_borrow") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::TofinoCPU_Vector)) {
      return std::nullopt;
    }

    return current_speculative_ctx;
  }

  virtual std::vector<generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_borrow") {
      return products;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::TofinoCPU_Vector)) {
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

    place(new_ep, vector_addr, PlacementDecision::TofinoCPU_Vector);

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
