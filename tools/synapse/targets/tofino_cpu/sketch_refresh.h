#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class SketchRefresh : public TofinoCPUModule {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> time;

public:
  SketchRefresh(const bdd::Node *node, addr_t _sketch_addr,
                klee::ref<klee::Expr> _time)
      : TofinoCPUModule(ModuleType::TofinoCPU_SketchRefresh, "SketchRefresh",
                        node),
        sketch_addr(_sketch_addr), time(_time) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SketchRefresh(node, sketch_addr, time);
    return cloned;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
};

class SketchRefreshGenerator : public TofinoCPUModuleGenerator {
public:
  SketchRefreshGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_SketchRefresh,
                                 "SketchRefresh") {}

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

    if (call.function_name != "sketch_refresh") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "sketch",
                   PlacementDecision::TofinoCPU_Sketch)) {
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

    if (call.function_name != "sketch_refresh") {
      return products;
    }

    if (!can_place(ep, call_node, "sketch",
                   PlacementDecision::TofinoCPU_Sketch)) {
      return products;
    }

    klee::ref<klee::Expr> sketch_addr_expr = call.args.at("sketch").expr;
    klee::ref<klee::Expr> time = call.args.at("time").expr;

    addr_t sketch_addr = kutil::expr_addr_to_obj_addr(sketch_addr_expr);

    Module *module = new SketchRefresh(node, sketch_addr, time);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, sketch_addr, PlacementDecision::TofinoCPU_Sketch);

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
