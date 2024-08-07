#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class SketchTouchBuckets : public TofinoCPUModule {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> time;
  symbol_t success;

public:
  SketchTouchBuckets(const bdd::Node *node, addr_t _sketch_addr,
                     klee::ref<klee::Expr> _time, symbol_t _success)
      : TofinoCPUModule(ModuleType::TofinoCPU_SketchTouchBuckets,
                        "SketchTouchBuckets", node),
        sketch_addr(_sketch_addr), time(_time), success(_success) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SketchTouchBuckets(node, sketch_addr, time, success);
    return cloned;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
  const symbol_t &get_success() const { return success; }
};

class SketchTouchBucketsGenerator : public TofinoCPUModuleGenerator {
public:
  SketchTouchBucketsGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_SketchTouchBuckets,
                                 "SketchTouchBuckets") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "sketch_touch_buckets") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "sketch",
                   PlacementDecision::TofinoCPU_Sketch)) {
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

    if (call.function_name != "sketch_touch_buckets") {
      return products;
    }

    if (!can_place(ep, call_node, "sketch",
                   PlacementDecision::TofinoCPU_Sketch)) {
      return products;
    }

    klee::ref<klee::Expr> sketch_addr_expr = call.args.at("sketch").expr;
    klee::ref<klee::Expr> time = call.args.at("time").expr;

    addr_t sketch_addr = kutil::expr_addr_to_obj_addr(sketch_addr_expr);

    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t success;
    bool found = get_symbol(symbols, "success", success);
    assert(found && "Symbol success not found");

    Module *module = new SketchTouchBuckets(node, sketch_addr, time, success);
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
